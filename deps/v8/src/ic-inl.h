// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_INL_H_
#define V8_IC_INL_H_

#include "ic.h"

#include "compiler.h"
#include "debug.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {


Address IC::address() const {
  // Get the address of the call.
  Address result = Assembler::target_address_from_return_address(pc());

  Debug* debug = isolate()->debug();
  // First check if any break points are active if not just return the address
  // of the call.
  if (!debug->has_break_points()) return result;

  // At least one break point is active perform additional test to ensure that
  // break point locations are updated correctly.
  if (debug->IsDebugBreak(Assembler::target_address_at(result,
                                                       raw_constant_pool()))) {
    // If the call site is a call to debug break then return the address in
    // the original code instead of the address in the running code. This will
    // cause the original code to be updated and keeps the breakpoint active in
    // the running code.
    Code* code = GetCode();
    Code* original_code = GetOriginalCode();
    intptr_t delta =
        original_code->instruction_start() - code->instruction_start();
    // Return the address in the original code. This is the place where
    // the call which has been overwritten by the DebugBreakXXX resides
    // and the place where the inline cache system should look.
    return result + delta;
  } else {
    // No break point here just return the address of the call.
    return result;
  }
}


ConstantPoolArray* IC::constant_pool() const {
  if (!FLAG_enable_ool_constant_pool) {
    return NULL;
  } else {
    Handle<ConstantPoolArray> result = raw_constant_pool_;
    Debug* debug = isolate()->debug();
    // First check if any break points are active if not just return the
    // original constant pool.
    if (!debug->has_break_points()) return *result;

    // At least one break point is active perform additional test to ensure that
    // break point locations are updated correctly.
    Address target = Assembler::target_address_from_return_address(pc());
    if (debug->IsDebugBreak(
            Assembler::target_address_at(target, raw_constant_pool()))) {
      // If the call site is a call to debug break then we want to return the
      // constant pool for the original code instead of the breakpointed code.
      return GetOriginalCode()->constant_pool();
    }
    return *result;
  }
}


ConstantPoolArray* IC::raw_constant_pool() const {
  if (FLAG_enable_ool_constant_pool) {
    return *raw_constant_pool_;
  } else {
    return NULL;
  }
}


Code* IC::GetTargetAtAddress(Address address,
                             ConstantPoolArray* constant_pool) {
  // Get the target address of the IC.
  Address target = Assembler::target_address_at(address, constant_pool);
  // Convert target address to the code object. Code::GetCodeFromTargetAddress
  // is safe for use during GC where the map might be marked.
  Code* result = Code::GetCodeFromTargetAddress(target);
  ASSERT(result->is_inline_cache_stub());
  return result;
}


void IC::SetTargetAtAddress(Address address,
                            Code* target,
                            ConstantPoolArray* constant_pool) {
  ASSERT(target->is_inline_cache_stub() || target->is_compare_ic_stub());
  Heap* heap = target->GetHeap();
  Code* old_target = GetTargetAtAddress(address, constant_pool);
#ifdef DEBUG
  // STORE_IC and KEYED_STORE_IC use Code::extra_ic_state() to mark
  // ICs as strict mode. The strict-ness of the IC must be preserved.
  if (old_target->kind() == Code::STORE_IC ||
      old_target->kind() == Code::KEYED_STORE_IC) {
    ASSERT(StoreIC::GetStrictMode(old_target->extra_ic_state()) ==
           StoreIC::GetStrictMode(target->extra_ic_state()));
  }
#endif
  Assembler::set_target_address_at(
      address, constant_pool, target->instruction_start());
  if (heap->gc_state() == Heap::MARK_COMPACT) {
    heap->mark_compact_collector()->RecordCodeTargetPatch(address, target);
  } else {
    heap->incremental_marking()->RecordCodeTargetPatch(address, target);
  }
  PostPatching(address, target, old_target);
}


InlineCacheHolderFlag IC::GetCodeCacheForObject(Object* object) {
  if (object->IsJSObject()) return OWN_MAP;

  // If the object is a value, we use the prototype map for the cache.
  ASSERT(object->IsString() || object->IsSymbol() ||
         object->IsNumber() || object->IsBoolean());
  return PROTOTYPE_MAP;
}


HeapObject* IC::GetCodeCacheHolder(Isolate* isolate,
                                   Object* object,
                                   InlineCacheHolderFlag holder) {
  if (object->IsSmi()) holder = PROTOTYPE_MAP;
  Object* map_owner = holder == OWN_MAP
      ? object : object->GetPrototype(isolate);
  return HeapObject::cast(map_owner);
}


InlineCacheHolderFlag IC::GetCodeCacheFlag(HeapType* type) {
  if (type->Is(HeapType::Boolean()) ||
      type->Is(HeapType::Number()) ||
      type->Is(HeapType::String()) ||
      type->Is(HeapType::Symbol())) {
    return PROTOTYPE_MAP;
  }
  return OWN_MAP;
}


Handle<Map> IC::GetCodeCacheHolder(InlineCacheHolderFlag flag,
                                   HeapType* type,
                                   Isolate* isolate) {
  if (flag == PROTOTYPE_MAP) {
    Context* context = isolate->context()->native_context();
    JSFunction* constructor;
    if (type->Is(HeapType::Boolean())) {
      constructor = context->boolean_function();
    } else if (type->Is(HeapType::Number())) {
      constructor = context->number_function();
    } else if (type->Is(HeapType::String())) {
      constructor = context->string_function();
    } else {
      ASSERT(type->Is(HeapType::Symbol()));
      constructor = context->symbol_function();
    }
    return handle(JSObject::cast(constructor->instance_prototype())->map());
  }
  return TypeToMap(type, isolate);
}


} }  // namespace v8::internal

#endif  // V8_IC_INL_H_
