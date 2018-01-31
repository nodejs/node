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
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/template-objects.h"

namespace v8 {
namespace internal {


// static
ScriptContextTable* ScriptContextTable::cast(Object* context) {
  DCHECK(context->IsScriptContextTable());
  return reinterpret_cast<ScriptContextTable*>(context);
}

int ScriptContextTable::used() const { return Smi::ToInt(get(kUsedSlot)); }

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

Context* Context::native_context() const {
  Object* result = get(NATIVE_CONTEXT_INDEX);
  DCHECK(IsBootstrappingOrNativeContext(this->GetIsolate(), result));
  return reinterpret_cast<Context*>(result);
}


void Context::set_native_context(Context* context) {
  set(NATIVE_CONTEXT_INDEX, context);
}

bool Context::IsNativeContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->native_context_map();
}

bool Context::IsFunctionContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->function_context_map();
}

bool Context::IsCatchContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->catch_context_map();
}

bool Context::IsWithContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->with_context_map();
}

bool Context::IsDebugEvaluateContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->debug_evaluate_context_map();
}

bool Context::IsBlockContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->block_context_map();
}

bool Context::IsModuleContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->module_context_map();
}

bool Context::IsEvalContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->eval_context_map();
}

bool Context::IsScriptContext() const {
  Map* map = this->map();
  return map == map->GetHeap()->script_context_map();
}

bool Context::HasSameSecurityTokenAs(Context* that) const {
  return this->native_context()->security_token() ==
         that->native_context()->security_token();
}

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  void Context::set_##name(type* value) {                 \
    DCHECK(IsNativeContext());                            \
    set(index, value);                                    \
  }                                                       \
  bool Context::is_##name(type* value) const {            \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index)) == value;               \
  }                                                       \
  type* Context::name() const {                           \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index));                        \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSORS)
#undef NATIVE_CONTEXT_FIELD_ACCESSORS

#define CHECK_FOLLOWS2(v1, v2) STATIC_ASSERT((v1 + 1) == (v2))
#define CHECK_FOLLOWS4(v1, v2, v3, v4) \
  CHECK_FOLLOWS2(v1, v2);              \
  CHECK_FOLLOWS2(v2, v3);              \
  CHECK_FOLLOWS2(v3, v4)

int Context::FunctionMapIndex(LanguageMode language_mode, FunctionKind kind,
                              bool has_prototype_slot, bool has_shared_name,
                              bool needs_home_object) {
  if (IsClassConstructor(kind)) {
    // Like the strict function map, but with no 'name' accessor. 'name'
    // needs to be the last property and it is added during instantiation,
    // in case a static property with the same name exists"
    return CLASS_FUNCTION_MAP_INDEX;
  }

  int base = 0;
  if (IsGeneratorFunction(kind)) {
    CHECK_FOLLOWS4(GENERATOR_FUNCTION_MAP_INDEX,
                   GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX,
                   GENERATOR_FUNCTION_WITH_HOME_OBJECT_MAP_INDEX,
                   GENERATOR_FUNCTION_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX);
    CHECK_FOLLOWS4(
        ASYNC_GENERATOR_FUNCTION_MAP_INDEX,
        ASYNC_GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX,
        ASYNC_GENERATOR_FUNCTION_WITH_HOME_OBJECT_MAP_INDEX,
        ASYNC_GENERATOR_FUNCTION_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX);

    base = IsAsyncFunction(kind) ? ASYNC_GENERATOR_FUNCTION_MAP_INDEX
                                 : GENERATOR_FUNCTION_MAP_INDEX;

  } else if (IsAsyncFunction(kind)) {
    CHECK_FOLLOWS4(ASYNC_FUNCTION_MAP_INDEX, ASYNC_FUNCTION_WITH_NAME_MAP_INDEX,
                   ASYNC_FUNCTION_WITH_HOME_OBJECT_MAP_INDEX,
                   ASYNC_FUNCTION_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX);

    base = ASYNC_FUNCTION_MAP_INDEX;

  } else if (IsArrowFunction(kind) || IsConciseMethod(kind) ||
             IsAccessorFunction(kind)) {
    DCHECK_IMPLIES(IsArrowFunction(kind), !needs_home_object);
    CHECK_FOLLOWS4(STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX,
                   METHOD_WITH_NAME_MAP_INDEX,
                   METHOD_WITH_HOME_OBJECT_MAP_INDEX,
                   METHOD_WITH_NAME_AND_HOME_OBJECT_MAP_INDEX);

    base = STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX;

  } else {
    DCHECK(!needs_home_object);
    CHECK_FOLLOWS2(SLOPPY_FUNCTION_MAP_INDEX,
                   SLOPPY_FUNCTION_WITH_NAME_MAP_INDEX);
    CHECK_FOLLOWS2(STRICT_FUNCTION_MAP_INDEX,
                   STRICT_FUNCTION_WITH_NAME_MAP_INDEX);

    base = is_strict(language_mode) ? STRICT_FUNCTION_MAP_INDEX
                                    : SLOPPY_FUNCTION_MAP_INDEX;
  }
  int offset = static_cast<int>(!has_shared_name) |
               (static_cast<int>(needs_home_object) << 1);
  DCHECK_EQ(0, offset & ~3);

  return base + offset;
}

#undef CHECK_FOLLOWS2
#undef CHECK_FOLLOWS4

Map* Context::GetInitialJSArrayMap(ElementsKind kind) const {
  DCHECK(IsNativeContext());
  if (!IsFastElementsKind(kind)) return nullptr;
  DisallowHeapAllocation no_gc;
  Object* const initial_js_array_map = get(Context::ArrayMapIndex(kind));
  DCHECK(!initial_js_array_map->IsUndefined(GetIsolate()));
  return Map::cast(initial_js_array_map);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CONTEXTS_INL_H_
