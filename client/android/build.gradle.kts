import com.android.build.gradle.internal.api.BaseVariantOutputImpl

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.serialization)
    id("property-delegate")
}

kotlin {
    jvmToolchain(17)
}

// get values from gradle or local properties
val qtTargetSdkVersion: String by gradleProperties
val qtTargetAbiList: String by gradleProperties
val outputBaseName: String by gradleProperties

android {
    namespace = "org.amnezia.vpn"

    buildFeatures {
        viewBinding = true
    }

    androidResources {
        // don't compress Qt binary resources file
        noCompress += "rcc"
    }

    packaging {
        // NvoVPN: поддержка 16 КБ страниц памяти (требование Google Play для targetSDK 35).
        // .so раздаём несжатыми и 16КБ-выровненными (mmap прямо из APK); сжатие ломало бы это.
        jniLibs.useLegacyPackaging = false
        // Xray не используется (NvoVPN = только AmneziaWG). Его Go-либа libgojni.so —
        // единственная 4КБ-выровненная (0x1000), из-за неё Play отклонял 16КБ. Исключаем.
        jniLibs.excludes += "**/libgojni.so"
    }

    defaultConfig {
        // NvoVPN: install-identity на устройстве (отдельное приложение, не конфликтует с
        // оригинальным AmneziaVPN org.amnezia.vpn). namespace остаётся org.amnezia.vpn —
        // от него зависят JNI-классы (android_controller.cpp) и R/BuildConfig.
        applicationId = "com.nvovpn.app"
        targetSdk = qtTargetSdkVersion.toInt()

        // keeps language resources for only the locales specified below
        resourceConfigurations += listOf("en", "ru", "b+zh+Hans")
        ndk.abiFilters += qtTargetAbiList.split(",")
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("AndroidManifest.xml")
            java.setSrcDirs(listOf("src"))
            res.setSrcDirs(listOf("res"))
            // androyddeployqt creates the folders below
            assets.setSrcDirs(listOf("assets"))
            jniLibs.setSrcDirs(listOf("libs"))
        }
    }

    buildTypes {
        release {
            // exclude coroutine debug resource from release build
            packaging {
                resources.excludes += "DebugProbesKt.bin"
            }
        }
    }

    lint {
        disable += "InvalidFragmentVersionForActivityResult"
    }
}

dependencies {
    implementation(project(":qt"))
    implementation(project(":utils"))
    implementation(project(":protocolApi"))
    implementation(project(":wireguard"))
    implementation(project(":awg"))
    implementation(project(":openvpn"))
    implementation(project(":xray"))
    implementation(libs.androidx.core)
    implementation(libs.androidx.activity)
    implementation(libs.androidx.fragment)
    implementation(libs.kotlinx.coroutines)
    implementation(libs.kotlinx.serialization.protobuf)
    implementation(libs.bundles.androidx.camera)
    implementation(libs.google.mlkit)
    implementation(libs.androidx.datastore)
    implementation(libs.androidx.biometric)
    // NvoVPN: Google Play In-App Review (окно оценки в приложении)
    implementation("com.google.android.play:review:2.0.2")
}
