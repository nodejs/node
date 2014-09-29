// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_INL_H_
#define V8_IC_INL_H_

#include "src/ic.h"

#include "src/compiler.h"
#include "src/debug.h"
#include "src/macro-assembler.h"
#include "src/prototype.h"

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
  DCHECK(result->is_inline_cache_stub());
  return result;
}


void IC::SetTargetAtAddress(Address address,
                            Code* target,
                            ConstantPoolArray* constant_pool) {
  DCHECK(target->is_inline_cache_stub() || target->is_compare_ic_stub());
  Heap* heap = target->GetHeap();
  Code* old_target = GetTargetAtAddress(address, constant_pool);
#ifdef DEBUG
  // STORE_IC and KEYED_STORE_IC use Code::extra_ic_state() to mark
  // ICs as strict mode. The strict-ness of the IC must be preserved.
  if (old_target->kind() == Code::STORE_IC ||
      old_target->kind() == Code::KEYED_STORE_IC) {
    DCHECK(StoreIC::GetStrictMode(old_target->extra_ic_state()) ==
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


template <class TypeClass>
JSFunction* IC::GetRootConstructor(TypeClass* type, Context* native_context) {
  if (type->Is(TypeClass::Boolean())) {
    return native_context->boolean_function();
  } else if (type->Is(TypeClass::Number())) {
    return native_context->number_function();
  } else if (type->Is(TypeClass::String())) {
    return native_context->string_function();
  } else if (type->Is(TypeClass::Symbol())) {
    return native_context->symbol_function();
  } else {
    return NULL;
  }
}


Handle<Map> IC::GetHandlerCacheHolder(HeapType* type, bool receiver_is_holder,
                                      Isolate* isolate, CacheHolderFlag* flag) {
  Handle<Map> receiver_map = TypeToMap(type, isolate);
  if (receiver_is_holder) {
    *flag = kCacheOnReceiver;
    return receiver_map;
  }
  Context* native_context = *isolate->native_context();
  JSFunction* builtin_ctor = GetRootConstructor(type, native_context);
  if (builtin_ctor != NULL) {
    *flag = kCacheOnPrototypeReceiverIsPrimitive;
    return handle(HeapObject::cast(builtin_ctor->instance_prototype())->map());
  }
  *flag = receiver_map->is_dictionary_map()
              ? kCacheOnPrototypeReceiverIsDictionary
              : kCacheOnPrototype;
  // Callers must ensure that the prototype is non-null.
  return handle(JSObject::cast(receiver_map->prototype())->map());
}


Handle<Map> IC::GetICCacheHolder(HeapType* type, Isolate* isolate,
                                 CacheHolderFlag* flag) {
  Context* native_context = *isolate->native_context();
  JSFunction* builtin_ctor = GetRootConstructor(type, native_context);
  if (builtin_ctor != NULL) {
    *flag = kCacheOnPrototype;
    return handle(builtin_ctor->initial_map());
  }
  *flag = kCacheOnReceiver;
  return TypeToMap(type, isolate);
}


IC::State CallIC::FeedbackToState(Handle<FixedArray> vector,
                                  Handle<Smi> slot) const {
  IC::State state = UNINITIALIZED;
  Object* feedback = vector->get(slot->value());

  if (feedback == *TypeFeedbackInfo::MegamorphicSentinel(isolate())) {
    state = GENERIC;
  } else if (feedback->IsAllocationSite() || feedback->IsJSFunction()) {
    state = MONOMORPHIC;
  } else {
    CHECK(feedback == *TypeFeedbackInfo::UninitializedSentinel(isolate()));
  }

  return state;
}
} }  // namespace v8::internal

#endif  // V8_IC_INL_H_
