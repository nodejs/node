// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CONTEXTS_INL_H_
#define V8_OBJECTS_CONTEXTS_INL_H_

#include "src/heap/heap-write-barrier.h"
#include "src/objects/contexts.h"
#include "src/objects/dictionary-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/osr-optimized-code-cache-inl.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/scope-info.h"
#include "src/objects/shared-function-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/contexts-tq-inl.inc"

OBJECT_CONSTRUCTORS_IMPL(ScriptContextTable, FixedArray)
CAST_ACCESSOR(ScriptContextTable)

int ScriptContextTable::synchronized_used() const {
  return Smi::ToInt(get(kUsedSlotIndex, kAcquireLoad));
}

void ScriptContextTable::synchronized_set_used(int used) {
  set(kUsedSlotIndex, Smi::FromInt(used), kReleaseStore);
}

// static
Handle<Context> ScriptContextTable::GetContext(Isolate* isolate,
                                               Handle<ScriptContextTable> table,
                                               int i) {
  return handle(table->get_context(i), isolate);
}

Context ScriptContextTable::get_context(int i) const {
  DCHECK_LT(i, synchronized_used());
  return Context::cast(this->get(i + kFirstContextSlotIndex));
}

TQ_OBJECT_CONSTRUCTORS_IMPL(Context)
NEVER_READ_ONLY_SPACE_IMPL(Context)

CAST_ACCESSOR(NativeContext)

V8_INLINE Object Context::get(int index) const { return elements(index); }
V8_INLINE Object Context::get(PtrComprCageBase cage_base, int index) const {
  return elements(cage_base, index);
}
V8_INLINE void Context::set(int index, Object value) {
  set_elements(index, value);
}
V8_INLINE void Context::set(int index, Object value, WriteBarrierMode mode) {
  set_elements(index, value, mode);
}

void Context::set_scope_info(ScopeInfo scope_info, WriteBarrierMode mode) {
  set(SCOPE_INFO_INDEX, scope_info, mode);
}

Object Context::synchronized_get(int index) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return synchronized_get(cage_base, index);
}

Object Context::synchronized_get(PtrComprCageBase cage_base, int index) const {
  DCHECK_LT(static_cast<unsigned int>(index),
            static_cast<unsigned int>(this->length()));
  return ACQUIRE_READ_FIELD(*this, OffsetOfElementAt(index));
}

void Context::synchronized_set(int index, Object value) {
  DCHECK_LT(static_cast<unsigned int>(index),
            static_cast<unsigned int>(this->length()));
  const int offset = OffsetOfElementAt(index);
  RELEASE_WRITE_FIELD(*this, offset, value);
  WRITE_BARRIER(*this, offset, value);
}

Object Context::unchecked_previous() { return get(PREVIOUS_INDEX); }

Context Context::previous() {
  Object result = get(PREVIOUS_INDEX);
  DCHECK(IsBootstrappingOrValidParentContext(result, *this));
  return Context::unchecked_cast(result);
}
void Context::set_previous(Context context, WriteBarrierMode mode) {
  set(PREVIOUS_INDEX, context, mode);
}

Object Context::next_context_link() { return get(Context::NEXT_CONTEXT_LINK); }

bool Context::has_extension() {
  return scope_info().HasContextExtensionSlot() && !extension().IsUndefined();
}

HeapObject Context::extension() {
  DCHECK(scope_info().HasContextExtensionSlot());
  return HeapObject::cast(get(EXTENSION_INDEX));
}

void Context::set_extension(HeapObject object, WriteBarrierMode mode) {
  DCHECK(scope_info().HasContextExtensionSlot());
  set(EXTENSION_INDEX, object, mode);
}

NativeContext Context::native_context() const {
  return this->map().native_context();
}

bool Context::IsFunctionContext() const {
  return map().instance_type() == FUNCTION_CONTEXT_TYPE;
}

bool Context::IsCatchContext() const {
  return map().instance_type() == CATCH_CONTEXT_TYPE;
}

bool Context::IsWithContext() const {
  return map().instance_type() == WITH_CONTEXT_TYPE;
}

bool Context::IsDebugEvaluateContext() const {
  return map().instance_type() == DEBUG_EVALUATE_CONTEXT_TYPE;
}

bool Context::IsAwaitContext() const {
  return map().instance_type() == AWAIT_CONTEXT_TYPE;
}

bool Context::IsBlockContext() const {
  return map().instance_type() == BLOCK_CONTEXT_TYPE;
}

bool Context::IsModuleContext() const {
  return map().instance_type() == MODULE_CONTEXT_TYPE;
}

bool Context::IsEvalContext() const {
  return map().instance_type() == EVAL_CONTEXT_TYPE;
}

