package org.amnezia.vpn

import android.annotation.SuppressLint
import android.app.ActivityManager
import android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE
import android.app.NotificationManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_MANIFEST
import android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_SYSTEM_EXEMPTED
import android.net.VpnService
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.os.Messenger
import android.os.PowerManager
import android.os.Process
import android.os.SystemClock
import androidx.annotation.MainThread
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import androidx.core.content.getSystemService
import java.net.HttpURLConnection
import java.net.URL
import java.net.UnknownHostException
import java.util.concurrent.ConcurrentHashMap
import kotlin.LazyThreadSafetyMode.NONE
import kotlinx.coroutines.CoroutineExceptionHandler
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.cancel
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.withContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.amnezia.vpn.protocol.BadConfigException
import org.amnezia.vpn.protocol.ProtocolState.CONNECTED
import org.amnezia.vpn.protocol.ProtocolState.CONNECTING
import org.amnezia.vpn.protocol.ProtocolState.DISCONNECTED
import org.amnezia.vpn.protocol.ProtocolState.DISCONNECTING
import org.amnezia.vpn.protocol.ProtocolState.RECONNECTING
import org.amnezia.vpn.protocol.ProtocolState.UNKNOWN
import org.amnezia.vpn.protocol.VpnException
import org.amnezia.vpn.protocol.VpnStartException
import org.amnezia.vpn.protocol.Statistics
import org.amnezia.vpn.protocol.putStatistics
import org.amnezia.vpn.protocol.putStatus
import org.amnezia.vpn.util.LoadLibraryException
import org.amnezia.vpn.util.Log
import org.amnezia.vpn.util.Prefs
import org.amnezia.vpn.util.net.NetworkState
import org.amnezia.vpn.util.net.TrafficStats
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject

private const val TAG = "AmneziaVpnService"

const val ACTION_DISCONNECT = "org.amnezia.vpn.action.disconnect"
const val ACTION_CONNECT = "org.amnezia.vpn.action.connect"

const val MSG_VPN_CONFIG = "VPN_CONFIG"
const val MSG_ERROR = "ERROR"
const val MSG_SAVE_LOGS = "SAVE_LOGS"
const val MSG_CLIENT_NAME = "CLIENT_NAME"

const val AFTER_PERMISSION_CHECK = "AFTER_PERMISSION_CHECK"
private const val PREFS_CONFIG_KEY = "LAST_CONF"
private const val PREFS_SERVER_NAME = "LAST_SERVER_NAME"
private const val PREFS_SERVER_INDEX = "LAST_SERVER_INDEX"
// §5.7 NvoVPN failover: живость туннеля и переключение awg → VLESS(direct) → VLESS(CDN) живут в СЛУЖБЕ —
// Qt/C++ на Android в фоне спит и обрыв при свёрнутом приложении не ловит. Кандидаты (готовые vpnConfig-JSON)
// и параметры живости приходят от C++ в connect-JSON ключами nvo_candidates / nvo_liveness (см. AndroidController::start).
// Детектор: дельты UID-счётчиков (трафик к серверу, протокол-независимо; xray свою статистику не отдаёт)
// по выверенному критерию «шлём, ответа нет» + активный проб GET /api/v1/ping ЧЕРЕЗ туннель как подтверждение,
// чтобы не рвать живой awg в простое. Прежние v1 (C++) и v2 (здесь, сравнивал "amneziawg" с "awg") — снесены.
private const val NVO_CANDIDATES_KEY = "nvo_candidates"
private const val NVO_LIVENESS_KEY = "nvo_liveness"
const val MSG_PATH = "PATH"
const val MSG_SERVER_ID = "SERVER_ID"
private const val LIVENESS_ALIVE_RX = 1024L               // rx за интервал ≥ → жив
private const val LIVENESS_DEAD_TX = 4096L                // tx за интервал > без ответа → «шлём, ответа нет»
private const val LIVENESS_DEAD_SECONDS_SCREEN_ON = 10    // секунд подряд (экран вкл, интервал 1с)
private const val LIVENESS_DEAD_SECONDS_SCREEN_OFF = 45   // секунд подряд (экран выкл, интервал 15с)
private const val LIVENESS_INTERVAL_ON_MS = 1000L
private const val LIVENESS_INTERVAL_OFF_MS = 15000L
private const val LIVENESS_IDLE_PROBE_MS = 30000L         // плановый активный проб не реже чем раз в 30с (по настенным часам)
private const val LIVENESS_PROBE_TIMEOUT_MS = 5000
private const val LIVENESS_START_DELAY_MS = 3000L
private const val SWITCH_CONNECT_TIMEOUT_MS = 12000L
private const val SWITCH_PROBE_DELAY_MS = 2500L
private const val SWITCH_MAX_CYCLES = 3                   // полных перебора кандидатов за сессию
private const val TELEMETRY_QUEUE_MAX = 50

