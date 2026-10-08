/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 License. */

#ifndef KRML_COMPAT_H
#define KRML_COMPAT_H

#include <inttypes.h>

/* A series of macros that define C implementations of types that are not Low*,
 * to facilitate porting programs to Low*. */

typedef const char *Prims_string;

typedef struct {
  uint32_t length;
  const char *data;
} FStar_Bytes_bytes;

typedef int32_t Prims_pos, Prims_nat, Prims_nonzero, Prims_int,
    krml_checked_int_t;

#define RETURN_OR(x)                                                           \
  do {                                                                         \
    int64_t __ret = x;                                                         \
    if (__ret < INT32_MIN || INT32_MAX < __ret) {                              \
      KRML_HOST_PRINTF(                                                        \
          "Prims.{int,nat,pos} integer overflow at %s:%d\n", __FILE__,         \
          __LINE__);                                                           \
      KRML_HOST_EXIT(252);                                                     \
    }                                                                          \
    return (int32_t)__ret;                                                     \
  } while (0)

#endif
