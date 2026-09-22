//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Interface for wctype conversion functions.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_WCTYPE_WCTYPE_CONVERSION_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_WCTYPE_WCTYPE_CONVERSION_UTILS_H

#include "hdr/types/wchar_t.h"
#include "hdr/types/wint_t.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace wctype_internal {

// Helper functions for non-ASCII case conversions.
// These are implemented in wctype_conversion_utils.cpp using the generated
// maps.
wint_t tolower(wchar_t wch);
wint_t toupper(wchar_t wch);

} // namespace wctype_internal
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_WCTYPE_WCTYPE_CONVERSION_UTILS_H
