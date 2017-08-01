// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Compile-time constants.
 *
 * This header provides access to information about the value serializer at
 * compile time, without declaring or defining any symbols that require linking
 * to V8.
 */

#ifndef INCLUDE_V8_VALUE_SERIALIZER_VERSION_H_
#define INCLUDE_V8_VALUE_SERIALIZER_VERSION_H_

#include <stdint.h>

namespace v8 {

constexpr uint32_t CurrentValueSerializerFormatVersion() { return 13; }

}  // namespace v8

#endif  // INCLUDE_V8_VALUE_SERIALIZER_VERSION_H_
