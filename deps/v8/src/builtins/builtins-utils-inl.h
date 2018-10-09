// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_UTILS_INL_H_
#define V8_BUILTINS_BUILTINS_UTILS_INL_H_

#include "src/builtins/builtins-utils.h"

#include "src/arguments-inl.h"

namespace v8 {
namespace internal {

Handle<Object> BuiltinArguments::atOrUndefined(Isolate* isolate, int index) {
  if (index >= length()) {
    return isolate->factory()->undefined_value();
  }
  return at<Object>(index);
}

Handle<Object> BuiltinArguments::receiver() { return at<Object>(0); }

Handle<JSFunction> BuiltinArguments::target() {
  return Arguments::at<JSFunction>(Arguments::length() - 1 - kTargetOffset);
}

Handle<HeapObject> BuiltinArguments::new_target() {
  return Arguments::at<HeapObject>(Arguments::length() - 1 - kNewTargetOffset);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_UTILS_INL_H_
