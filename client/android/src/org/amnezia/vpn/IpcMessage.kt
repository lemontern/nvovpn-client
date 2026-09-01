package org.amnezia.vpn

import android.os.Bundle
import android.os.Message
import kotlin.enums.enumEntries

sealed interface IpcMessage {
    companion object {
        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified T> extractFromMessage(msg: Message): T
            where T : Enum<T>,
                  T : IpcMessage {
            val values = enumEntries<T>()
            if (msg.what !in values.indices) {
                throw IllegalArgumentException("IPC action or event not found for the message: $msg")
            }
            return values[msg.what]
        }
    }
}

enum class ServiceEvent : IpcMessage {
    STATUS_CHANGED,
    STATUS,
    STATISTICS_UPDATE,
    ERROR,
    // §5.7: служба сама переключает путь (awg → VLESS direct → VLESS CDN) при обрыве.
    PROTO_SWITCHING,   // начали перерейс: C++ должен молчать (не считать разрыв обрывом и не запускать свой фолбек)
    PROTO_SWITCHED     // перерейс удался: data PATH ("vless-direct"|"vless-cdn"), SERVER_ID
}

enum class Action : IpcMessage {
    REGISTER_CLIENT,
    UNREGISTER_CLIENT,
    CONNECT,
    DISCONNECT,
    REQUEST_STATUS,
    NOTIFICATION_PERMISSION_GRANTED,
    SET_SAVE_LOGS
}

fun <T> T.packToMessage(): Message
    where T : Enum<T>, T : IpcMessage = Message.obtain().also { it.what = ordinal }

fun <T> T.packToMessage(block: Bundle.() -> Unit): Message
    where T : Enum<T>, T : IpcMessage = packToMessage().also { it.data = Bundle().apply(block) }

inline fun <reified T> Message.extractIpcMessage(): T
    where T : Enum<T>, T : IpcMessage = IpcMessage.extractFromMessage<T>(this)
