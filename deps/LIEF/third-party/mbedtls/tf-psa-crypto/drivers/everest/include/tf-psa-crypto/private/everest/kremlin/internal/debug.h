/* Copyright (c) INRIA and Microsoft Corporation. All rights reserved.
   Licensed under the Apache 2.0 License. */

#ifndef __KREMLIN_DEBUG_H
#define __KREMLIN_DEBUG_H

#include <inttypes.h>

#include "kremlin/internal/target.h"

/******************************************************************************/
/* Debugging helpers - intended only for KreMLin developers                   */
/******************************************************************************/

/* In support of "-wasm -d force-c": we might need this function to be
 * forward-declared, because the dependency on WasmSupport appears very late,
 * after SimplifyWasm, and sadly, after the topological order has been done. */
void WasmSupport_check_buffer_size(uint32_t s);

/* A series of GCC atrocities to trace function calls (kremlin's [-d c-calls]
 * option). Useful when trying to debug, say, Wasm, to compare traces. */
/* clang-format off */
#ifdef __GNUC__
#define KRML_FORMAT(X) _Generic((X),                                           \
  uint8_t : "0x%08" PRIx8,                                                     \
  uint16_t: "0x%08" PRIx16,                                                    \
  uint32_t: "0x%08" PRIx32,                                                    \
  uint64_t: "0x%08" PRIx64,                                                    \
  int8_t  : "0x%08" PRIx8,                                                     \
  int16_t : "0x%08" PRIx16,                                                    \
  int32_t : "0x%08" PRIx32,                                                    \
  int64_t : "0x%08" PRIx64,                                                    \
  default : "%s")

#define KRML_FORMAT_ARG(X) _Generic((X),                                       \
  uint8_t : X,                                                                 \
  uint16_t: X,                                                                 \
  uint32_t: X,                                                                 \
  uint64_t: X,                                                                 \
  int8_t  : X,                                                                 \
  int16_t : X,                                                                 \
  int32_t : X,                                                                 \
  int64_t : X,                                                                 \
  default : "unknown")
/* clang-format on */

#  define KRML_DEBUG_RETURN(X)                                                 \
    ({                                                                         \
      __auto_type _ret = (X);                                                  \
      KRML_HOST_PRINTF("returning: ");                                         \
      KRML_HOST_PRINTF(KRML_FORMAT(_ret), KRML_FORMAT_ARG(_ret));              \
      KRML_HOST_PRINTF(" \n");                                                 \
      _ret;                                                                    \
    })
#endif

#endif
