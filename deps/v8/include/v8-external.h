// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_EXTERNAL_H_
#define INCLUDE_V8_EXTERNAL_H_

#include "v8-value.h"  // NOLINT(build/include_directory)
#include "v8config.h"  // NOLINT(build/include_directory)

namespace v8 {

class Isolate;

/**
 * A tag for external pointers. Objects with different C++ types should use
 * different values of ExternalPointerTypeTag when using v8::External. The
 * allowed range is 0..V8_EXTERNAL_POINTER_TAG_COUNT - 1. If this is not
 * sufficient, V8_EXTERNAL_POINTER_TAG_COUNT can be increased.
 */
using ExternalPointerTypeTag = uint16_t;

constexpr ExternalPointerTypeTag kExternalPointerTypeTagDefault = 0;

/**
 * A JavaScript value that wraps a C++ void*. This type of value is mainly used
 * to associate C++ data structures with JavaScript objects.
 */
class V8_EXPORT External : public Value {
 public:
  V8_DEPRECATE_SOON("Use the version with the type tag.")
  static Local<External> New(Isolate* isolate, void* value) {
    return New(isolate, value, kExternalPointerTypeTagDefault);
  }
  static Local<External> New(Isolate* isolate, void* value,
                             ExternalPointerTypeTag tag);
  V8_INLINE static External* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<External*>(value);
  }

  V8_DEPRECATE_SOON("Use the version with the type tag.")
  void* Value() const { return Value(kExternalPointerTypeTagDefault); }

  void* Value(ExternalPointerTypeTag tag) const;

 private:
  static void CheckCast(v8::Value* obj);
};

}  // namespace v8

#endif  // INCLUDE_V8_EXTERNAL_H_
