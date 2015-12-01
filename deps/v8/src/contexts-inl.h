// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CONTEXTS_INL_H_
#define V8_CONTEXTS_INL_H_

#include "src/contexts.h"
#include "src/objects-inl.h"

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
  return Handle<Context>::cast(FixedArray::get(table, i + kFirstContextSlot));
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


bool Context::has_extension() { return extension() != nullptr; }
Object* Context::extension() { return get(EXTENSION_INDEX); }
void Context::set_extension(Object* object) { set(EXTENSION_INDEX, object); }


JSModule* Context::module() { return JSModule::cast(get(EXTENSION_INDEX)); }
void Context::set_module(JSModule* module) { set(EXTENSION_INDEX, module); }


GlobalObject* Context::global_object() {
  Object* result = get(GLOBAL_OBJECT_INDEX);
  DCHECK(IsBootstrappingOrGlobalObject(this->GetIsolate(), result));
  return reinterpret_cast<GlobalObject*>(result);
}


void Context::set_global_object(GlobalObject* object) {
  set(GLOBAL_OBJECT_INDEX, object);
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


bool Context::IsBlockContext() {
  Map* map = this->map();
  return map == map->GetHeap()->block_context_map();
}


bool Context::IsModuleContext() {
  Map* map = this->map();
  return map == map->GetHeap()->module_context_map();
}


bool Context::IsScriptContext() {
  Map* map = this->map();
  return map == map->GetHeap()->script_context_map();
}


bool Context::HasSameSecurityTokenAs(Context* that) {
  return this->global_object()->native_context()->security_token() ==
         that->global_object()->native_context()->security_token();
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