private data class NvoCandidate(val path: String, val config: String)

private data class NvoLiveness(
    val pingUrls: List<String>,
    val apiBase: String,
    val token: String,
    val serverId: Int,
    var path: String
)
private const val STATISTICS_SENDING_TIMEOUT = 1000L
private const val TRAFFIC_STATS_UPDATE_TIMEOUT = 1000L
private const val DISCONNECT_TIMEOUT = 5000L
private const val STOP_SERVICE_TIMEOUT = 5000L

@SuppressLint("Registered")
open class AmneziaVpnService : VpnService() {

    private lateinit var mainScope: CoroutineScope
    private lateinit var connectionScope: CoroutineScope
    private var isServiceBound = false
    private var vpnProto: VpnProto? = null
    private var protocolState = MutableStateFlow(UNKNOWN)
    private var serverName: String? = null
    private var serverIndex: Int = -1

    private val isConnected
        get() = protocolState.value == CONNECTED

    private val isDisconnected
        get() = protocolState.value == DISCONNECTED

    private val isUnknown
        get() = protocolState.value == UNKNOWN

    private var connectionJob: Job? = null
    private var disconnectionJob: Job? = null
    private var trafficStatsUpdateJob: Job? = null
    private var statisticsSendingJob: Job? = null
    // §5.7 failover state (см. константы NVO_* выше)
    private var nvoCandidates: List<NvoCandidate> = emptyList()
    private var nvoLiveness: NvoLiveness? = null
    private var livenessJob: Job? = null
    private var switchJob: Job? = null
    private var switching = false
    private var switchCycles = 0
    private var currentPath: String? = null
    private var connectStartedMs = 0L
    private var connectedAtMs = 0L
    private val telemetryQueue = ArrayDeque<JSONObject>()
    private val appVersion: String by lazy(NONE) {
        try { packageManager.getPackageInfo(packageName, 0).versionName ?: "" } catch (e: Exception) { "" }
    }
    private lateinit var networkState: NetworkState
    private lateinit var trafficStats: TrafficStats
    private var controlReceiver: BroadcastReceiver? = null
    private var notificationStateReceiver: BroadcastReceiver? = null
    private var screenOnReceiver: BroadcastReceiver? = null
    private var screenOffReceiver: BroadcastReceiver? = null
    private val clientMessengers = ConcurrentHashMap<Messenger, IpcMessenger>()

    private val isActivityConnected
        get() = clientMessengers.any { it.value.name == ACTIVITY_MESSENGER_NAME }

    private val connectionExceptionHandler = CoroutineExceptionHandler { _, e ->
        connectionJob?.cancel()
        connectionJob = null
        disconnectionJob?.cancel()
        disconnectionJob = null
        protocolState.value = DISCONNECTED
        when (e) {
            is IllegalArgumentException,
            is VpnStartException,
            is VpnException -> onError(e.message ?: e.toString())

            is JSONException,
            is BadConfigException -> onError("VPN config format error: ${e.message}")

            is LoadLibraryException -> onError("${e.message}. Caused: ${e.cause?.message}")

            is UnknownHostException -> onError("Unknown host")

            else -> throw e
        }
    }

    private val actionMessageHandler: Handler by lazy(NONE) {
        object : Handler(Looper.getMainLooper()) {
            override fun handleMessage(msg: Message) {
                val action = msg.extractIpcMessage<Action>()
                Log.d(TAG, "Handle action: $action")
                when (action) {
                    Action.REGISTER_CLIENT -> {
                        val clientName = msg.data.getString(MSG_CLIENT_NAME)
                        val messenger = IpcMessenger(msg.replyTo, clientName)
                        clientMessengers[msg.replyTo] = messenger
                        Log.d(TAG, "Messenger client '$clientName' was registered")
                        if (clientName == ACTIVITY_MESSENGER_NAME && isConnected) launchSendingStatistics()
                    }

                    Action.UNREGISTER_CLIENT -> {
                        clientMessengers.remove(msg.replyTo)?.let {
                            Log.d(TAG, "Messenger client '${it.name}' was unregistered")
                            if (it.name == ACTIVITY_MESSENGER_NAME) stopSendingStatistics()
                        }
                    }

                    Action.CONNECT -> {
                        cancelSwitch("user connect")
                        connect(msg.data.getString(MSG_VPN_CONFIG))
                    }

                    Action.DISCONNECT -> {
                        cancelSwitch("user disconnect")
                        disconnect()
                    }

                    Action.REQUEST_STATUS -> {
                        clientMessengers[msg.replyTo]?.let { clientMessenger ->
                            clientMessenger.send {
                                ServiceEvent.STATUS.packToMessage {
                                    putStatus(this@AmneziaVpnService.protocolState.value)
                                }
                            }
                        }
                    }

                    Action.NOTIFICATION_PERMISSION_GRANTED -> {
                        enableNotification()
                    }

                    Action.SET_SAVE_LOGS -> {
                        Log.saveLogs = msg.data.getBoolean(MSG_SAVE_LOGS)
                    }
                }
            }
        }
    }

