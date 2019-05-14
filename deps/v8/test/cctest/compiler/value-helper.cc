// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

// Define constexpr arrays of ValueHelper for external references.
constexpr int8_t ValueHelper::int8_array[];
constexpr int16_t ValueHelper::int16_array[];
constexpr uint32_t ValueHelper::uint32_array[];
constexpr uint64_t ValueHelper::uint64_array[];
constexpr float ValueHelper::float32_array[];
constexpr double ValueHelper::float64_array[];

}  // namespace compiler
}  // namespace internal
}  // namespace v8
