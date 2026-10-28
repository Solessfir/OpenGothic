# APK packaging for the native Gothic2Notr target; included from the root project on Android.
set(OPENGOTHIC_ANDROID_GAME "launcher" CACHE STRING "launcher (all games, Steam downloads), or a single-game edition: gothic1, gothic2, gothic2notr or archolos")
set_property(CACHE OPENGOTHIC_ANDROID_GAME PROPERTY STRINGS launcher gothic1 gothic2 gothic2notr archolos)
if(OPENGOTHIC_ANDROID_GAME STREQUAL "launcher")
  set(OPENGOTHIC_APPLICATION_ID org.opengothic.launcher)
  set(OPENGOTHIC_APP_NAME "Gothic Launcher")
  set(OPENGOTHIC_ICON_COLOR "#EEE8DE")
elseif(OPENGOTHIC_ANDROID_GAME STREQUAL "gothic1" OR OPENGOTHIC_ANDROID_GAME STREQUAL "1")
  set(OPENGOTHIC_APPLICATION_ID org.opengothic.gothic1)
  set(OPENGOTHIC_APP_NAME "Gothic")
  set(OPENGOTHIC_ICON_COLOR "#F2F2F2")
elseif(OPENGOTHIC_ANDROID_GAME STREQUAL "gothic2")
  set(OPENGOTHIC_APPLICATION_ID org.opengothic.gothic2)
  set(OPENGOTHIC_APP_NAME "Gothic II Classic")
  set(OPENGOTHIC_ICON_COLOR "#D7B56D")
elseif(OPENGOTHIC_ANDROID_GAME STREQUAL "gothic2notr" OR OPENGOTHIC_ANDROID_GAME STREQUAL "2")
  set(OPENGOTHIC_APPLICATION_ID org.opengothic.gothic2notr)
  set(OPENGOTHIC_APP_NAME "Gothic II: NotR")
  set(OPENGOTHIC_ICON_COLOR "#739FD7")
elseif(OPENGOTHIC_ANDROID_GAME STREQUAL "archolos")
  set(OPENGOTHIC_APPLICATION_ID org.opengothic.archolos)
  set(OPENGOTHIC_APP_NAME "Archolos")
  set(OPENGOTHIC_ICON_COLOR "#C65A50")
else()
  message(FATAL_ERROR "OPENGOTHIC_ANDROID_GAME must be launcher, gothic1, gothic2, gothic2notr or archolos")
endif()

# Each app has its own Android identity and storage, but shares the engine and UI sources.
set(generated "${CMAKE_CURRENT_BINARY_DIR}/edition")
configure_file(app/identity.xml.in "${generated}/res/values/identity.xml" @ONLY)

if(OPENGOTHIC_ANDROID_GAME STREQUAL "launcher")
  # Raise for every release: installed launchers update to GitHub releases tagged launcher-<version>
  set(OPENGOTHIC_LAUNCHER_VERSION 0.6.6)
  string(REPLACE "." ";" parts "${OPENGOTHIC_LAUNCHER_VERSION}.0.0")
  list(GET parts 0 major)
  list(GET parts 1 minor)
  list(GET parts 2 patch)
  # Android only installs an update with a higher code; 0.2 is 200, 1.10.3 is 11003
  math(EXPR code "${major} * 10000 + ${minor} * 100 + ${patch}")

  # The engine shows the release in its main menu and log
  target_compile_definitions(OpenGothic PUBLIC OPENGOTHIC_APP_VERSION="${OPENGOTHIC_LAUNCHER_VERSION}")

  # Library of Gothic, Gothic II and Archolos side by side; Steam needs Android 8, ZIP import works everywhere
  add_android_apk(OpenGothic-apk
    PACKAGE_NAME ${OPENGOTHIC_APPLICATION_ID}
    CODE Gothic2Notr
    VERSION_CODE ${code}
    VERSION_NAME ${OPENGOTHIC_LAUNCHER_VERSION}
    PROPERTY_PREFIX opengothic
    SIGNING_ENV_PREFIX OPENGOTHIC
    MANIFEST launcher/AndroidManifest.xml
    JAVA_DIRS launcher/java app/src/main/java
    RESOURCE_DIRS app/src/main/res launcher/res "${generated}/res"
    DEPENDENCIES
      androidx.appcompat:appcompat:1.7.1
      com.google.zxing:core:3.5.3
      in.dragonbra:javasteam:1.8.0
      in.dragonbra:javasteam-depotdownloader:1.8.0
      # Crypto provider JavaSteam loads by name; the Java 8 jar, since the multi-release one clashes with okhttp on META-INF files
      org.bouncycastle:bcprov-jdk15to18:1.80
      # Depot decompression and libraries JavaSteam needs at compile time
      org.tukaani:xz:1.12
      com.github.luben:zstd-jni:1.5.7-11@aar
      org.jetbrains.kotlin:kotlin-stdlib:2.2.21
      org.jetbrains.kotlinx:kotlinx-coroutines-core:1.10.2
      com.google.protobuf:protobuf-java:4.31.1
      com.squareup.okhttp3:okhttp:5.3.2
      com.squareup.okio:okio:3.16.0
    PROGUARD_FILES launcher/proguard-rules.pro
    CMAKE_ARGUMENTS -DBUILD_SHARED_LIBS=OFF -DTEMPEST_BUILD_SHARED=OFF -DTEMPEST_BUILD_AUDIO=ON
    CPP_FLAGS -std=c++20 -fvisibility=hidden -ffunction-sections -fdata-sections
    NO_COMPRESS zip
    SHRINK_RELEASE ANDROIDX)
  return()
endif()

configure_file(app/SetupActivity.java.in "${generated}/java/org/opengothic/app/SetupActivity.java" @ONLY)
configure_file(app/shortcuts.xml.in "${generated}/res/xml/shortcuts.xml" @ONLY)

add_android_apk(OpenGothic-apk
  PACKAGE_NAME ${OPENGOTHIC_APPLICATION_ID}
  CODE Gothic2Notr
  VERSION_NAME 0.1-android
  PROPERTY_PREFIX opengothic
  SIGNING_ENV_PREFIX OPENGOTHIC
  MANIFEST app/src/main/AndroidManifest.xml
  JAVA_DIRS app/src/main/java "${generated}/java"
  RESOURCE_DIRS app/src/main/res app/setup/res "${generated}/res"
  DEPENDENCIES com.google.android.material:material:1.13.0
  CMAKE_ARGUMENTS -DBUILD_SHARED_LIBS=OFF -DTEMPEST_BUILD_SHARED=OFF -DTEMPEST_BUILD_AUDIO=ON
  CPP_FLAGS -std=c++20 -fvisibility=hidden -ffunction-sections -fdata-sections
  ASSET_PROPERTY privateGameAssets
  NO_COMPRESS zip ogpart
  SHRINK_RELEASE ANDROIDX)
