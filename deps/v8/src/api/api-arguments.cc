// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-arguments.h"

#include "src/api/api-arguments-inl.h"

namespace v8 {
namespace internal {

void FunctionCallbackArguments::IterateInstance(RootVisitor* v) {
  // Visit newTargetSlot which is located in the frame.
  v->VisitRootPointer(Root::kRelocatable, nullptr, slot_at(T::kNewTargetIndex));

  // Visit all slots above "pc" in this artificial Api callback frame object.
  v->VisitRootPointers(Root::kRelocatable, nullptr,
                       slot_at(T::kFirstApiArgumentIndex),
                       FullObjectSlot(values_.end()));
}

void PropertyCallbackArguments::IterateInstance(RootVisitor* v) {
  // Visit property key slot for named case (for indexed case it contains
  // raw uint32_t value).
  if (is_named()) {
    v->VisitRootPointer(Root::kRelocatable, nullptr,
                        slot_at(T::kPropertyKeyIndex));
  }
  // It's not necessary to visit the optional part because it doesn't contain
  // tagged values (the kValueIndex slot is used as a handle storage only
  // by CallApiSetter builtin).
  v->VisitRootPointers(Root::kRelocatable, nullptr,
                       slot_at(T::kFirstApiArgumentIndex),
                       slot_at(kMandatoryArgsLength));
}

}  // namespace internal
}  // namespace v8
