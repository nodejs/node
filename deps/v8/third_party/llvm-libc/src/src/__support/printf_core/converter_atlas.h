//===-- Map of converter headers in printf ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// This file exists so that if the user wants to supply a custom atlas they can
// just replace the #include, additionally it keeps the ifdefs out of the
// converter header.

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_ATLAS_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_ATLAS_H

// defines convert_string
#include "src/__support/printf_core/string_converter.h"

// defines convert_char
#include "src/__support/printf_core/char_converter.h"

// defines convert_int
#include "src/__support/printf_core/int_converter.h"

#ifndef LIBC_COPT_PRINTF_DISABLE_FLOAT
// defines convert_float_decimal
// defines convert_float_dec_exp
// defines convert_float_dec_auto
#ifdef LIBC_COPT_FLOAT_TO_STR_USE_FLOAT320
#include "src/__support/printf_core/float_dec_converter_limited.h"
#else
#include "src/__support/printf_core/float_dec_converter.h"
#endif
// defines convert_float_hex_exp
#include "src/__support/printf_core/float_hex_converter.h"
#endif // LIBC_COPT_PRINTF_DISABLE_FLOAT

#ifdef LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT
// defines convert_fixed
#include "src/__support/printf_core/fixed_converter.h"
#endif // LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT

#ifndef LIBC_COPT_PRINTF_DISABLE_WRITE_INT
#include "src/__support/printf_core/write_int_converter.h"
#endif // LIBC_COPT_PRINTF_DISABLE_WRITE_INT

// defines convert_pointer
#include "src/__support/printf_core/ptr_converter.h"

#ifndef LIBC_COPT_PRINTF_DISABLE_STRERROR
// defines convert_strerror
#include "src/__support/printf_core/strerror_converter.h"
#endif // LIBC_COPT_PRINTF_DISABLE_STRERROR

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_CONVERTER_ATLAS_H
