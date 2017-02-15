// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-flags.h"

#include "src/code-stubs.h"

namespace v8 {
namespace internal {
namespace interpreter {

// static
uint8_t CreateArrayLiteralFlags::Encode(bool use_fast_shallow_clone,
                                        int runtime_flags) {
  uint8_t result = FlagsBits::encode(runtime_flags);
  result |= FastShallowCloneBit::encode(use_fast_shallow_clone);
  return result;
}

// static
uint8_t CreateObjectLiteralFlags::Encode(bool fast_clone_supported,
                                         int properties_count,
                                         int runtime_flags) {
  uint8_t result = FlagsBits::encode(runtime_flags);
  if (fast_clone_supported) {
    STATIC_ASSERT(
        FastCloneShallowObjectStub::kMaximumClonedProperties <=
        1 << CreateObjectLiteralFlags::FastClonePropertiesCountBits::kShift);
    DCHECK_LE(properties_count,
              FastCloneShallowObjectStub::kMaximumClonedProperties);
    result |= CreateObjectLiteralFlags::FastClonePropertiesCountBits::encode(
        properties_count);
  }
  return result;
}

// static
uint8_t CreateClosureFlags::Encode(bool pretenure, bool is_function_scope) {
  uint8_t result = PretenuredBit::encode(pretenure);
  if (!FLAG_always_opt && !FLAG_prepare_always_opt &&
      pretenure == NOT_TENURED && is_function_scope) {
    result |= FastNewClosureBit::encode(true);
  }
  return result;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
