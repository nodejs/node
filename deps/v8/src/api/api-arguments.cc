// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-arguments.h"

#include "src/api/api-arguments-inl.h"

namespace v8 {
namespace internal {

PropertyCallbackArguments::PropertyCallbackArguments(
    Isolate* isolate, Tagged<Object> data, Tagged<Object> self,
    Tagged<JSObject> holder, Maybe<ShouldThrow> should_throw)
    : Super(isolate)
#ifdef DEBUG
      ,
      javascript_execution_counter_(isolate->javascript_execution_counter())
#endif  // DEBUG
{
  slot_at(T::kThisIndex).store(self);
  slot_at(T::kHolderIndex).store(holder);
  slot_at(T::kDataIndex).store(data);
  slot_at(T::kIsolateIndex)
      .store(Tagged<Object>(reinterpret_cast<Address>(isolate)));
  int value = Internals::kInferShouldThrowMode;
  if (should_throw.IsJust()) {
    value = should_throw.FromJust();
  }
  slot_at(T::kShouldThrowOnErrorIndex).store(Smi::FromInt(value));
  // Here the hole is set as default value.
  // It cannot escape into js as it's removed in Call below.
  Tagged<HeapObject> the_hole_value = ReadOnlyRoots(isolate).the_hole_value();
  slot_at(T::kReturnValueIndex).store(the_hole_value);
  slot_at(T::kHolderV2Index).store(Smi::zero());
  DCHECK(IsHeapObject(*slot_at(T::kHolderIndex)));
  DCHECK(IsSmi(*slot_at(T::kIsolateIndex)));
}

FunctionCallbackArguments::FunctionCallbackArguments(
    Isolate* isolate, Tagged<FunctionTemplateInfo> target,
    Tagged<Object> holder, Tagged<HeapObject> new_target, Address* argv,
    int argc)
    : Super(isolate), argv_(argv), argc_(argc) {
  slot_at(T::kTargetIndex).store(target);
  slot_at(T::kHolderIndex).store(holder);
  slot_at(T::kNewTargetIndex).store(new_target);
  slot_at(T::kIsolateIndex)
      .store(Tagged<Object>(reinterpret_cast<Address>(isolate)));
  // Here the hole is set as default value. It's converted to and not
  // directly exposed to js.
  // TODO(cbruni): Remove and/or use custom sentinel value.
  Tagged<HeapObject> the_hole_value = ReadOnlyRoots(isolate).the_hole_value();
  slot_at(T::kReturnValueIndex).store(the_hole_value);
  slot_at(T::kUnusedIndex).store(Smi::zero());
  DCHECK(IsHeapObject(*slot_at(T::kHolderIndex)));
  DCHECK(IsSmi(*slot_at(T::kIsolateIndex)));
}

}  // namespace internal
}  // namespace v8
