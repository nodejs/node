//===-- Definition of macros from endian.h --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_ENDIAN_MACROS_H
#define LLVM_LIBC_MACROS_ENDIAN_MACROS_H

#include "stdint-macros.h"

#define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define BYTE_ORDER __BYTE_ORDER__

#if BYTE_ORDER == LITTLE_ENDIAN

#define htobe16(x) __builtin_bswap16((x))
#define htobe32(x) __builtin_bswap32((x))
#define htobe64(x) __builtin_bswap64((x))
#define htole16(x) __LLVM_LIBC_CAST(static_cast, uint16_t, x)
#define htole32(x) __LLVM_LIBC_CAST(static_cast, uint32_t, x)
#define htole64(x) __LLVM_LIBC_CAST(static_cast, uint64_t, x)
#define be16toh(x) __builtin_bswap16((x))
#define be32toh(x) __builtin_bswap32((x))
#define be64toh(x) __builtin_bswap64((x))
#define le16toh(x) __LLVM_LIBC_CAST(static_cast, uint16_t, x)
#define le32toh(x) __LLVM_LIBC_CAST(static_cast, uint32_t, x)
#define le64toh(x) __LLVM_LIBC_CAST(static_cast, uint64_t, x)

#else

#define htobe16(x) __LLVM_LIBC_CAST(static_cast, uint16_t, x)
#define htobe32(x) __LLVM_LIBC_CAST(static_cast, uint32_t, x)
#define htobe64(x) __LLVM_LIBC_CAST(static_cast, uint64_t, x)
#define htole16(x) __builtin_bswap16((x))
#define htole32(x) __builtin_bswap32((x))
#define htole64(x) __builtin_bswap64((x))
#define be16toh(x) __LLVM_LIBC_CAST(static_cast, uint16_t, x)
#define be32toh(x) __LLVM_LIBC_CAST(static_cast, uint32_t, x)
#define be64toh(x) __LLVM_LIBC_CAST(static_cast, uint64_t, x)
#define le16toh(x) __builtin_bswap16((x))
#define le32toh(x) __builtin_bswap32((x))
#define le64toh(x) __builtin_bswap64((x))

#endif

#endif // LLVM_LIBC_MACROS_ENDIAN_MACROS_H
