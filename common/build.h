#pragma once

#if defined(OPENGOTHIC_APP_VERSION)
// Android launcher releases show their own version, see android/CMakeLists.txt
static const char* appBuild = "OpenGothic " OPENGOTHIC_APP_VERSION;
#else
static const char* appBuild = "OpenGothic v1.0 dev";
#endif
