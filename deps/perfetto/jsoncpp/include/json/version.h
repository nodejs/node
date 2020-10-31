#ifndef JSON_VERSION_H_INCLUDED
#define JSON_VERSION_H_INCLUDED

// Note: version must be updated in three places when doing a release. This
// annoying process ensures that amalgamate, CMake, and meson all report the
// correct version.
// 1. /meson.build
// 2. /include/json/version.h
// 3. /CMakeLists.txt
// IMPORTANT: also update the SOVERSION!!

#define JSONCPP_VERSION_STRING "1.9.3"
#define JSONCPP_VERSION_MAJOR 1
#define JSONCPP_VERSION_MINOR 9
#define JSONCPP_VERSION_PATCH 3
#define JSONCPP_VERSION_QUALIFIER
#define JSONCPP_VERSION_HEXA                                                   \
  ((JSONCPP_VERSION_MAJOR << 24) | (JSONCPP_VERSION_MINOR << 16) |             \
   (JSONCPP_VERSION_PATCH << 8))

#ifdef JSONCPP_USING_SECURE_MEMORY
#undef JSONCPP_USING_SECURE_MEMORY
#endif
#define JSONCPP_USING_SECURE_MEMORY 0
// If non-zero, the library zeroes any memory that it has allocated before
// it frees its memory.

#endif // JSON_VERSION_H_INCLUDED
