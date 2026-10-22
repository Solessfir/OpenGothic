# JavaSteam finds protobuf messages, handlers and crypto providers by reflection.
-keep class in.dragonbra.javasteam.** { *; }
-keep class com.google.protobuf.** { *; }
-keep class org.bouncycastle.** { *; }
# zstd-jni and xz back the depot decompression, zstd through JNI.
-keep class com.github.luben.zstd.** { *; }
-keep class org.tukaani.xz.** { *; }

# Desktop-only paths of the libraries that never run on Android.
-dontwarn java.lang.management.**
-dontwarn javax.naming.**
-dontwarn org.slf4j.**
-dontwarn org.apache.logging.log4j.**
-dontwarn org.bouncycastle.**
-dontwarn org.conscrypt.**
-dontwarn org.openjsse.**
-dontwarn reactor.blockhound.**
-dontwarn io.ktor.**
-dontwarn kotlinx.coroutines.debug.**
-dontwarn com.google.protobuf.**
