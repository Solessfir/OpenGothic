cmake_minimum_required(VERSION 3.22)
project(OpenGothicAndroid LANGUAGES NONE)

# This project only generates packaging files. Gradle builds the native root project.
include(../lib/Tempest/Engine/cmake/TempestAndroid.cmake)
set(OPENGOTHIC_ANDROID_GAME "gothic2notr" CACHE STRING "Launcher edition: gothic1, gothic2, gothic2notr or archolos")
set_property(CACHE OPENGOTHIC_ANDROID_GAME PROPERTY STRINGS gothic1 gothic2 gothic2notr archolos)
if(OPENGOTHIC_ANDROID_GAME STREQUAL "gothic1" OR OPENGOTHIC_ANDROID_GAME STREQUAL "1")
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
  set(OPENGOTHIC_ICON_COLOR "#80B89D")
else()
  message(FATAL_ERROR "OPENGOTHIC_ANDROID_GAME must be gothic1, gothic2, gothic2notr or archolos")
endif()

# Each launcher has its own Android identity and storage, but shares the engine and UI sources.
set(generated "${CMAKE_CURRENT_BINARY_DIR}/edition")
configure_file(app/SetupActivity.java.in "${generated}/java/org/opengothic/app/SetupActivity.java" @ONLY)
configure_file(app/identity.xml.in "${generated}/res/values/identity.xml" @ONLY)
configure_file(app/shortcuts.xml.in "${generated}/res/xml/shortcuts.xml" @ONLY)

tempest_android_application(OpenGothic
  APPLICATION_ID ${OPENGOTHIC_APPLICATION_ID}
  NATIVE_SOURCE_DIR ..
  NATIVE_TARGET Gothic2Notr
  LIBRARY_NAME opengothic
  VERSION_NAME 0.1-android
  PROPERTY_PREFIX opengothic
  SIGNING_ENV_PREFIX OPENGOTHIC
  MANIFEST app/src/main/AndroidManifest.xml
  JAVA_DIRS app/src/main/java "${generated}/java"
  RESOURCE_DIRS app/src/main/res "${generated}/res"
  DEPENDENCIES com.google.android.material:material:1.13.0
  CMAKE_ARGUMENTS -DBUILD_SHARED_LIBS=OFF -DTEMPEST_BUILD_AUDIO=ON
  CPP_FLAGS -std=c++20 -fvisibility=hidden -ffunction-sections -fdata-sections
  ASSET_PROPERTY privateGameAssets
  NO_COMPRESS zip ogpart
  SHRINK_RELEASE ANDROIDX REPACKAGE)
