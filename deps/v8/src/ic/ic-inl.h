// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_INL_H_
#define V8_IC_INL_H_

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
    return NULL;
  }
}


Address IC::raw_constant_pool() const {
  if (FLAG_enable_embedded_constant_pool) {
    return *constant_pool_address_;
  } else {
    return NULL;
  }
}


Code* IC::GetTargetAtAddress(Address address, Address constant_pool) {
  // Get the target address of the IC.
  Address target = Assembler::target_address_at(address, constant_pool);
  // Convert target address to the code object. Code::GetCodeFromTargetAddress
  // is safe for use during GC where the map might be marked.
  Code* result = Code::GetCodeFromTargetAddress(target);
  // The result can be an IC dispatcher (for vector-based ICs), an IC handler
  // (for old-style patching ICs) or CEntryStub (for IC dispatchers inlined to
  // bytecode handlers).
  DCHECK(result->is_inline_cache_stub() || result->is_stub());
  return result;
}


void IC::SetTargetAtAddress(Address address, Code* target,
                            Address constant_pool) {
  if (AddressIsDeoptimizedCode(target->GetIsolate(), address)) return;

  // Only these three old-style ICs still do code patching.
  DCHECK(target->is_binary_op_stub() || target->is_compare_ic_stub() ||
         target->is_to_boolean_ic_stub());

  Heap* heap = target->GetHeap();
  Code* old_target = GetTargetAtAddress(address, constant_pool);

  Assembler::set_target_address_at(heap->isolate(), address, constant_pool,
                                   target->instruction_start());
  if (heap->gc_state() == Heap::MARK_COMPACT) {
    heap->mark_compact_collector()->RecordCodeTargetPatch(address, target);
  } else {
    heap->incremental_marking()->RecordCodeTargetPatch(address, target);
  }
  PostPatching(address, target, old_target);
}


void IC::set_target(Code* code) {
  SetTargetAtAddress(address(), code, constant_pool());
}

Code* IC::target() const {
  return GetTargetAtAddress(address(), constant_pool());
}

bool IC::IsHandler(Object* object) {
  return (object->IsSmi() && (object != nullptr)) || object->IsTuple2() ||
         object->IsTuple3() || object->IsFixedArray() || object->IsWeakCell() ||
         (object->IsCode() && Code::cast(object)->is_handler());
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

#endif  // V8_IC_INL_H_
