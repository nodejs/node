// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONTEXTS_INL_H_
#define V8_CONTEXTS_INL_H_

#include "src/contexts.h"
#include "src/heap/heap.h"
#include "src/objects-inl.h"
#include "src/objects/dictionary.h"
#include "src/objects/map-inl.h"
#include "src/objects/regexp-match-info.h"

namespace v8 {
namespace internal {


// static
ScriptContextTable* ScriptContextTable::cast(Object* context) {
  DCHECK(context->IsScriptContextTable());
  return reinterpret_cast<ScriptContextTable*>(context);
}


int ScriptContextTable::used() const {
  return Smi::cast(get(kUsedSlot))->value();
}


void ScriptContextTable::set_used(int used) {
  set(kUsedSlot, Smi::FromInt(used));
}


// static
Handle<Context> ScriptContextTable::GetContext(Handle<ScriptContextTable> table,
                                               int i) {
  DCHECK(i < table->used());
  return Handle<Context>::cast(
      FixedArray::get(*table, i + kFirstContextSlot, table->GetIsolate()));
}


// static
Context* Context::cast(Object* context) {
  DCHECK(context->IsContext());
  return reinterpret_cast<Context*>(context);
}


JSFunction* Context::closure() { return JSFunction::cast(get(CLOSURE_INDEX)); }
void Context::set_closure(JSFunction* closure) { set(CLOSURE_INDEX, closure); }


Context* Context::previous() {
  Object* result = get(PREVIOUS_INDEX);
  DCHECK(IsBootstrappingOrValidParentContext(result, this));
  return reinterpret_cast<Context*>(result);
}
void Context::set_previous(Context* context) { set(PREVIOUS_INDEX, context); }

Object* Context::next_context_link() { return get(Context::NEXT_CONTEXT_LINK); }

bool Context::has_extension() { return !extension()->IsTheHole(GetIsolate()); }
HeapObject* Context::extension() {
  return HeapObject::cast(get(EXTENSION_INDEX));
}
void Context::set_extension(HeapObject* object) {
  set(EXTENSION_INDEX, object);
}


Context* Context::native_context() {
  Object* result = get(NATIVE_CONTEXT_INDEX);
  DCHECK(IsBootstrappingOrNativeContext(this->GetIsolate(), result));
  return reinterpret_cast<Context*>(result);
}


void Context::set_native_context(Context* context) {
  set(NATIVE_CONTEXT_INDEX, context);
}


bool Context::IsNativeContext() {
  Map* map = this->map();
  return map == map->GetHeap()->native_context_map();
}


bool Context::IsFunctionContext() {
  Map* map = this->map();
  return map == map->GetHeap()->function_context_map();
}


bool Context::IsCatchContext() {
  Map* map = this->map();
  return map == map->GetHeap()->catch_context_map();
}


bool Context::IsWithContext() {
  Map* map = this->map();
  return map == map->GetHeap()->with_context_map();
}

bool Context::IsDebugEvaluateContext() {
  Map* map = this->map();
  return map == map->GetHeap()->debug_evaluate_context_map();
}

bool Context::IsBlockContext() {
  Map* map = this->map();
  return map == map->GetHeap()->block_context_map();
}


bool Context::IsModuleContext() {
  Map* map = this->map();
  return map == map->GetHeap()->module_context_map();
}

bool Context::IsEvalContext() {
  Map* map = this->map();
  return map == map->GetHeap()->eval_context_map();
}

bool Context::IsScriptContext() {
  Map* map = this->map();
  return map == map->GetHeap()->script_context_map();
}

bool Context::OSROptimizedCodeCacheIsCleared() {
  return osr_code_table() == GetHeap()->empty_fixed_array();
}

bool Context::HasSameSecurityTokenAs(Context* that) {
  return this->native_context()->security_token() ==
         that->native_context()->security_token();
}


#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  void Context::set_##name(type* value) {                 \
    DCHECK(IsNativeContext());                            \
    set(index, value);                                    \
  }                                                       \
  bool Context::is_##name(type* value) {                  \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index)) == value;               \
  }                                                       \
  type* Context::name() {                                 \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index));                        \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSORS)
#undef NATIVE_CONTEXT_FIELD_ACCESSORS


}  // namespace internal
}  // namespace v8

#endif  // V8_CONTEXTS_INL_H_
