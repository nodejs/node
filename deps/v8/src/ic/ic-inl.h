// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_IC_INL_H_
#define V8_IC_IC_INL_H_

#include "src/ic/ic.h"

#include "src/assembler-inl.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/handles-inl.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {


Address IC::address() const {
  // Get the address of the call.
  return Assembler::target_address_from_return_address(pc());
}


Address IC::constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    return raw_constant_pool();
  } else {
    return kNullAddress;
  }
}


Address IC::raw_constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    return *constant_pool_address_;
  } else {
    return kNullAddress;
  }
}

void IC::update_receiver_map(Handle<Object> receiver) {
  if (receiver->IsSmi()) {
    receiver_map_ = isolate_->factory()->heap_number_map();
  } else {
    receiver_map_ = handle(HeapObject::cast(*receiver)->map(), isolate_);
  }
}

bool IC::IsHandler(MaybeObject object) {
  HeapObject heap_object;
  return (object->IsSmi() && (object.ptr() != kNullAddress)) ||
         (object->GetHeapObjectIfWeak(&heap_object) &&
          (heap_object->IsMap() || heap_object->IsPropertyCell())) ||
         (object->GetHeapObjectIfStrong(&heap_object) &&
          (heap_object->IsDataHandler() || heap_object->IsCode()));
}

bool IC::AddressIsDeoptimizedCode() const {
  return AddressIsDeoptimizedCode(isolate(), address());
}

// static
bool IC::AddressIsDeoptimizedCode(Isolate* isolate, Address address) {
  Code host =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(address)->code;
  return (host->kind() == Code::OPTIMIZED_FUNCTION &&
          host->marked_for_deoptimization());
}

bool IC::vector_needs_update() {
  if (state() == NO_FEEDBACK) return false;
  return (!vector_set_ &&
          (state() != MEGAMORPHIC ||
           nexus()->GetFeedbackExtra().ToSmi().value() != ELEMENT));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_IC_INL_H_
