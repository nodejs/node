/* Copyright 2016 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Macros for compiler / platform specific features and build options.

   Build options are:
    * BROTLI_BUILD_MODERN_COMPILER forces to use modern compilers built-ins,
      features and attributes
 */

#ifndef BROTLI_COMMON_PORT_H_
#define BROTLI_COMMON_PORT_H_

/* Compatibility with non-clang compilers. */
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#define BROTLI_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define BROTLI_GCC_VERSION 0
#endif

#if defined(__ICC)
#define BROTLI_ICC_VERSION __ICC
#else
#define BROTLI_ICC_VERSION 0
#endif

#if defined(BROTLI_BUILD_MODERN_COMPILER)
#define BROTLI_MODERN_COMPILER 1
#elif BROTLI_GCC_VERSION >= 304 || BROTLI_ICC_VERSION >= 1600
#define BROTLI_MODERN_COMPILER 1
#else
#define BROTLI_MODERN_COMPILER 0
#endif

#if defined(BROTLI_SHARED_COMPILATION) && defined(_WIN32)
#if defined(BROTLICOMMON_SHARED_COMPILATION)
#define BROTLI_COMMON_API __declspec(dllexport)
#else
#define BROTLI_COMMON_API __declspec(dllimport)
#endif  /* BROTLICOMMON_SHARED_COMPILATION */
#if defined(BROTLIDEC_SHARED_COMPILATION)
#define BROTLI_DEC_API __declspec(dllexport)
#else
#define BROTLI_DEC_API __declspec(dllimport)
#endif  /* BROTLIDEC_SHARED_COMPILATION */
#if defined(BROTLIENC_SHARED_COMPILATION)
#define BROTLI_ENC_API __declspec(dllexport)
#else
#define BROTLI_ENC_API __declspec(dllimport)
#endif  /* BROTLIENC_SHARED_COMPILATION */
#else  /* BROTLI_SHARED_COMPILATION && _WIN32 */
#define BROTLI_COMMON_API
#define BROTLI_DEC_API
#define BROTLI_ENC_API
#endif

#if BROTLI_MODERN_COMPILER || __has_attribute(deprecated)
#define BROTLI_DEPRECATED __attribute__((deprecated))
#else
#define BROTLI_DEPRECATED
#endif

#endif  /* BROTLI_COMMON_PORT_H_ */
