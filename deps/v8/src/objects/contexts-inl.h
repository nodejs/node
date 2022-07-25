// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CONTEXTS_INL_H_
#define V8_OBJECTS_CONTEXTS_INL_H_

#include "src/common/globals.h"
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

int ScriptContextTable::used(AcquireLoadTag tag) const {
  return Smi::ToInt(get(kUsedSlotIndex, tag));
}

void ScriptContextTable::set_used(int used, ReleaseStoreTag tag) {
  set(kUsedSlotIndex, Smi::FromInt(used), tag);
}

ACCESSORS(ScriptContextTable, names_to_context_index, NameToIndexHashTable,
          kHashTableOffset)

// static
Handle<Context> ScriptContextTable::GetContext(Isolate* isolate,
                                               Handle<ScriptContextTable> table,
                                               int i) {
  return handle(table->get_context(i), isolate);
}

Context ScriptContextTable::get_context(int i) const {
  DCHECK_LT(i, used(kAcquireLoad));
  return Context::cast(get(i + kFirstContextSlotIndex));
}

Context ScriptContextTable::get_context(int i, AcquireLoadTag tag) const {
  DCHECK_LT(i, used(kAcquireLoad));
  return Context::cast(get(i + kFirstContextSlotIndex, tag));
}

TQ_OBJECT_CONSTRUCTORS_IMPL(Context)
NEVER_READ_ONLY_SPACE_IMPL(Context)

CAST_ACCESSOR(NativeContext)

RELAXED_SMI_ACCESSORS(Context, length, kLengthOffset)

Object Context::get(int index) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index);
}

Object Context::get(PtrComprCageBase cage_base, int index) const {
  DCHECK_LT(static_cast<unsigned int>(index),
            static_cast<unsigned int>(length(kRelaxedLoad)));
  return TaggedField<Object>::Relaxed_Load(cage_base, *this,
                                           OffsetOfElementAt(index));
}

void Context::set(int index, Object value, WriteBarrierMode mode) {
  DCHECK_LT(static_cast<unsigned int>(index),
            static_cast<unsigned int>(length(kRelaxedLoad)));
  const int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

Object Context::get(int index, AcquireLoadTag tag) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index, tag);
}

Object Context::get(PtrComprCageBase cage_base, int index,
                    AcquireLoadTag) const {
  DCHECK_LT(static_cast<unsigned int>(index),
            static_cast<unsigned int>(length(kRelaxedLoad)));
  return ACQUIRE_READ_FIELD(*this, OffsetOfElementAt(index));
}

void Context::set(int index, Object value, WriteBarrierMode mode,
                  ReleaseStoreTag) {
  DCHECK_LT(static_cast<unsigned int>(index),
            static_cast<unsigned int>(length(kRelaxedLoad)));
  const int offset = OffsetOfElementAt(index);
  RELEASE_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

void NativeContext::set(int index, Object value, WriteBarrierMode mode,
                        ReleaseStoreTag tag) {
  Context::set(index, value, mode, tag);
}

ACCESSORS(Context, scope_info, ScopeInfo, kScopeInfoOffset)

Object Context::unchecked_previous() const { return get(PREVIOUS_INDEX); }

Context Context::previous() const {
  Object result = get(PREVIOUS_INDEX);
  DCHECK(IsBootstrappingOrValidParentContext(result, *this));
  return Context::unchecked_cast(result);
}
void Context::set_previous(Context context, WriteBarrierMode mode) {
  set(PREVIOUS_INDEX, context, mode);
}

Object Context::next_context_link() const {
  return get(Context::NEXT_CONTEXT_LINK);
}

bool Context::has_extension() const {
  return scope_info().HasContextExtensionSlot() && !extension().IsUndefined();
}

HeapObject Context::extension() const {
  DCHECK(scope_info().HasContextExtensionSlot());
  return HeapObject::cast(get(EXTENSION_INDEX));
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

#define NATIVE_CONTEXT_FIELD_ACCESSORS(index, type, name)   \
  void Context::set_##name(type value) {                    \
    DCHECK(IsNativeContext());                              \
    set(index, value, UPDATE_WRITE_BARRIER, kReleaseStore); \
  }                                                         \
  bool Context::is_##name(type value) const {               \
    DCHECK(IsNativeContext());                              \
    return type::cast(get(index)) == value;                 \
  }                                                         \
  type Context::name() const {                              \
    DCHECK(IsNativeContext());                              \
    return type::cast(get(index));                          \
  }                                                         \
  type Context::name(AcquireLoadTag tag) const {            \
    DCHECK(IsNativeContext());                              \
    return type::cast(get(index, tag));                     \
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
  Isolate* isolate = GetIsolateForSandbox(*this);
  return reinterpret_cast<MicrotaskQueue*>(ReadExternalPointerField(
      kMicrotaskQueueOffset, isolate, kNativeContextMicrotaskQueueTag));
}

void NativeContext::AllocateExternalPointerEntries(Isolate* isolate) {
  InitExternalPointerField(kMicrotaskQueueOffset, isolate,
                           kNativeContextMicrotaskQueueTag);
}

void NativeContext::set_microtask_queue(Isolate* isolate,
                                        MicrotaskQueue* microtask_queue) {
  WriteExternalPointerField(kMicrotaskQueueOffset, isolate,
                            reinterpret_cast<Address>(microtask_queue),
                            kNativeContextMicrotaskQueueTag);
}

void NativeContext::synchronized_set_script_context_table(
    ScriptContextTable script_context_table) {
  set(SCRIPT_CONTEXT_TABLE_INDEX, script_context_table, UPDATE_WRITE_BARRIER,
      kReleaseStore);
}

ScriptContextTable NativeContext::synchronized_script_context_table() const {
  return ScriptContextTable::cast(
      get(SCRIPT_CONTEXT_TABLE_INDEX, kAcquireLoad));
}

void NativeContext::SetOptimizedCodeListHead(Object head) {
  set(OPTIMIZED_CODE_LIST, head, UPDATE_WEAK_WRITE_BARRIER, kReleaseStore);
}

Object NativeContext::OptimizedCodeListHead() {
  return get(OPTIMIZED_CODE_LIST);
}

void NativeContext::SetDeoptimizedCodeListHead(Object head) {
  set(DEOPTIMIZED_CODE_LIST, head, UPDATE_WEAK_WRITE_BARRIER, kReleaseStore);
}

Object NativeContext::DeoptimizedCodeListHead() {
  return get(DEOPTIMIZED_CODE_LIST);
}

OBJECT_CONSTRUCTORS_IMPL(NativeContext, Context)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CONTEXTS_INL_H_
