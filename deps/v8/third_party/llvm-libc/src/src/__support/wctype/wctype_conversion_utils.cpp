//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of wctype conversion helpers.
///
//===----------------------------------------------------------------------===//

#include "wctype_conversion_utils.h"
#include "lower_to_upper.h"
#include "upper_to_lower.h"

namespace LIBC_NAMESPACE_DECL {
namespace wctype_internal {

wint_t tolower(wchar_t wch) {
  if (auto mapped = UPPER_TO_LOWER_MAP.find(wch)) {
    return mapped.value();
  }
#if WINT_MAX > 0xFFFF
  if (auto mapped = UPPER_TO_LOWER_MAP_32.find(wch)) {
    return mapped.value();
  }
#endif
  return wch;
}

wint_t toupper(wchar_t wch) {
  if (auto mapped = LOWER_TO_UPPER_MAP.find(wch)) {
    return mapped.value();
  }
#if WINT_MAX > 0xFFFF
  if (auto mapped = LOWER_TO_UPPER_MAP_32.find(wch)) {
    return mapped.value();
  }
#endif
  return wch;
}

} // namespace wctype_internal
} // namespace LIBC_NAMESPACE_DECL
