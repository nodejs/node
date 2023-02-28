#ifndef _HAD_ZIPCONF_H
#define _HAD_ZIPCONF_H

#if __clang__
#else
#define _Nullable
#define _Nonnull
#endif

#define LIBZIP_VERSION "1.9.2"
#define LIBZIP_VERSION_MAJOR 1
#define LIBZIP_VERSION_MINOR 9
#define LIBZIP_VERSION_MICRO 2

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS 1
#endif
#include <inttypes.h>

typedef int8_t zip_int8_t;
typedef uint8_t zip_uint8_t;
typedef int16_t zip_int16_t;
typedef uint16_t zip_uint16_t;
typedef int32_t zip_int32_t;
typedef uint32_t zip_uint32_t;
typedef int64_t zip_int64_t;
typedef uint64_t zip_uint64_t;

#define ZIP_INT8_MIN	 (-ZIP_INT8_MAX-1)
#define ZIP_INT8_MAX	 0x7f
#define ZIP_UINT8_MAX	 0xff

#define ZIP_INT16_MIN	 (-ZIP_INT16_MAX-1)
#define ZIP_INT16_MAX	 0x7fff
#define ZIP_UINT16_MAX	 0xffff

#define ZIP_INT32_MIN	 (-ZIP_INT32_MAX-1L)
#define ZIP_INT32_MAX	 0x7fffffffL
#define ZIP_UINT32_MAX	 0xffffffffLU

#define ZIP_INT64_MIN	 (-ZIP_INT64_MAX-1LL)
#define ZIP_INT64_MAX	 0x7fffffffffffffffLL
#define ZIP_UINT64_MAX	 0xffffffffffffffffULL

#endif /* zipconf.h */
