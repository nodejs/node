// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_VLQ_BASE64_H_
#define V8_BASE_VLQ_BASE64_H_

#include <stddef.h>
#include <stdint.h>

#include "src/base/base-export.h"

namespace v8 {
namespace base {
V8_BASE_EXPORT int8_t charToDigitDecodeForTesting(uint8_t c);

// Decodes a VLQ-Base64-encoded string into 32bit digits. A valid return value
// is within [-2^31+1, 2^31-1]. This function returns -2^31
// (std::numeric_limits<int32_t>::min()) when bad input s is passed.
V8_BASE_EXPORT int32_t VLQBase64Decode(const char* start, size_t sz,
                                       size_t* pos);
}  // namespace base
}  // namespace v8
#endif  // V8_BASE_VLQ_BASE64_H_
