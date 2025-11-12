// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_STRING_CASE_H_
#define V8_STRINGS_STRING_CASE_H_

#include <cinttypes>

namespace v8 {
namespace internal {

// Computes the prefix length of src that's ascii and matches the case.
template <class CaseMapping>
uint32_t FastAsciiCasePrefixLength(const char* src, uint32_t length);

// Converts ascii chars from src to dst to the expected case in a latin1 string.
// In case non-ascii chars appears it may bail out early. Returns the length of
// the already processed ascii prefix.
template <class CaseMapping>
uint32_t FastAsciiConvert(char* dst, const char* src, uint32_t length);

}  // namespace internal
}  // namespace v8

#endif  // V8_STRINGS_STRING_CASE_H_
