// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_HEX_FORMAT_H_
#define V8_UTILS_HEX_FORMAT_H_

#include <stddef.h>
#include <stdint.h>

namespace v8 {
namespace internal {

// Takes a byte array in `val` and formats into a hex-based character array
// contained within `formatted`. `formatted` should be a valid buffer which is
// at least 2x the size of `size_of_val`. Additionally, `size_of_val` should be
// less than 0x20000000. If either of these invariants is violated, a CHECK will
// occur.
void FormatBytesToHex(char* formatted, size_t size_of_formatted,
                      const uint8_t* val, size_t size_of_val);

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_HEX_FORMAT_H_
