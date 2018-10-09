// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api-arguments.h"

#include "src/api-arguments-inl.h"

namespace v8 {
namespace internal {

PropertyCallbackArguments::PropertyCallbackArguments(Isolate* isolate,
                                                     Object* data, Object* self,
                                                     JSObject* holder,
                                                     ShouldThrow should_throw)
    : Super(isolate) {
  Object** values = this->begin();
  values[T::kThisIndex] = self;
  values[T::kHolderIndex] = holder;
  values[T::kDataIndex] = data;
  values[T::kIsolateIndex] = reinterpret_cast<Object*>(isolate);
  values[T::kShouldThrowOnErrorIndex] =
      Smi::FromInt(should_throw == kThrowOnError ? 1 : 0);

  // Here the hole is set as default value.
  // It cannot escape into js as it's removed in Call below.
  HeapObject* the_hole = ReadOnlyRoots(isolate).the_hole_value();
  values[T::kReturnValueDefaultValueIndex] = the_hole;
  values[T::kReturnValueIndex] = the_hole;
  DCHECK(values[T::kHolderIndex]->IsHeapObject());
  DCHECK(values[T::kIsolateIndex]->IsSmi());
}

FunctionCallbackArguments::FunctionCallbackArguments(
    internal::Isolate* isolate, internal::Object* data,
    internal::HeapObject* callee, internal::Object* holder,
    internal::HeapObject* new_target, internal::Object** argv, int argc)
    : Super(isolate), argv_(argv), argc_(argc) {
  Object** values = begin();
  values[T::kDataIndex] = data;
  values[T::kHolderIndex] = holder;
  values[T::kNewTargetIndex] = new_target;
  values[T::kIsolateIndex] = reinterpret_cast<internal::Object*>(isolate);
  // Here the hole is set as default value.
  // It cannot escape into js as it's remove in Call below.
  HeapObject* the_hole = ReadOnlyRoots(isolate).the_hole_value();
  values[T::kReturnValueDefaultValueIndex] = the_hole;
  values[T::kReturnValueIndex] = the_hole;
  DCHECK(values[T::kHolderIndex]->IsHeapObject());
  DCHECK(values[T::kIsolateIndex]->IsSmi());
}

}  // namespace internal
}  // namespace v8
