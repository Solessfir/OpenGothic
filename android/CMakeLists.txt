cmake_minimum_required(VERSION 3.22)
project(OpenGothicAndroid LANGUAGES NONE)

# This project only generates packaging files. Gradle builds the native root project.
include(../lib/Tempest/Engine/cmake/TempestAndroid.cmake)
tempest_android_application(OpenGothic
  APPLICATION_ID org.opengothic.app
  NATIVE_SOURCE_DIR ..
  NATIVE_TARGET Gothic2Notr
  LIBRARY_NAME opengothic
  VERSION_NAME 0.1-android
  PROPERTY_PREFIX opengothic
  SIGNING_ENV_PREFIX OPENGOTHIC
  MANIFEST app/src/main/AndroidManifest.xml
  JAVA_DIRS app/src/main/java
  RESOURCE_DIRS app/src/main/res
  DEPENDENCIES com.google.android.material:material:1.13.0
  CMAKE_ARGUMENTS -DBUILD_SHARED_LIBS=OFF -DTEMPEST_BUILD_AUDIO=ON
  CPP_FLAGS -std=c++20 -fvisibility=hidden -ffunction-sections -fdata-sections
  ASSET_PROPERTY privateGameAssets
  NO_COMPRESS zip ogpart
  SHRINK_RELEASE ANDROIDX REPACKAGE)
