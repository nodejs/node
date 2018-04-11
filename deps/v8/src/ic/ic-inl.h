// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_IC_INL_H_
#define V8_IC_IC_INL_H_

#include "src/ic/ic.h"

#include "src/assembler-inl.h"
#include "src/debug/debug.h"
#include "src/macro-assembler.h"
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
    return nullptr;
  }
}


Address IC::raw_constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    return *constant_pool_address_;
  } else {
    return nullptr;
  }
}


bool IC::IsHandler(Object* object) {
  return (object->IsSmi() && (object != nullptr)) || object->IsDataHandler() ||
         object->IsWeakCell() || object->IsCode();
}

bool IC::AddressIsDeoptimizedCode() const {
  return AddressIsDeoptimizedCode(isolate(), address());
}


bool IC::AddressIsDeoptimizedCode(Isolate* isolate, Address address) {
  Code* host =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(address)->code;
  return (host->kind() == Code::OPTIMIZED_FUNCTION &&
          host->marked_for_deoptimization());
}
}  // namespace internal
}  // namespace v8

#endif  // V8_IC_IC_INL_H_
