// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_IC_INL_H_
#define V8_IC_IC_INL_H_

#include "src/ic/ic.h"

#include "src/codegen/assembler-inl.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/handles/handles-inl.h"
#include "src/objects/prototype.h"

namespace v8 {
namespace internal {

void IC::update_lookup_start_object_map(Handle<Object> object) {
  if (object->IsSmi()) {
    lookup_start_object_map_ = isolate_->factory()->heap_number_map();
  } else {
    lookup_start_object_map_ =
        handle(HeapObject::cast(*object).map(), isolate_);
  }
}

bool IC::IsHandler(MaybeObject object) {
  HeapObject heap_object;
  return (object->IsSmi() && (object.ptr() != kNullAddress)) ||
         (object->GetHeapObjectIfWeak(&heap_object) &&
          (heap_object.IsMap() || heap_object.IsPropertyCell())) ||
         (object->GetHeapObjectIfStrong(&heap_object) &&
          (heap_object.IsDataHandler() || heap_object.IsCodeT()));
}

bool IC::vector_needs_update() {
  if (state() == NO_FEEDBACK) return false;
  return (!vector_set_ &&
          (state() != MEGAMORPHIC || nexus()->GetKeyType() != ELEMENT));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_IC_INL_H_