    private val vpnServiceMessenger: Messenger by lazy(NONE) {
        Messenger(actionMessageHandler)
    }

    /**
     * Notification setup
     */
    private val foregroundServiceTypeCompat
        get() = when {
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE -> FOREGROUND_SERVICE_TYPE_SYSTEM_EXEMPTED
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q -> FOREGROUND_SERVICE_TYPE_MANIFEST
            else -> 0
        }

    private val serviceNotification: ServiceNotification by lazy(NONE) { ServiceNotification(this) }

    /**
     * Service overloaded methods
     */
    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Create Amnezia VPN service")
        mainScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
        connectionScope = CoroutineScope(SupervisorJob() + Dispatchers.IO + connectionExceptionHandler)
        loadServerData()
        launchProtocolStateHandler()
        networkState = NetworkState(this, ::reconnect)
        trafficStats = TrafficStats()
        registerBroadcastReceivers()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val isAlwaysOn = intent != null && intent.action == SERVICE_INTERFACE

        if (isAlwaysOn) {
            Log.d(TAG, "Start service via Always-on")
            connect()
        } else if (intent?.getBooleanExtra(AFTER_PERMISSION_CHECK, false) == true) {
            Log.d(TAG, "Start service after permission check")
            connect()
        } else {
            Log.d(TAG, "Start service")
            connect(intent?.getStringExtra(MSG_VPN_CONFIG))
        }
        ServiceCompat.startForeground(
            this, NOTIFICATION_ID,
            serviceNotification.buildNotification(serverName, vpnProto?.label, protocolState.value),
            foregroundServiceTypeCompat
        )
        return START_REDELIVER_INTENT
    }

    override fun onBind(intent: Intent?): IBinder? {
        Log.d(TAG, "onBind by $intent")
        if (intent?.action == SERVICE_INTERFACE) return super.onBind(intent)
        isServiceBound = true
        return vpnServiceMessenger.binder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        Log.d(TAG, "onUnbind by $intent")
        if (intent?.action != SERVICE_INTERFACE) {
            if (clientMessengers.isEmpty()) {
                isServiceBound = false
                if (isUnknown || isDisconnected) stopService()
            }
        }
        return true
    }

    override fun onRebind(intent: Intent?) {
        Log.d(TAG, "onRebind by $intent")
        if (intent?.action != SERVICE_INTERFACE) {
            isServiceBound = true
        }
        super.onRebind(intent)
    }

    override fun onRevoke() {
        Log.d(TAG, "onRevoke")
        // Calls to onRevoke() method may not happen on the main thread of the process
        mainScope.launch {
            disconnect()
        }
    }

    override fun onDestroy() {
        Log.d(TAG, "Destroy service")
        unregisterBroadcastReceivers()
        runBlocking {
            disconnect()
            disconnectionJob?.join()
        }
        connectionScope.cancel()
        mainScope.cancel()
        super.onDestroy()
    }

    private fun stopService() {
        Log.d(TAG, "Stop service")
        // the coroutine below will be canceled during the onDestroy call
        mainScope.launch {
            delay(STOP_SERVICE_TIMEOUT)
            Log.w(TAG, "Stop service timeout, kill process")
            Process.killProcess(Process.myPid())
        }
        stopSelf()
    }

    private fun registerBroadcastReceivers() {
        Log.d(TAG, "Register broadcast receivers")
        controlReceiver = registerBroadcastReceiver(
            arrayOf(ACTION_CONNECT, ACTION_DISCONNECT), ContextCompat.RECEIVER_NOT_EXPORTED
        ) {
            it?.action?.let { action ->
                Log.v(TAG, "Broadcast request received: $action")
                when (action) {
                    ACTION_CONNECT -> connect()
                    ACTION_DISCONNECT -> disconnect()
                    else -> Log.w(TAG, "Unknown action received: $action")
                }
            }
        }

        notificationStateReceiver = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            registerBroadcastReceiver(
                arrayOf(
                    NotificationManager.ACTION_NOTIFICATION_CHANNEL_BLOCK_STATE_CHANGED,
                    NotificationManager.ACTION_APP_BLOCK_STATE_CHANGED
                )
            ) {
                val state = it?.getBooleanExtra(NotificationManager.EXTRA_BLOCKED_STATE, false)
                Log.v(TAG, "Notification state changed: ${it?.action}, blocked = $state")
                if (state == false) {
                    enableNotification()
                } else {
                    disableNotification()
                }
            }
        } else null

        registerScreenStateBroadcastReceivers()
    }

    private fun registerScreenStateBroadcastReceivers() {
        if (serviceNotification.isNotificationEnabled()) {
            Log.d(TAG, "Register screen state broadcast receivers")
            screenOnReceiver = registerBroadcastReceiver(Intent.ACTION_SCREEN_ON) {
                if (isConnected && serviceNotification.isNotificationEnabled()) startTrafficStatsUpdateJob()
            }

            screenOffReceiver = registerBroadcastReceiver(Intent.ACTION_SCREEN_OFF) {
                stopTrafficStatsUpdateJob()
            }
        }
    }

    private fun unregisterScreenStateBroadcastReceivers() {
        Log.d(TAG, "Unregister screen state broadcast receivers")
        unregisterBroadcastReceiver(screenOnReceiver)
        unregisterBroadcastReceiver(screenOffReceiver)
        screenOnReceiver = null
        screenOffReceiver = null
    }

    private fun unregisterBroadcastReceivers() {
        Log.d(TAG, "Unregister broadcast receivers")
        unregisterBroadcastReceiver(controlReceiver)
        unregisterBroadcastReceiver(notificationStateReceiver)
        unregisterScreenStateBroadcastReceivers()
        controlReceiver = null
        notificationStateReceiver = null
    }

    /**
     * Methods responsible for processing VPN connection
     */
    private fun launchProtocolStateHandler() {
        mainScope.launch {
            // drop first default UNKNOWN state
            protocolState.drop(1).collect { protocolState ->
                Log.d(TAG, "Protocol state changed: $protocolState")

                serviceNotification.updateNotification(serverName, vpnProto?.label, protocolState)

                clientMessengers.send {
                    ServiceEvent.STATUS_CHANGED.packToMessage {
                        putStatus(protocolState)
                    }
                }

                VpnStateStore.store { VpnState(protocolState, serverName, serverIndex, vpnProto) }

                when (protocolState) {
                    CONNECTED -> {
                        networkState.bindNetworkListener()
                        if (isActivityConnected) launchSendingStatistics()
                        launchTrafficStatsUpdate()
                        onTunnelConnected()
                    }

                    DISCONNECTED -> {
                        networkState.unbindNetworkListener()
                        stopTrafficStatsUpdateJob()
                        stopSendingStatistics()
                        stopLivenessJob()
                        if (!isServiceBound && !switching) stopService()
                    }

                    DISCONNECTING -> {
                        networkState.unbindNetworkListener()
                        stopTrafficStatsUpdateJob()
                        stopSendingStatistics()
                        stopLivenessJob()
                    }

                    RECONNECTING -> {
                        stopTrafficStatsUpdateJob()
                        stopSendingStatistics()
                        stopLivenessJob()
                    }

                    CONNECTING, UNKNOWN -> {}
                }
            }
        }
    }

    @MainThread
    private fun launchSendingStatistics() {
        if (isServiceBound && isConnected) {
            statisticsSendingJob = mainScope.launch {
                while (true) {
                    clientMessengers.send {
                        ServiceEvent.STATISTICS_UPDATE.packToMessage {
                            putStatistics(vpnProto?.protocol?.statistics ?: Statistics.EMPTY_STATISTICS)
                        }
                    }
                    delay(STATISTICS_SENDING_TIMEOUT)
                }
            }
        }
    }

    @MainThread
    private fun stopSendingStatistics() {
        statisticsSendingJob?.cancel()
    }

    @MainThread
    private fun enableNotification() {
        registerScreenStateBroadcastReceivers()
        serviceNotification.updateNotification(serverName, vpnProto?.label, protocolState.value)
        launchTrafficStatsUpdate()
    }

    @MainThread
    private fun disableNotification() {
        unregisterScreenStateBroadcastReceivers()
        stopTrafficStatsUpdateJob()
    }

    @MainThread
    private fun launchTrafficStatsUpdate() {
        stopTrafficStatsUpdateJob()
        if (isConnected &&
            serviceNotification.isNotificationEnabled() &&
            getSystemService<PowerManager>()?.isInteractive != false
        ) {
            Log.v(TAG, "Launch traffic stats update")
            trafficStats.reset()
            startTrafficStatsUpdateJob()
        }
    }

    @MainThread
    private fun startTrafficStatsUpdateJob() {
        if (trafficStatsUpdateJob == null && trafficStats.isSupported()) {
            Log.d(TAG, "Start traffic stats update")
            trafficStatsUpdateJob = mainScope.launch {
                while (true) {
                    trafficStats.getSpeed().let { speed ->
                        if (isConnected) {
                            serviceNotification.updateSpeed(speed)
                        }
                    }
                    delay(TRAFFIC_STATS_UPDATE_TIMEOUT)
                }
            }
        }
    }

    /**
     * §5.7: failover внутри службы. Данные (кандидаты/параметры) — из connect-JSON, см. parseNvoExtras().
     */
    private fun parseNvoExtras(config: JSONObject) {
        val cands: JSONArray? = config.optJSONArray(NVO_CANDIDATES_KEY)
        val live: JSONObject? = config.optJSONObject(NVO_LIVENESS_KEY)
        // Конфиг без §5.7-данных (кандидат при свитче, старый C++) — состояние не трогаем.
        if (cands == null && live == null) return
        nvoCandidates = buildList {
            if (cands != null) for (i in 0 until cands.length()) {
                val o = cands.optJSONObject(i) ?: continue
                val path = o.optString("path"); val cfg = o.optString("config")
                if (path.isNotEmpty() && cfg.isNotEmpty()) add(NvoCandidate(path, cfg))
            }
        }
        nvoLiveness = live?.let { l ->
            NvoLiveness(
                pingUrls = buildList {
                    val a = l.optJSONArray("ping_urls")
                    if (a != null) for (i in 0 until a.length()) a.optString(i).takeIf { it.isNotEmpty() }?.let { add(it) }
                },
                apiBase = l.optString("api_base"),
                token = l.optString("token"),
                serverId = l.optInt("server_id", -1),
                path = l.optString("path", "awg")
            )
        }
        currentPath = nvoLiveness?.path
        switchCycles = 0
        Log.d(TAG, "nvo extras: path=$currentPath candidates=${nvoCandidates.map { it.path }} " +
            "ping=${nvoLiveness?.pingUrls?.size ?: 0} server=${nvoLiveness?.serverId}")
    }

    @MainThread
    private fun onTunnelConnected() {
        connectedAtMs = SystemClock.elapsedRealtime()
        if (switching) return   // свитчер сам дождётся CONNECTED, проверит пробом и перезапустит живость
        val live = nvoLiveness
        if (live != null) {
            val ms = if (connectStartedMs > 0) (connectedAtMs - connectStartedMs).toInt() else 0
            queueTelemetry(event("tunnel_up") { put("proto", currentPath); put("server_id", live.serverId); put("ms", ms) })
        }
        startLivenessJob()
        flushTelemetry()
    }

    @MainThread
    private fun startLivenessJob() {
        stopLivenessJob()
        val live = nvoLiveness ?: return
        if (live.pingUrls.isEmpty()) return
        val uid = Process.myUid()
        Log.d(TAG, "liveness: start (path=$currentPath)")
        livenessJob = connectionScope.launch {
            var lastRx = android.net.TrafficStats.getUidRxBytes(uid)
            var lastTx = android.net.TrafficStats.getUidTxBytes(uid)
            val counters = lastRx >= 0 && lastTx >= 0
            if (!counters) Log.w(TAG, "liveness: UID traffic counters unsupported — probe-only mode")
            var deadSeconds = 0
            // Активный проб — по НАСТЕННЫМ часам: последний проб был >LIVENESS_IDLE_PROBE_MS назад.
            // (Раньше таймер сбрасывался любым tx-всплеском → тихо-мёртвый туннель, который клиент
            //  безуспешно долбит малым трафиком, мог не пробиться вовсе. Теперь всплески его не сбивают.)
            var lastProbeMs = SystemClock.elapsedRealtime()
            delay(LIVENESS_START_DELAY_MS)
            while (isActive) {
                val screenOn = getSystemService<PowerManager>()?.isInteractive != false
                val intervalMs = if (screenOn) LIVENESS_INTERVAL_ON_MS else LIVENESS_INTERVAL_OFF_MS
                delay(intervalMs)
                if (!isConnected || switching) continue
                var suspect = false
                if (counters) {
                    val rx = android.net.TrafficStats.getUidRxBytes(uid)
                    val tx = android.net.TrafficStats.getUidTxBytes(uid)
                    val dRx = rx - lastRx; val dTx = tx - lastTx
                    lastRx = rx; lastTx = tx
                    // Критерий выверен на macOS/iOS/Android v1: у мёртвого туннеля rx капает крохами
                    // (ретрансмиты), поэтому смотрим на СООТНОШЕНИЕ rx/tx, а не на факт роста rx.
                    if (dRx >= LIVENESS_ALIVE_RX && dRx * 10 >= dTx) {
                        deadSeconds = 0
                        lastProbeMs = SystemClock.elapsedRealtime()   // видим живой трафик — сдвигаем плановый проб
                    } else if (dTx > LIVENESS_DEAD_TX) {
                        deadSeconds += (intervalMs / 1000).toInt()     // шлём, ответа нет
                    }
                    val limit = if (screenOn) LIVENESS_DEAD_SECONDS_SCREEN_ON else LIVENESS_DEAD_SECONDS_SCREEN_OFF
                    suspect = deadSeconds >= limit
                }
                // Плановый проб (настенные часы): ловит и «тихую смерть» без исходящего трафика.
                val dueProbe = SystemClock.elapsedRealtime() - lastProbeMs >= LIVENESS_IDLE_PROBE_MS
                if (!suspect && !dueProbe) continue
                // Подтверждение активным пробом ЧЕРЕЗ туннель (сокет службы не protect()-ится → идёт в tun).
                lastProbeMs = SystemClock.elapsedRealtime()
                if (probeTunnel(live.pingUrls)) { deadSeconds = 0; continue }
                if (!isConnected || switching) continue
                Log.w(TAG, "liveness: tunnel dead (path=$currentPath, ${if (suspect) "rx_stall" else "probe_fail"})")
                withContext(Dispatchers.Main.immediate) { onTunnelDead(if (suspect) "rx_stall" else "probe_fail") }
                return@launch
            }
        }
    }

    @MainThread
    private fun stopLivenessJob() {
        livenessJob?.cancel()
        livenessJob = null
    }

    // GET ping-URL через туннель: 2xx/204 — жив. Два URL (разные домены — на случай блока одного).
    private fun probeTunnel(urls: List<String>): Boolean {
        for (u in urls) {
            try {
                val conn = URL(u).openConnection() as HttpURLConnection
                conn.connectTimeout = LIVENESS_PROBE_TIMEOUT_MS
                conn.readTimeout = LIVENESS_PROBE_TIMEOUT_MS
                conn.instanceFollowRedirects = false
                conn.useCaches = false
                conn.setRequestProperty("Cache-Control", "no-cache")
                val code = conn.responseCode
                conn.disconnect()
                if (code in 200..299) return true
                Log.w(TAG, "probe $u -> HTTP $code")
            } catch (e: Exception) {
                Log.w(TAG, "probe $u failed: ${e.javaClass.simpleName}: ${e.message}")
            }
        }
        return false
    }

    @MainThread
    private fun onTunnelDead(reason: String) {
        val live = nvoLiveness ?: return
        val aliveS = ((SystemClock.elapsedRealtime() - connectedAtMs) / 1000).toInt()
        queueTelemetry(event("tunnel_dead") {
            put("proto", currentPath); put("server_id", live.serverId); put("alive_s", aliveS); put("reason", reason)
        })
        val next = nvoCandidates.filter { it.path != currentPath }
        if (next.isEmpty() || switchCycles >= SWITCH_MAX_CYCLES) {
            Log.w(TAG, "liveness: no candidates to switch (cycles=$switchCycles)")
            onError("Соединение потеряно, резервных каналов нет")
            return
        }
        switchJob = mainScope.launch { switchTo(next) }
    }

    @MainThread
    private fun cancelSwitch(why: String) {
        if (switchJob != null || switching) Log.d(TAG, "switch cancelled: $why")
        switchJob?.cancel()
        switchJob = null
        switching = false
        stopLivenessJob()
    }

    // Последовательный перерейс по кандидатам: рвём мёртвый туннель → поднимаем следующий → подтверждаем пробом.
    // Всё внутри ОДНОЙ службы/процесса (xray грузится в awg-процесс) — VpnService не пересоздаётся.
    @MainThread
    private suspend fun switchTo(cands: List<NvoCandidate>) {
        if (switching) return
        switching = true
        switchCycles++
        stopLivenessJob()
        clientMessengers.send { ServiceEvent.PROTO_SWITCHING.packToMessage { putString(MSG_PATH, currentPath ?: "") } }
        val live = nvoLiveness
        try {
            for (c in cands) {
                val started = SystemClock.elapsedRealtime()
                Log.i(TAG, "switch: $currentPath -> ${c.path}")
                disconnectForSwitch()
                if (!isDisconnected) {
                    Log.w(TAG, "switch: tunnel did not stop, abort")
                    break
                }
                connect(c.config)  // сохранится как LAST_CONF: после рестарта поднимется этот же путь
                val up = try {
                    withTimeout(SWITCH_CONNECT_TIMEOUT_MS) { protocolState.first { it == CONNECTED || it == DISCONNECTED } }
                    isConnected
                } catch (e: TimeoutCancellationException) {
                    false
                }
                val ok = up && withContext(Dispatchers.IO) {
                    delay(SWITCH_PROBE_DELAY_MS)
                    probeTunnel(live?.pingUrls ?: emptyList())
                }
                val ms = (SystemClock.elapsedRealtime() - started).toInt()
                queueTelemetry(event("switch") {
                    put("proto", currentPath); put("to", c.path); put("server_id", live?.serverId ?: -1); put("ms", ms); put("ok", ok)
                })
                if (ok) {
                    currentPath = c.path
                    live?.path = c.path
                    switching = false
                    Log.i(TAG, "switch: OK -> ${c.path} in ${ms}ms")
                    clientMessengers.send {
                        ServiceEvent.PROTO_SWITCHED.packToMessage {
                            putString(MSG_PATH, c.path); putInt(MSG_SERVER_ID, live?.serverId ?: -1)
                        }
                    }
                    serviceNotification.updateNotification(serverName, vpnProto?.label, protocolState.value)
                    startLivenessJob()
                    flushTelemetry()
                    return
                }
                Log.w(TAG, "switch: ${c.path} failed (up=$up)")
            }
            queueTelemetry(event("connect_fail") {
                put("proto", currentPath); put("server_id", live?.serverId ?: -1); put("reason", "all_candidates_failed")
            })
            disconnectForSwitch()
            onError("Соединение потеряно: ни один резервный канал не поднялся. Нажмите «Подключить».")
        } finally {
            switching = false
            switchJob = null
        }
    }

    // Как disconnect(), но без stopService()/kill процесса по таймауту — посреди свитча служба должна жить.
    @MainThread
    private suspend fun disconnectForSwitch() {
        if (isUnknown || isDisconnected) return
        protocolState.value = DISCONNECTING
        connectionJob?.cancelAndJoin()
        connectionJob = null
        withContext(Dispatchers.IO) { vpnProto?.protocol?.stopVpn() }
        try {
            withTimeout(DISCONNECT_TIMEOUT) { protocolState.first { it == DISCONNECTED } }
        } catch (e: TimeoutCancellationException) {
            Log.w(TAG, "switch: disconnect timeout (service kept alive)")
        }
    }

    /**
     * Телеметрия сессий → POST /api/v1/connection/event (sanctum). Очередь: события копятся и уходят,
     * когда туннель заведомо жив (после CONNECTED/успешного свитча) — иначе POST уйдёт в мёртвый туннель.
     */
    private fun event(name: String, fill: JSONObject.() -> Unit): JSONObject =
        JSONObject().apply { put("event", name); put("app", appVersion); fill() }

    private fun queueTelemetry(ev: JSONObject) {
        synchronized(telemetryQueue) {
            if (telemetryQueue.size >= TELEMETRY_QUEUE_MAX) telemetryQueue.removeFirst()
            telemetryQueue.addLast(ev)
        }
    }

    private fun flushTelemetry() {
        val live = nvoLiveness ?: return
        if (live.token.isEmpty() || live.apiBase.isEmpty()) return
        connectionScope.launch {
            while (true) {
                val ev = synchronized(telemetryQueue) { telemetryQueue.removeFirstOrNull() } ?: break
                if (!postTelemetry(live, ev)) {
                    synchronized(telemetryQueue) { telemetryQueue.addFirst(ev) }
                    break
                }
            }
        }
    }

    private fun postTelemetry(live: NvoLiveness, ev: JSONObject): Boolean = try {
        val conn = URL(live.apiBase + "/connection/event").openConnection() as HttpURLConnection
        conn.requestMethod = "POST"
        conn.connectTimeout = 6000
        conn.readTimeout = 6000
        conn.instanceFollowRedirects = false
        conn.setRequestProperty("Authorization", "Bearer " + live.token)
        conn.setRequestProperty("Content-Type", "application/json")
        conn.setRequestProperty("Accept", "application/json")
        conn.doOutput = true
        conn.outputStream.use { it.write(ev.toString().toByteArray()) }
        val code = conn.responseCode
        conn.disconnect()
        code in 200..299
    } catch (e: Exception) {
        Log.w(TAG, "telemetry: ${e.javaClass.simpleName}: ${e.message}")
        false
    }

    @MainThread
    private fun stopTrafficStatsUpdateJob() {
        Log.d(TAG, "Stop traffic stats update")
        trafficStatsUpdateJob?.cancel()
        trafficStatsUpdateJob = null
    }

    @MainThread
    private fun connect(vpnConfig: String? = null) {
        if (vpnConfig == null) {
            connectToVpn(Prefs.load(PREFS_CONFIG_KEY))
        } else {
            Prefs.save(PREFS_CONFIG_KEY, vpnConfig)
            connectToVpn(vpnConfig)
        }
    }

    @MainThread
    private fun connectToVpn(vpnConfig: String) {
        if (isConnected || protocolState.value == CONNECTING) return

        Log.d(TAG, "Start VPN connection")

        val config = parseConfigToJson(vpnConfig)
        saveServerData(config)
        if (config == null) {
            onError("Invalid VPN config")
            protocolState.value = DISCONNECTED
            return
        }

        try {
            vpnProto = VpnProto.get(config.getString("protocol"))
        } catch (e: Exception) {
            onError("Invalid VPN config: ${e.message}")
            protocolState.value = DISCONNECTED
            return
        }

        // §5.7: кандидаты failover + параметры живости из connect-JSON (если есть — это новый коннект от C++).
        parseNvoExtras(config)
        connectStartedMs = SystemClock.elapsedRealtime()

        protocolState.value = CONNECTING

        if (!checkPermission()) {
            protocolState.value = DISCONNECTED
            return
        }

        connectionJob = connectionScope.launch {
            disconnectionJob?.join()
            disconnectionJob = null

            vpnProto?.protocol?.let { protocol ->
                protocol.initialize(applicationContext, protocolState, ::onError)
                protocol.startVpn(config, Builder(), ::protect)
            }
        }
    }

    @MainThread
    private fun disconnect() {
        if (isUnknown || isDisconnected || protocolState.value == DISCONNECTING) return

        Log.d(TAG, "Stop VPN connection")

        protocolState.value = DISCONNECTING

        disconnectionJob = connectionScope.launch {
            connectionJob?.cancelAndJoin()
            connectionJob = null

            vpnProto?.protocol?.stopVpn()

            try {
                withTimeout(DISCONNECT_TIMEOUT) {
                    // waiting for disconnect state
                    protocolState.first { it == DISCONNECTED }
                }
            } catch (e: TimeoutCancellationException) {
                Log.w(TAG, "Disconnect timeout")
                stopService()
            }
        }
    }

    @MainThread
    private fun reconnect() {
        if (!isConnected) return

        Log.d(TAG, "Reconnect VPN")

        protocolState.value = RECONNECTING

        connectionJob = connectionScope.launch {
            vpnProto?.protocol?.reconnectVpn(Builder(), ::protect)
        }
    }

    /**
     * Utils methods
     */
    private fun onError(msg: String) {
        Log.e(TAG, msg)
        mainScope.launch {
            clientMessengers.send {
                ServiceEvent.ERROR.packToMessage {
                    putString(MSG_ERROR, msg)
                }
            }
        }
    }

    private fun parseConfigToJson(vpnConfig: String): JSONObject? =
        if (vpnConfig.isBlank()) {
            null
        } else {
            try {
                JSONObject(vpnConfig)
            } catch (e: JSONException) {
                onError("Invalid VPN config json format: ${e.message}")
                null
            }
        }

    private fun saveServerData(config: JSONObject?) {
        serverName = config?.opt("description") as String?
        serverIndex = config?.opt("serverIndex") as Int? ?: -1
        Log.d(TAG, "Save server data: ($serverIndex, $serverName)")
        Prefs.save(PREFS_SERVER_NAME, serverName)
        Prefs.save(PREFS_SERVER_INDEX, serverIndex)
    }

    private fun loadServerData() {
        serverName = Prefs.load<String>(PREFS_SERVER_NAME).ifBlank { null }
        if (serverName != null) serverIndex = Prefs.load(PREFS_SERVER_INDEX)
        Log.d(TAG, "Load server data: ($serverIndex, $serverName)")
    }

    private fun checkPermission(): Boolean =
        if (prepare(applicationContext) != null) {
            Intent(this, VpnRequestActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                putExtra(EXTRA_PROTOCOL, vpnProto)
            }.also {
                startActivity(it)
            }
            false
        } else {
            true
        }

    companion object {
        fun isRunning(context: Context, processName: String): Boolean =
            context.getSystemService<ActivityManager>()!!.runningAppProcesses.any {
                it.processName == processName && it.importance <= IMPORTANCE_FOREGROUND_SERVICE
            }
    }
}
