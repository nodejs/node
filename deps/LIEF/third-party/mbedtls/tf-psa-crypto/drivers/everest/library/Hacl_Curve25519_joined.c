/*
 *  Interface to code from Project Everest
 *
 *  Copyright 2016-2018 INRIA and Microsoft Corporation
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of Mbed TLS (https://tls.mbed.org)
 */
#ifndef _BSD_SOURCE
/* Required to get htole64() from gcc/glibc's endian.h (older systems)
 * when we compile with -std=c99 */
#define _BSD_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
/* (modern version of _BSD_SOURCE) */
#define _DEFAULT_SOURCE
#endif

#include "tf_psa_crypto_common.h"
#include "mbedtls/private_access.h"

#if defined(MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED)

#if defined(__SIZEOF_INT128__) && (__SIZEOF_INT128__ == 16)
#define MBEDTLS_HAVE_INT128
#endif

#if defined(MBEDTLS_HAVE_INT128)
#include "Hacl_Curve25519.c"
#else
#define KRML_VERIFIED_UINT128
#include "kremlib/FStar_UInt128_extracted.c"
#include "legacy/Hacl_Curve25519.c"
#endif

#include "kremlib/FStar_UInt64_FStar_UInt32_FStar_UInt16_FStar_UInt8.c"

#endif /* defined(MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED) */
