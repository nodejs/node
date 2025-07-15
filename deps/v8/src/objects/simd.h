// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SIMD_H_
#define V8_OBJECTS_SIMD_H_

#include <cstdint>

#include "include/v8-internal.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

uintptr_t ArrayIndexOfIncludesSmiOrObject(Address array_start,
                                          uintptr_t array_len,
                                          uintptr_t from_index,
                                          Address search_element);
uintptr_t ArrayIndexOfIncludesDouble(Address array_start, uintptr_t array_len,
                                     uintptr_t from_index,
                                     Address search_element);
Tagged<Object> Uint8ArrayToHex(const char* bytes, size_t length,
                               DirectHandle<SeqOneByteString> string_output);
template <typename T>
bool ArrayBufferFromHex(const base::Vector<T>& input_vector, uint8_t* buffer,
                        size_t output_length);

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_SIMD_H_