bool Context::IsScriptContext() const {
  return map().instance_type() == SCRIPT_CONTEXT_TYPE;
}

bool Context::HasSameSecurityTokenAs(Context that) const {
  return this->native_context().security_token() ==
         that.native_context().security_token();
}

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name) \
  void Context::set_##name(type value) {                  \
    DCHECK(IsNativeContext());                            \
    set(index, value);                                    \
  }                                                       \
  bool Context::is_##name(type value) const {             \
    DCHECK(IsNativeContext());                            \
    return type::cast(get(index)) == value;               \
  }                                                       \
  type Context::name() const {                            \
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
                              bool has_shared_name) {
  if (IsClassConstructor(kind)) {
    // Like the strict function map, but with no 'name' accessor. 'name'
    // needs to be the last property and it is added during instantiation,
    // in case a static property with the same name exists"
    return CLASS_FUNCTION_MAP_INDEX;
  }

  int base = 0;
  if (IsGeneratorFunction(kind)) {
    CHECK_FOLLOWS2(GENERATOR_FUNCTION_MAP_INDEX,
                   GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX);
    CHECK_FOLLOWS2(ASYNC_GENERATOR_FUNCTION_MAP_INDEX,
                   ASYNC_GENERATOR_FUNCTION_WITH_NAME_MAP_INDEX);

    base = IsAsyncFunction(kind) ? ASYNC_GENERATOR_FUNCTION_MAP_INDEX
                                 : GENERATOR_FUNCTION_MAP_INDEX;

  } else if (IsAsyncFunction(kind) || IsAsyncModule(kind)) {
    CHECK_FOLLOWS2(ASYNC_FUNCTION_MAP_INDEX,
                   ASYNC_FUNCTION_WITH_NAME_MAP_INDEX);

    base = ASYNC_FUNCTION_MAP_INDEX;

  } else if (IsStrictFunctionWithoutPrototype(kind)) {
    CHECK_FOLLOWS2(STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX,
                   METHOD_WITH_NAME_MAP_INDEX);

    base = STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX;

  } else {
    CHECK_FOLLOWS2(SLOPPY_FUNCTION_MAP_INDEX,
                   SLOPPY_FUNCTION_WITH_NAME_MAP_INDEX);
    CHECK_FOLLOWS2(STRICT_FUNCTION_MAP_INDEX,
                   STRICT_FUNCTION_WITH_NAME_MAP_INDEX);

    base = is_strict(language_mode) ? STRICT_FUNCTION_MAP_INDEX
                                    : SLOPPY_FUNCTION_MAP_INDEX;
  }
  int offset = static_cast<int>(!has_shared_name);
  DCHECK_EQ(0, offset & ~1);

  return base + offset;
}

#undef CHECK_FOLLOWS2
#undef CHECK_FOLLOWS4

Map Context::GetInitialJSArrayMap(ElementsKind kind) const {
  DCHECK(IsNativeContext());
  if (!IsFastElementsKind(kind)) return Map();
  DisallowGarbageCollection no_gc;
  Object const initial_js_array_map = get(Context::ArrayMapIndex(kind));
  DCHECK(!initial_js_array_map.IsUndefined());
  return Map::cast(initial_js_array_map);
}

DEF_GETTER(NativeContext, microtask_queue, MicrotaskQueue*) {
  Isolate* isolate = GetIsolateForHeapSandbox(*this);
  return reinterpret_cast<MicrotaskQueue*>(ReadExternalPointerField(
      kMicrotaskQueueOffset, isolate, kNativeContextMicrotaskQueueTag));
}

void NativeContext::AllocateExternalPointerEntries(Isolate* isolate) {
  InitExternalPointerField(kMicrotaskQueueOffset, isolate);
}

void NativeContext::set_microtask_queue(Isolate* isolate,
                                        MicrotaskQueue* microtask_queue) {
  WriteExternalPointerField(kMicrotaskQueueOffset, isolate,
                            reinterpret_cast<Address>(microtask_queue),
                            kNativeContextMicrotaskQueueTag);
}

void NativeContext::synchronized_set_script_context_table(
    ScriptContextTable script_context_table) {
  synchronized_set(SCRIPT_CONTEXT_TABLE_INDEX, script_context_table);
}

ScriptContextTable NativeContext::synchronized_script_context_table() const {
  return ScriptContextTable::cast(synchronized_get(SCRIPT_CONTEXT_TABLE_INDEX));
}

OSROptimizedCodeCache NativeContext::GetOSROptimizedCodeCache() {
  return OSROptimizedCodeCache::cast(osr_code_cache());
}

OBJECT_CONSTRUCTORS_IMPL(NativeContext, Context)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CONTEXTS_INL_H_
