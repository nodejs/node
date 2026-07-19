// DO NOT EDIT. This file (and "version") is a template used by the build system
// (either CMake or Meson) to generate a "version.h" header file.
#ifndef JSON_VERSION_H_INCLUDED
#define JSON_VERSION_H_INCLUDED

#define JSONCPP_VERSION_STRING "1.9.0"
#define JSONCPP_VERSION_MAJOR 1
#define JSONCPP_VERSION_MINOR 9
#define JSONCPP_VERSION_PATCH 0
#define JSONCPP_VERSION_QUALIFIER
#define JSONCPP_VERSION_HEXA                                       \
  ((JSONCPP_VERSION_MAJOR << 24) | (JSONCPP_VERSION_MINOR << 16) | \
   (JSONCPP_VERSION_PATCH << 8))

#ifdef JSONCPP_USING_SECURE_MEMORY
#undef JSONCPP_USING_SECURE_MEMORY
#endif
#define JSONCPP_USING_SECURE_MEMORY 0
// If non-zero, the library zeroes any memory that it has allocated before
// it frees its memory.

#endif  // JSON_VERSION_H_INCLUDED
