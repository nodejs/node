// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_UTILS_INL_H_
#define V8_BUILTINS_BUILTINS_UTILS_INL_H_

#include "src/builtins/builtins-utils.h"

#include "src/execution/arguments-inl.h"

namespace v8 {
namespace internal {

Handle<Object> BuiltinArguments::atOrUndefined(Isolate* isolate,
                                               int index) const {
  if (index >= length()) {
    return isolate->factory()->undefined_value();
  }
  return at<Object>(index);
}

Handle<Object> BuiltinArguments::receiver() const { return at<Object>(0); }

Handle<JSFunction> BuiltinArguments::target() const {
#ifdef V8_REVERSE_JSARGS
  int index = kTargetOffset;
#else
  int index = Arguments::length() - 1 - kTargetOffset;
#endif
  return Handle<JSFunction>(address_of_arg_at(index));
}

Handle<HeapObject> BuiltinArguments::new_target() const {
#ifdef V8_REVERSE_JSARGS
  int index = kNewTargetOffset;
#else
  int index = Arguments::length() - 1 - kNewTargetOffset;
#endif
  return Handle<JSFunction>(address_of_arg_at(index));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_UTILS_INL_H_
