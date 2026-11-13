// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_FAST_API_TEST_HELPERS_H_
#define V8_TEST_COMMON_FAST_API_TEST_HELPERS_H_

#include <cassert>
#include <cstdint>

#include "include/v8-internal.h"
#include "include/v8-object.h"
#include "src/base/bounds.h"
#include "src/base/flags.h"

// Helpers for tests of fast API calls that unwrap embedder objects.

namespace v8 {

// The following should correspond to Chromium's kV8DOMWrapperTypeIndex and
// kV8DOMWrapperObjectIndex.
static const int kV8WrapperTypeIndex = 0;
static const int kV8WrapperObjectIndex = 1;

enum class ApiCheckerResult : uint8_t {
  kNotCalled = 0,
  kSlowCalled = 1 << 0,
  kFastCalled = 1 << 1,
};
using ApiCheckerResultFlags = v8::base::Flags<ApiCheckerResult>;
DEFINE_OPERATORS_FOR_FLAGS(ApiCheckerResultFlags)

inline bool IsValidUnwrapObject(v8::Object* object) {
  internal::Address addr = internal::ValueHelper::ValueAsAddress(object);
  auto instance_type = internal::Internals::GetInstanceType(addr);
  return (v8::base::IsInRange(instance_type,
                              internal::Internals::kFirstJSApiObjectType,
                              internal::Internals::kLastJSApiObjectType) ||
          instance_type == internal::Internals::kJSObjectType ||
          instance_type == internal::Internals::kJSSpecialApiObjectType);
}

template <typename T>
T* GetInternalField(v8::Object* wrapper, v8::EmbedderDataTypeTag tag) {
  assert(kV8WrapperObjectIndex < wrapper->InternalFieldCount());
  return reinterpret_cast<T*>(
      wrapper->GetAlignedPointerFromInternalField(kV8WrapperObjectIndex, tag));
}

}  // namespace v8

#endif  // V8_TEST_COMMON_FAST_API_TEST_HELPERS_H_
