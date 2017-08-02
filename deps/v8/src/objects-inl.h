// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Review notes:
//
// - The use of macros in these inline functions may seem superfluous
// but it is absolutely needed to make sure gcc generates optimal
// code. gcc is not happy when attempting to inline too deep.
//

#ifndef V8_OBJECTS_INL_H_
#define V8_OBJECTS_INL_H_

#include "src/base/atomicops.h"
#include "src/base/bits.h"
#include "src/builtins/builtins.h"
#include "src/contexts-inl.h"
#include "src/conversions-inl.h"
#include "src/factory.h"
#include "src/feedback-vector-inl.h"
#include "src/field-index-inl.h"
#include "src/field-type.h"
#include "src/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/isolate-inl.h"
#include "src/isolate.h"
#include "src/keys.h"
#include "src/layout-descriptor-inl.h"
#include "src/lookup-cache-inl.h"
#include "src/lookup.h"
#include "src/objects.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/module-info.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/scope-info.h"
#include "src/property.h"
#include "src/prototype.h"
#include "src/string-hasher-inl.h"
#include "src/transitions-inl.h"
#include "src/v8memory.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

PropertyDetails::PropertyDetails(Smi* smi) {
  value_ = smi->value();
}


Smi* PropertyDetails::AsSmi() const {
  // Ensure the upper 2 bits have the same value by sign extending it. This is
  // necessary to be able to use the 31st bit of the property details.
  int value = value_ << 1;
  return Smi::FromInt(value >> 1);
}


int PropertyDetails::field_width_in_words() const {
  DCHECK(location() == kField);
  if (!FLAG_unbox_double_fields) return 1;
  if (kDoubleSize == kPointerSize) return 1;
  return representation().IsDouble() ? kDoubleSize / kPointerSize : 1;
}

#define INT_ACCESSORS(holder, name, offset)                                   \
  int holder::name() const { return READ_INT_FIELD(this, offset); }           \
  void holder::set_##name(int value) { WRITE_INT_FIELD(this, offset, value); }

#define ACCESSORS_CHECKED2(holder, name, type, offset, get_condition, \
                           set_condition)                             \
  type* holder::name() const {                                        \
    DCHECK(get_condition);                                            \
    return type::cast(READ_FIELD(this, offset));                      \
  }                                                                   \
  void holder::set_##name(type* value, WriteBarrierMode mode) {       \
    DCHECK(set_condition);                                            \
    WRITE_FIELD(this, offset, value);                                 \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);  \
  }
#define ACCESSORS_CHECKED(holder, name, type, offset, condition) \
  ACCESSORS_CHECKED2(holder, name, type, offset, condition, condition)

#define ACCESSORS(holder, name, type, offset) \
  ACCESSORS_CHECKED(holder, name, type, offset, true)

// Getter that returns a Smi as an int and writes an int as a Smi.
#define SMI_ACCESSORS_CHECKED(holder, name, offset, condition) \
  int holder::name() const {                                   \
    DCHECK(condition);                                         \
    Object* value = READ_FIELD(this, offset);                  \
    return Smi::cast(value)->value();                          \
  }                                                            \
  void holder::set_##name(int value) {                         \
    DCHECK(condition);                                         \
    WRITE_FIELD(this, offset, Smi::FromInt(value));            \
  }

#define SMI_ACCESSORS(holder, name, offset) \
  SMI_ACCESSORS_CHECKED(holder, name, offset, true)

#define SYNCHRONIZED_SMI_ACCESSORS(holder, name, offset)    \
  int holder::synchronized_##name() const {                 \
    Object* value = ACQUIRE_READ_FIELD(this, offset);       \
    return Smi::cast(value)->value();                       \
  }                                                         \
  void holder::synchronized_set_##name(int value) {         \
    RELEASE_WRITE_FIELD(this, offset, Smi::FromInt(value)); \
  }

#define NOBARRIER_SMI_ACCESSORS(holder, name, offset)          \
  int holder::nobarrier_##name() const {                       \
    Object* value = NOBARRIER_READ_FIELD(this, offset);        \
    return Smi::cast(value)->value();                          \
  }                                                            \
  void holder::nobarrier_set_##name(int value) {               \
    NOBARRIER_WRITE_FIELD(this, offset, Smi::FromInt(value));  \
  }

#define BOOL_GETTER(holder, field, name, offset)           \
  bool holder::name() const {                              \
    return BooleanBit::get(field(), offset);               \
  }                                                        \


#define BOOL_ACCESSORS(holder, field, name, offset)        \
  bool holder::name() const {                              \
    return BooleanBit::get(field(), offset);               \
  }                                                        \
  void holder::set_##name(bool value) {                    \
    set_##field(BooleanBit::set(field(), offset, value));  \
  }

#define TYPE_CHECKER(type, instancetype)           \
  bool HeapObject::Is##type() const {              \
    return map()->instance_type() == instancetype; \
  }

TYPE_CHECKER(BreakPointInfo, TUPLE2_TYPE)
TYPE_CHECKER(ByteArray, BYTE_ARRAY_TYPE)
TYPE_CHECKER(BytecodeArray, BYTECODE_ARRAY_TYPE)
TYPE_CHECKER(CallHandlerInfo, TUPLE2_TYPE)
TYPE_CHECKER(Cell, CELL_TYPE)
TYPE_CHECKER(Code, CODE_TYPE)
TYPE_CHECKER(ConstantElementsPair, TUPLE2_TYPE)
TYPE_CHECKER(FixedDoubleArray, FIXED_DOUBLE_ARRAY_TYPE)
TYPE_CHECKER(Foreign, FOREIGN_TYPE)
TYPE_CHECKER(FreeSpace, FREE_SPACE_TYPE)
TYPE_CHECKER(HeapNumber, HEAP_NUMBER_TYPE)
TYPE_CHECKER(JSArgumentsObject, JS_ARGUMENTS_TYPE)
TYPE_CHECKER(JSArray, JS_ARRAY_TYPE)
TYPE_CHECKER(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)
TYPE_CHECKER(JSAsyncGeneratorObject, JS_ASYNC_GENERATOR_OBJECT_TYPE)
TYPE_CHECKER(JSBoundFunction, JS_BOUND_FUNCTION_TYPE)
TYPE_CHECKER(JSContextExtensionObject, JS_CONTEXT_EXTENSION_OBJECT_TYPE)
TYPE_CHECKER(JSDataView, JS_DATA_VIEW_TYPE)
TYPE_CHECKER(JSDate, JS_DATE_TYPE)
TYPE_CHECKER(JSError, JS_ERROR_TYPE)
TYPE_CHECKER(JSFunction, JS_FUNCTION_TYPE)
TYPE_CHECKER(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)
TYPE_CHECKER(JSMap, JS_MAP_TYPE)
TYPE_CHECKER(JSMapIterator, JS_MAP_ITERATOR_TYPE)
TYPE_CHECKER(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)
TYPE_CHECKER(JSModuleNamespace, JS_MODULE_NAMESPACE_TYPE)
TYPE_CHECKER(JSPromiseCapability, JS_PROMISE_CAPABILITY_TYPE)
TYPE_CHECKER(JSPromise, JS_PROMISE_TYPE)
TYPE_CHECKER(JSRegExp, JS_REGEXP_TYPE)
TYPE_CHECKER(JSSet, JS_SET_TYPE)
TYPE_CHECKER(JSSetIterator, JS_SET_ITERATOR_TYPE)
TYPE_CHECKER(JSAsyncFromSyncIterator, JS_ASYNC_FROM_SYNC_ITERATOR_TYPE)
TYPE_CHECKER(JSStringIterator, JS_STRING_ITERATOR_TYPE)
TYPE_CHECKER(JSTypedArray, JS_TYPED_ARRAY_TYPE)
TYPE_CHECKER(JSValue, JS_VALUE_TYPE)
TYPE_CHECKER(JSWeakMap, JS_WEAK_MAP_TYPE)
TYPE_CHECKER(JSWeakSet, JS_WEAK_SET_TYPE)
TYPE_CHECKER(Map, MAP_TYPE)
TYPE_CHECKER(MutableHeapNumber, MUTABLE_HEAP_NUMBER_TYPE)
TYPE_CHECKER(Oddball, ODDBALL_TYPE)
TYPE_CHECKER(PropertyCell, PROPERTY_CELL_TYPE)
TYPE_CHECKER(SharedFunctionInfo, SHARED_FUNCTION_INFO_TYPE)
TYPE_CHECKER(SourcePositionTableWithFrameCache, TUPLE2_TYPE)
TYPE_CHECKER(Symbol, SYMBOL_TYPE)
TYPE_CHECKER(TransitionArray, TRANSITION_ARRAY_TYPE)
TYPE_CHECKER(TypeFeedbackInfo, TUPLE3_TYPE)
TYPE_CHECKER(WeakCell, WEAK_CELL_TYPE)
TYPE_CHECKER(WeakFixedArray, FIXED_ARRAY_TYPE)

#define TYPED_ARRAY_TYPE_CHECKER(Type, type, TYPE, ctype, size) \
  TYPE_CHECKER(Fixed##Type##Array, FIXED_##TYPE##_ARRAY_TYPE)
TYPED_ARRAYS(TYPED_ARRAY_TYPE_CHECKER)
#undef TYPED_ARRAY_TYPE_CHECKER

#undef TYPE_CHECKER

bool HeapObject::IsFixedArrayBase() const {
  return IsFixedArray() || IsFixedDoubleArray() || IsFixedTypedArrayBase();
}

bool HeapObject::IsFixedArray() const {
  InstanceType instance_type = map()->instance_type();
  return instance_type == FIXED_ARRAY_TYPE ||
         instance_type == TRANSITION_ARRAY_TYPE;
}

bool HeapObject::IsSloppyArgumentsElements() const { return IsFixedArray(); }

bool HeapObject::IsJSSloppyArgumentsObject() const {
  return IsJSArgumentsObject();
}

bool HeapObject::IsJSGeneratorObject() const {
  return map()->instance_type() == JS_GENERATOR_OBJECT_TYPE ||
         IsJSAsyncGeneratorObject();
}

bool HeapObject::IsBoilerplateDescription() const { return IsFixedArray(); }

// External objects are not extensible, so the map check is enough.
bool HeapObject::IsExternal() const {
  return map() == GetHeap()->external_map();
}

#define IS_TYPE_FUNCTION_DEF(type_)                               \
  bool Object::Is##type_() const {                                \
    return IsHeapObject() && HeapObject::cast(this)->Is##type_(); \
  }
HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DEF)
#undef IS_TYPE_FUNCTION_DEF

#define IS_TYPE_FUNCTION_DEF(Type, Value)             \
  bool Object::Is##Type(Isolate* isolate) const {     \
    return this == isolate->heap()->Value();          \
  }                                                   \
  bool HeapObject::Is##Type(Isolate* isolate) const { \
    return this == isolate->heap()->Value();          \
  }
ODDBALL_LIST(IS_TYPE_FUNCTION_DEF)
#undef IS_TYPE_FUNCTION_DEF

bool Object::IsNullOrUndefined(Isolate* isolate) const {
  Heap* heap = isolate->heap();
  return this == heap->null_value() || this == heap->undefined_value();
}

bool HeapObject::IsNullOrUndefined(Isolate* isolate) const {
  Heap* heap = isolate->heap();
  return this == heap->null_value() || this == heap->undefined_value();
}

bool HeapObject::IsString() const {
  return map()->instance_type() < FIRST_NONSTRING_TYPE;
}

bool HeapObject::IsName() const {
  return map()->instance_type() <= LAST_NAME_TYPE;
}

bool HeapObject::IsUniqueName() const {
  return IsInternalizedString() || IsSymbol();
}

bool Name::IsUniqueName() const {
  uint32_t type = map()->instance_type();
  return (type & (kIsNotStringMask | kIsNotInternalizedMask)) !=
         (kStringTag | kNotInternalizedTag);
}

bool HeapObject::IsFunction() const {
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  return map()->instance_type() >= FIRST_FUNCTION_TYPE;
}

bool HeapObject::IsCallable() const { return map()->is_callable(); }

bool HeapObject::IsConstructor() const { return map()->is_constructor(); }

bool HeapObject::IsTemplateInfo() const {
  return IsObjectTemplateInfo() || IsFunctionTemplateInfo();
}

bool HeapObject::IsInternalizedString() const {
  uint32_t type = map()->instance_type();
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (type & (kIsNotStringMask | kIsNotInternalizedMask)) ==
      (kStringTag | kInternalizedTag);
}

bool HeapObject::IsConsString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsCons();
}

bool HeapObject::IsThinString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsThin();
}

bool HeapObject::IsSlicedString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSliced();
}

bool HeapObject::IsSeqString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential();
}

bool HeapObject::IsSeqOneByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsOneByteRepresentation();
}

bool HeapObject::IsSeqTwoByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsTwoByteRepresentation();
}

bool HeapObject::IsExternalString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal();
}

bool HeapObject::IsExternalOneByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsOneByteRepresentation();
}

bool HeapObject::IsExternalTwoByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsTwoByteRepresentation();
}

bool Object::IsNumber() const { return IsSmi() || IsHeapNumber(); }

bool HeapObject::IsFiller() const {
  InstanceType instance_type = map()->instance_type();
  return instance_type == FREE_SPACE_TYPE || instance_type == FILLER_TYPE;
}

bool HeapObject::IsFixedTypedArrayBase() const {
  InstanceType instance_type = map()->instance_type();
  return (instance_type >= FIRST_FIXED_TYPED_ARRAY_TYPE &&
          instance_type <= LAST_FIXED_TYPED_ARRAY_TYPE);
}

bool HeapObject::IsJSReceiver() const {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return map()->instance_type() >= FIRST_JS_RECEIVER_TYPE;
}

bool HeapObject::IsJSObject() const {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return map()->IsJSObjectMap();
}

bool HeapObject::IsJSProxy() const { return map()->IsJSProxyMap(); }

bool HeapObject::IsJSArrayIterator() const {
  InstanceType instance_type = map()->instance_type();
  return (instance_type >= FIRST_ARRAY_ITERATOR_TYPE &&
          instance_type <= LAST_ARRAY_ITERATOR_TYPE);
}

bool HeapObject::IsJSWeakCollection() const {
  return IsJSWeakMap() || IsJSWeakSet();
}

bool HeapObject::IsJSCollection() const { return IsJSMap() || IsJSSet(); }

bool HeapObject::IsDescriptorArray() const { return IsFixedArray(); }

bool HeapObject::IsFrameArray() const { return IsFixedArray(); }

bool HeapObject::IsArrayList() const { return IsFixedArray(); }

bool HeapObject::IsRegExpMatchInfo() const { return IsFixedArray(); }

bool Object::IsLayoutDescriptor() const {
  return IsSmi() || IsFixedTypedArrayBase();
}

bool HeapObject::IsFeedbackVector() const {
  return map() == GetHeap()->feedback_vector_map();
}

bool HeapObject::IsFeedbackMetadata() const { return IsFixedArray(); }

bool HeapObject::IsDeoptimizationInputData() const {
  // Must be a fixed array.
  if (!IsFixedArray()) return false;

  // There's no sure way to detect the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can
  // check that the length is zero or else the fixed size plus a multiple of
  // the entry size.
  int length = FixedArray::cast(this)->length();
  if (length == 0) return true;

  length -= DeoptimizationInputData::kFirstDeoptEntryIndex;
  return length >= 0 && length % DeoptimizationInputData::kDeoptEntrySize == 0;
}

bool HeapObject::IsDeoptimizationOutputData() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can check
  // that the length is plausible though.
  if (FixedArray::cast(this)->length() % 2 != 0) return false;
  return true;
}

bool HeapObject::IsHandlerTable() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a handler table array.
  return true;
}

bool HeapObject::IsTemplateList() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a template list.
  if (FixedArray::cast(this)->length() < 1) return false;
  return true;
}

bool HeapObject::IsDependentCode() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a dependent codes array.
  return true;
}

bool HeapObject::IsContext() const {
  Map* map = this->map();
  Heap* heap = GetHeap();
  return (
      map == heap->function_context_map() || map == heap->catch_context_map() ||
      map == heap->with_context_map() || map == heap->native_context_map() ||
      map == heap->block_context_map() || map == heap->module_context_map() ||
      map == heap->eval_context_map() || map == heap->script_context_map() ||
      map == heap->debug_evaluate_context_map());
}

bool HeapObject::IsNativeContext() const {
  return map() == GetHeap()->native_context_map();
}

bool HeapObject::IsScriptContextTable() const {
  return map() == GetHeap()->script_context_table_map();
}

bool HeapObject::IsScopeInfo() const {
  return map() == GetHeap()->scope_info_map();
}

bool HeapObject::IsModuleInfo() const {
  return map() == GetHeap()->module_info_map();
}

template <>
inline bool Is<JSFunction>(Object* obj) {
  return obj->IsJSFunction();
}

bool HeapObject::IsAbstractCode() const {
  return IsBytecodeArray() || IsCode();
}

bool HeapObject::IsStringWrapper() const {
  return IsJSValue() && JSValue::cast(this)->value()->IsString();
}

bool HeapObject::IsBoolean() const {
  return IsOddball() &&
         ((Oddball::cast(this)->kind() & Oddball::kNotBooleanMask) == 0);
}

bool HeapObject::IsJSArrayBufferView() const {
  return IsJSDataView() || IsJSTypedArray();
}

template <>
inline bool Is<JSArray>(Object* obj) {
  return obj->IsJSArray();
}

bool HeapObject::IsHashTable() const {
  return map() == GetHeap()->hash_table_map();
}

bool HeapObject::IsWeakHashTable() const { return IsHashTable(); }

bool HeapObject::IsDictionary() const {
  return IsHashTable() && this != GetHeap()->string_table();
}

bool Object::IsNameDictionary() const { return IsDictionary(); }

bool Object::IsGlobalDictionary() const { return IsDictionary(); }

bool Object::IsSeededNumberDictionary() const { return IsDictionary(); }

bool HeapObject::IsUnseededNumberDictionary() const {
  return map() == GetHeap()->unseeded_number_dictionary_map();
}

bool HeapObject::IsStringTable() const { return IsHashTable(); }

bool HeapObject::IsStringSet() const { return IsHashTable(); }

bool HeapObject::IsObjectHashSet() const { return IsHashTable(); }

bool HeapObject::IsNormalizedMapCache() const {
  return NormalizedMapCache::IsNormalizedMapCache(this);
}

bool HeapObject::IsCompilationCacheTable() const { return IsHashTable(); }

bool HeapObject::IsCodeCacheHashTable() const { return IsHashTable(); }

bool HeapObject::IsMapCache() const { return IsHashTable(); }

bool HeapObject::IsObjectHashTable() const { return IsHashTable(); }

bool HeapObject::IsOrderedHashTable() const {
  return map() == GetHeap()->ordered_hash_table_map();
}

bool Object::IsOrderedHashSet() const { return IsOrderedHashTable(); }

bool Object::IsOrderedHashMap() const { return IsOrderedHashTable(); }

bool Object::IsPrimitive() const {
  return IsSmi() || HeapObject::cast(this)->map()->IsPrimitiveMap();
}

bool HeapObject::IsJSGlobalProxy() const {
  bool result = map()->instance_type() == JS_GLOBAL_PROXY_TYPE;
  DCHECK(!result || map()->is_access_check_needed());
  return result;
}

bool HeapObject::IsUndetectable() const { return map()->is_undetectable(); }

bool HeapObject::IsAccessCheckNeeded() const {
  if (IsJSGlobalProxy()) {
    const JSGlobalProxy* proxy = JSGlobalProxy::cast(this);
    JSGlobalObject* global = proxy->GetIsolate()->context()->global_object();
    return proxy->IsDetachedFrom(global);
  }
  return map()->is_access_check_needed();
}

bool HeapObject::IsStruct() const {
  switch (map()->instance_type()) {
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    return true;
    STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    default:
      return false;
  }
}

#define MAKE_STRUCT_PREDICATE(NAME, Name, name)                  \
  bool Object::Is##Name() const {                                \
    return IsHeapObject() && HeapObject::cast(this)->Is##Name(); \
  }                                                              \
  bool HeapObject::Is##Name() const {                            \
    return map()->instance_type() == NAME##_TYPE;                \
  }
STRUCT_LIST(MAKE_STRUCT_PREDICATE)
#undef MAKE_STRUCT_PREDICATE

double Object::Number() const {
  DCHECK(IsNumber());
  return IsSmi()
             ? static_cast<double>(reinterpret_cast<const Smi*>(this)->value())
             : reinterpret_cast<const HeapNumber*>(this)->value();
}

bool Object::IsNaN() const {
  return this->IsHeapNumber() && std::isnan(HeapNumber::cast(this)->value());
}

bool Object::IsMinusZero() const {
  return this->IsHeapNumber() &&
         i::IsMinusZero(HeapNumber::cast(this)->value());
}

// ------------------------------------
// Cast operations

CAST_ACCESSOR(AbstractCode)
CAST_ACCESSOR(ArrayList)
CAST_ACCESSOR(BoilerplateDescription)
CAST_ACCESSOR(BreakPointInfo)
CAST_ACCESSOR(ByteArray)
CAST_ACCESSOR(BytecodeArray)
CAST_ACCESSOR(CallHandlerInfo)
CAST_ACCESSOR(Cell)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(ConsString)
CAST_ACCESSOR(ConstantElementsPair)
CAST_ACCESSOR(DeoptimizationInputData)
CAST_ACCESSOR(DeoptimizationOutputData)
CAST_ACCESSOR(DependentCode)
CAST_ACCESSOR(DescriptorArray)
CAST_ACCESSOR(ExternalOneByteString)
CAST_ACCESSOR(ExternalString)
CAST_ACCESSOR(ExternalTwoByteString)
CAST_ACCESSOR(FixedArray)
CAST_ACCESSOR(FixedArrayBase)
CAST_ACCESSOR(FixedDoubleArray)
CAST_ACCESSOR(FixedTypedArrayBase)
CAST_ACCESSOR(Foreign)
CAST_ACCESSOR(GlobalDictionary)
CAST_ACCESSOR(HandlerTable)
CAST_ACCESSOR(HeapObject)
CAST_ACCESSOR(JSArgumentsObject);
CAST_ACCESSOR(JSArray)
CAST_ACCESSOR(JSArrayBuffer)
CAST_ACCESSOR(JSArrayBufferView)
CAST_ACCESSOR(JSBoundFunction)
CAST_ACCESSOR(JSDataView)
CAST_ACCESSOR(JSDate)
CAST_ACCESSOR(JSFunction)
CAST_ACCESSOR(JSGeneratorObject)
CAST_ACCESSOR(JSAsyncGeneratorObject)
CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSGlobalProxy)
CAST_ACCESSOR(JSMap)
CAST_ACCESSOR(JSMapIterator)
CAST_ACCESSOR(JSMessageObject)
CAST_ACCESSOR(JSModuleNamespace)
CAST_ACCESSOR(JSObject)
CAST_ACCESSOR(JSProxy)
CAST_ACCESSOR(JSReceiver)
CAST_ACCESSOR(JSRegExp)
CAST_ACCESSOR(JSPromiseCapability)
CAST_ACCESSOR(JSPromise)
CAST_ACCESSOR(JSSet)
CAST_ACCESSOR(JSSetIterator)
CAST_ACCESSOR(JSSloppyArgumentsObject)
CAST_ACCESSOR(JSAsyncFromSyncIterator)
CAST_ACCESSOR(JSStringIterator)
CAST_ACCESSOR(JSArrayIterator)
CAST_ACCESSOR(JSTypedArray)
CAST_ACCESSOR(JSValue)
CAST_ACCESSOR(JSWeakCollection)
CAST_ACCESSOR(JSWeakMap)
CAST_ACCESSOR(JSWeakSet)
CAST_ACCESSOR(LayoutDescriptor)
CAST_ACCESSOR(ModuleInfo)
CAST_ACCESSOR(Name)
CAST_ACCESSOR(NameDictionary)
CAST_ACCESSOR(NormalizedMapCache)
CAST_ACCESSOR(Object)
CAST_ACCESSOR(ObjectHashTable)
CAST_ACCESSOR(ObjectHashSet)
CAST_ACCESSOR(Oddball)
CAST_ACCESSOR(OrderedHashMap)
CAST_ACCESSOR(OrderedHashSet)
CAST_ACCESSOR(PropertyCell)
CAST_ACCESSOR(TemplateList)
CAST_ACCESSOR(RegExpMatchInfo)
CAST_ACCESSOR(ScopeInfo)
CAST_ACCESSOR(SeededNumberDictionary)
CAST_ACCESSOR(SeqOneByteString)
CAST_ACCESSOR(SeqString)
CAST_ACCESSOR(SeqTwoByteString)
CAST_ACCESSOR(SharedFunctionInfo)
CAST_ACCESSOR(SourcePositionTableWithFrameCache)
CAST_ACCESSOR(SlicedString)
CAST_ACCESSOR(SloppyArgumentsElements)
CAST_ACCESSOR(Smi)
CAST_ACCESSOR(String)
CAST_ACCESSOR(StringSet)
CAST_ACCESSOR(StringTable)
CAST_ACCESSOR(Struct)
CAST_ACCESSOR(Symbol)
CAST_ACCESSOR(TemplateInfo)
CAST_ACCESSOR(ThinString)
CAST_ACCESSOR(TypeFeedbackInfo)
CAST_ACCESSOR(UnseededNumberDictionary)
CAST_ACCESSOR(WeakCell)
CAST_ACCESSOR(WeakFixedArray)
CAST_ACCESSOR(WeakHashTable)

#define MAKE_STRUCT_CAST(NAME, Name, name) CAST_ACCESSOR(Name)
STRUCT_LIST(MAKE_STRUCT_CAST)
#undef MAKE_STRUCT_CAST

#undef CAST_ACCESSOR

bool Object::HasValidElements() {
  // Dictionary is covered under FixedArray.
  return IsFixedArray() || IsFixedDoubleArray() || IsFixedTypedArrayBase();
}

bool Object::KeyEquals(Object* second) {
  Object* first = this;
  if (second->IsNumber()) {
    if (first->IsNumber()) return first->Number() == second->Number();
    Object* temp = first;
    first = second;
    second = temp;
  }
  if (first->IsNumber()) {
    DCHECK_LE(0, first->Number());
    uint32_t expected = static_cast<uint32_t>(first->Number());
    uint32_t index;
    return Name::cast(second)->AsArrayIndex(&index) && index == expected;
  }
  return Name::cast(first)->Equals(Name::cast(second));
}

bool Object::FilterKey(PropertyFilter filter) {
  if (IsSymbol()) {
    if (filter & SKIP_SYMBOLS) return true;
    if (Symbol::cast(this)->is_private()) return true;
  } else {
    if (filter & SKIP_STRINGS) return true;
  }
  return false;
}

Handle<Object> Object::NewStorageFor(Isolate* isolate, Handle<Object> object,
                                     Representation representation) {
  if (!representation.IsDouble()) return object;
  Handle<HeapNumber> result = isolate->factory()->NewHeapNumber(MUTABLE);
  if (object->IsUninitialized(isolate)) {
    result->set_value_as_bits(kHoleNanInt64);
  } else if (object->IsMutableHeapNumber()) {
    // Ensure that all bits of the double value are preserved.
    result->set_value_as_bits(HeapNumber::cast(*object)->value_as_bits());
  } else {
    result->set_value(object->Number());
  }
  return result;
}

Handle<Object> Object::WrapForRead(Isolate* isolate, Handle<Object> object,
                                   Representation representation) {
  DCHECK(!object->IsUninitialized(isolate));
  if (!representation.IsDouble()) {
    DCHECK(object->FitsRepresentation(representation));
    return object;
  }
  return isolate->factory()->NewHeapNumber(HeapNumber::cast(*object)->value());
}

StringShape::StringShape(const String* str)
    : type_(str->map()->instance_type()) {
  set_valid();
  DCHECK((type_ & kIsNotStringMask) == kStringTag);
}

StringShape::StringShape(Map* map) : type_(map->instance_type()) {
  set_valid();
  DCHECK((type_ & kIsNotStringMask) == kStringTag);
}

StringShape::StringShape(InstanceType t) : type_(static_cast<uint32_t>(t)) {
  set_valid();
  DCHECK((type_ & kIsNotStringMask) == kStringTag);
}

bool StringShape::IsInternalized() {
  DCHECK(valid());
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (type_ & (kIsNotStringMask | kIsNotInternalizedMask)) ==
         (kStringTag | kInternalizedTag);
}

bool String::IsOneByteRepresentation() const {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kOneByteStringTag;
}

bool String::IsTwoByteRepresentation() const {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}

bool String::IsOneByteRepresentationUnderneath() {
  uint32_t type = map()->instance_type();
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
  DCHECK(IsFlat());
  switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
    case kOneByteStringTag:
      return true;
    case kTwoByteStringTag:
      return false;
    default:  // Cons or sliced string.  Need to go deeper.
      return GetUnderlying()->IsOneByteRepresentation();
  }
}

bool String::IsTwoByteRepresentationUnderneath() {
  uint32_t type = map()->instance_type();
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
  DCHECK(IsFlat());
  switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
    case kOneByteStringTag:
      return false;
    case kTwoByteStringTag:
      return true;
    default:  // Cons or sliced string.  Need to go deeper.
      return GetUnderlying()->IsTwoByteRepresentation();
  }
}

bool String::HasOnlyOneByteChars() {
  uint32_t type = map()->instance_type();
  return (type & kOneByteDataHintMask) == kOneByteDataHintTag ||
         IsOneByteRepresentation();
}

bool StringShape::HasOnlyOneByteChars() {
  return (type_ & kStringEncodingMask) == kOneByteStringTag ||
         (type_ & kOneByteDataHintMask) == kOneByteDataHintTag;
}

bool StringShape::IsCons() {
  return (type_ & kStringRepresentationMask) == kConsStringTag;
}

bool StringShape::IsThin() {
  return (type_ & kStringRepresentationMask) == kThinStringTag;
}

bool StringShape::IsSliced() {
  return (type_ & kStringRepresentationMask) == kSlicedStringTag;
}

bool StringShape::IsIndirect() {
  return (type_ & kIsIndirectStringMask) == kIsIndirectStringTag;
}

bool StringShape::IsExternal() {
  return (type_ & kStringRepresentationMask) == kExternalStringTag;
}

bool StringShape::IsSequential() {
  return (type_ & kStringRepresentationMask) == kSeqStringTag;
}

StringRepresentationTag StringShape::representation_tag() {
  uint32_t tag = (type_ & kStringRepresentationMask);
  return static_cast<StringRepresentationTag>(tag);
}

uint32_t StringShape::encoding_tag() { return type_ & kStringEncodingMask; }

uint32_t StringShape::full_representation_tag() {
  return (type_ & (kStringRepresentationMask | kStringEncodingMask));
}

STATIC_ASSERT((kStringRepresentationMask | kStringEncodingMask) ==
              Internals::kFullStringRepresentationMask);

STATIC_ASSERT(static_cast<uint32_t>(kStringEncodingMask) ==
              Internals::kStringEncodingMask);

bool StringShape::IsSequentialOneByte() {
  return full_representation_tag() == (kSeqStringTag | kOneByteStringTag);
}

bool StringShape::IsSequentialTwoByte() {
  return full_representation_tag() == (kSeqStringTag | kTwoByteStringTag);
}

bool StringShape::IsExternalOneByte() {
  return full_representation_tag() == (kExternalStringTag | kOneByteStringTag);
}

STATIC_ASSERT((kExternalStringTag | kOneByteStringTag) ==
              Internals::kExternalOneByteRepresentationTag);

STATIC_ASSERT(v8::String::ONE_BYTE_ENCODING == kOneByteStringTag);

bool StringShape::IsExternalTwoByte() {
  return full_representation_tag() == (kExternalStringTag | kTwoByteStringTag);
}

STATIC_ASSERT((kExternalStringTag | kTwoByteStringTag) ==
              Internals::kExternalTwoByteRepresentationTag);

STATIC_ASSERT(v8::String::TWO_BYTE_ENCODING == kTwoByteStringTag);

uc32 FlatStringReader::Get(int index) {
  if (is_one_byte_) {
    return Get<uint8_t>(index);
  } else {
    return Get<uc16>(index);
  }
}

template <typename Char>
Char FlatStringReader::Get(int index) {
  DCHECK_EQ(is_one_byte_, sizeof(Char) == 1);
  DCHECK(0 <= index && index <= length_);
  if (sizeof(Char) == 1) {
    return static_cast<Char>(static_cast<const uint8_t*>(start_)[index]);
  } else {
    return static_cast<Char>(static_cast<const uc16*>(start_)[index]);
  }
}

Handle<Object> StringTableShape::AsHandle(Isolate* isolate, HashTableKey* key) {
  return key->AsHandle(isolate);
}

template <typename Char>
class SequentialStringKey : public HashTableKey {
 public:
  explicit SequentialStringKey(Vector<const Char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) {}

  uint32_t Hash() override {
    hash_field_ = StringHasher::HashSequentialString<Char>(
        string_.start(), string_.length(), seed_);

    uint32_t result = hash_field_ >> String::kHashShift;
    DCHECK(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  uint32_t HashForObject(Object* other) override {
    return String::cast(other)->Hash();
  }

  Vector<const Char> string_;
  uint32_t hash_field_;
  uint32_t seed_;
};

class OneByteStringKey : public SequentialStringKey<uint8_t> {
 public:
  OneByteStringKey(Vector<const uint8_t> str, uint32_t seed)
      : SequentialStringKey<uint8_t>(str, seed) {}

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsOneByteEqualTo(string_);
  }

  Handle<Object> AsHandle(Isolate* isolate) override;
};

class SeqOneByteSubStringKey : public HashTableKey {
 public:
  SeqOneByteSubStringKey(Handle<SeqOneByteString> string, int from, int length)
      : string_(string), from_(from), length_(length) {
    DCHECK(string_->IsSeqOneByteString());
  }

// VS 2017 on official builds gives this spurious warning:
// warning C4789: buffer 'key' of size 16 bytes will be overrun; 4 bytes will
// be written starting at offset 16
// https://bugs.chromium.org/p/v8/issues/detail?id=6068
#if defined(V8_CC_MSVC)
#pragma warning(push)
#pragma warning(disable : 4789)
#endif
  uint32_t Hash() override {
    DCHECK(length_ >= 0);
    DCHECK(from_ + length_ <= string_->length());
    const uint8_t* chars = string_->GetChars() + from_;
    hash_field_ = StringHasher::HashSequentialString(
        chars, length_, string_->GetHeap()->HashSeed());
    uint32_t result = hash_field_ >> String::kHashShift;
    DCHECK(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }
#if defined(V8_CC_MSVC)
#pragma warning(pop)
#endif

  uint32_t HashForObject(Object* other) override {
    return String::cast(other)->Hash();
  }

  bool IsMatch(Object* string) override;
  Handle<Object> AsHandle(Isolate* isolate) override;

 private:
  Handle<SeqOneByteString> string_;
  int from_;
  int length_;
  uint32_t hash_field_;
};

class TwoByteStringKey : public SequentialStringKey<uc16> {
 public:
  explicit TwoByteStringKey(Vector<const uc16> str, uint32_t seed)
      : SequentialStringKey<uc16>(str, seed) {}

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsTwoByteEqualTo(string_);
  }

  Handle<Object> AsHandle(Isolate* isolate) override;
};

// Utf8StringKey carries a vector of chars as key.
class Utf8StringKey : public HashTableKey {
 public:
  explicit Utf8StringKey(Vector<const char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) {}

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsUtf8EqualTo(string_);
  }

  uint32_t Hash() override {
    if (hash_field_ != 0) return hash_field_ >> String::kHashShift;
    hash_field_ = StringHasher::ComputeUtf8Hash(string_, seed_, &chars_);
    uint32_t result = hash_field_ >> String::kHashShift;
    DCHECK(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  uint32_t HashForObject(Object* other) override {
    return String::cast(other)->Hash();
  }

  Handle<Object> AsHandle(Isolate* isolate) override {
    if (hash_field_ == 0) Hash();
    return isolate->factory()->NewInternalizedStringFromUtf8(string_, chars_,
                                                             hash_field_);
  }

  Vector<const char> string_;
  uint32_t hash_field_;
  int chars_;  // Caches the number of characters when computing the hash code.
  uint32_t seed_;
};

Representation Object::OptimalRepresentation() {
  if (!FLAG_track_fields) return Representation::Tagged();
  if (IsSmi()) {
    return Representation::Smi();
  } else if (FLAG_track_double_fields && IsHeapNumber()) {
    return Representation::Double();
  } else if (FLAG_track_computed_fields &&
             IsUninitialized(HeapObject::cast(this)->GetIsolate())) {
    return Representation::None();
  } else if (FLAG_track_heap_object_fields) {
    DCHECK(IsHeapObject());
    return Representation::HeapObject();
  } else {
    return Representation::Tagged();
  }
}


ElementsKind Object::OptimalElementsKind() {
  if (IsSmi()) return FAST_SMI_ELEMENTS;
  if (IsNumber()) return FAST_DOUBLE_ELEMENTS;
  return FAST_ELEMENTS;
}


bool Object::FitsRepresentation(Representation representation) {
  if (FLAG_track_fields && representation.IsSmi()) {
    return IsSmi();
  } else if (FLAG_track_double_fields && representation.IsDouble()) {
    return IsMutableHeapNumber() || IsNumber();
  } else if (FLAG_track_heap_object_fields && representation.IsHeapObject()) {
    return IsHeapObject();
  } else if (FLAG_track_fields && representation.IsNone()) {
    return false;
  }
  return true;
}

bool Object::ToUint32(uint32_t* value) {
  if (IsSmi()) {
    int num = Smi::cast(this)->value();
    if (num < 0) return false;
    *value = static_cast<uint32_t>(num);
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    return DoubleToUint32IfEqualToSelf(num, value);
  }
  return false;
}

// static
MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object,
                                         const char* method_name) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  return ToObject(isolate, object, isolate->native_context(), method_name);
}


// static
MaybeHandle<Name> Object::ToName(Isolate* isolate, Handle<Object> input) {
  if (input->IsName()) return Handle<Name>::cast(input);
  return ConvertToName(isolate, input);
}

// static
MaybeHandle<Object> Object::ToPropertyKey(Isolate* isolate,
                                          Handle<Object> value) {
  if (value->IsSmi() || HeapObject::cast(*value)->IsName()) return value;
  return ConvertToPropertyKey(isolate, value);
}

// static
MaybeHandle<Object> Object::ToPrimitive(Handle<Object> input,
                                        ToPrimitiveHint hint) {
  if (input->IsPrimitive()) return input;
  return JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input), hint);
}

// static
MaybeHandle<Object> Object::ToNumber(Handle<Object> input) {
  if (input->IsNumber()) return input;
  return ConvertToNumber(HeapObject::cast(*input)->GetIsolate(), input);
}

// static
MaybeHandle<Object> Object::ToInteger(Isolate* isolate, Handle<Object> input) {
  if (input->IsSmi()) return input;
  return ConvertToInteger(isolate, input);
}

// static
MaybeHandle<Object> Object::ToInt32(Isolate* isolate, Handle<Object> input) {
  if (input->IsSmi()) return input;
  return ConvertToInt32(isolate, input);
}

// static
MaybeHandle<Object> Object::ToUint32(Isolate* isolate, Handle<Object> input) {
  if (input->IsSmi()) return handle(Smi::cast(*input)->ToUint32Smi(), isolate);
  return ConvertToUint32(isolate, input);
}

// static
MaybeHandle<String> Object::ToString(Isolate* isolate, Handle<Object> input) {
  if (input->IsString()) return Handle<String>::cast(input);
  return ConvertToString(isolate, input);
}

// static
MaybeHandle<Object> Object::ToLength(Isolate* isolate, Handle<Object> input) {
  if (input->IsSmi()) {
    int value = std::max(Smi::cast(*input)->value(), 0);
    return handle(Smi::FromInt(value), isolate);
  }
  return ConvertToLength(isolate, input);
}

// static
MaybeHandle<Object> Object::ToIndex(Isolate* isolate, Handle<Object> input,
                                    MessageTemplate::Template error_index) {
  if (input->IsSmi() && Smi::cast(*input)->value() >= 0) return input;
  return ConvertToIndex(isolate, input, error_index);
}

bool Object::HasSpecificClassOf(String* name) {
  return this->IsJSObject() && (JSObject::cast(this)->class_name() == name);
}

MaybeHandle<Object> Object::GetProperty(Handle<Object> object,
                                        Handle<Name> name) {
  LookupIterator it(object, name);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetProperty(&it);
}

MaybeHandle<Object> JSReceiver::GetProperty(Handle<JSReceiver> receiver,
                                            Handle<Name> name) {
  LookupIterator it(receiver, name, receiver);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return Object::GetProperty(&it);
}

MaybeHandle<Object> Object::GetElement(Isolate* isolate, Handle<Object> object,
                                       uint32_t index) {
  LookupIterator it(isolate, object, index);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetProperty(&it);
}

MaybeHandle<Object> JSReceiver::GetElement(Isolate* isolate,
                                           Handle<JSReceiver> receiver,
                                           uint32_t index) {
  LookupIterator it(isolate, receiver, index, receiver);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return Object::GetProperty(&it);
}

Handle<Object> JSReceiver::GetDataProperty(Handle<JSReceiver> object,
                                           Handle<Name> name) {
  LookupIterator it(object, name, object,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetDataProperty(&it);
}

MaybeHandle<Object> Object::SetElement(Isolate* isolate, Handle<Object> object,
                                       uint32_t index, Handle<Object> value,
                                       LanguageMode language_mode) {
  LookupIterator it(isolate, object, index);
  MAYBE_RETURN_NULL(
      SetProperty(&it, value, language_mode, MAY_BE_STORE_FROM_KEYED));
  return value;
}

MaybeHandle<Object> JSReceiver::GetPrototype(Isolate* isolate,
                                             Handle<JSReceiver> receiver) {
  // We don't expect access checks to be needed on JSProxy objects.
  DCHECK(!receiver->IsAccessCheckNeeded() || receiver->IsJSObject());
  PrototypeIterator iter(isolate, receiver, kStartAtReceiver,
                         PrototypeIterator::END_AT_NON_HIDDEN);
  do {
    if (!iter.AdvanceFollowingProxies()) return MaybeHandle<Object>();
  } while (!iter.IsAtEnd());
  return PrototypeIterator::GetCurrent(iter);
}

MaybeHandle<Object> JSReceiver::GetProperty(Isolate* isolate,
                                            Handle<JSReceiver> receiver,
                                            const char* name) {
  Handle<String> str = isolate->factory()->InternalizeUtf8String(name);
  return GetProperty(receiver, str);
}

// static
MUST_USE_RESULT MaybeHandle<FixedArray> JSReceiver::OwnPropertyKeys(
    Handle<JSReceiver> object) {
  return KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                                 ALL_PROPERTIES,
                                 GetKeysConversion::kConvertToString);
}

bool JSObject::PrototypeHasNoElements(Isolate* isolate, JSObject* object) {
  DisallowHeapAllocation no_gc;
  HeapObject* prototype = HeapObject::cast(object->map()->prototype());
  HeapObject* null = isolate->heap()->null_value();
  HeapObject* empty = isolate->heap()->empty_fixed_array();
  while (prototype != null) {
    Map* map = prototype->map();
    if (map->instance_type() <= LAST_CUSTOM_ELEMENTS_RECEIVER) return false;
    if (JSObject::cast(prototype)->elements() != empty) return false;
    prototype = HeapObject::cast(map->prototype());
  }
  return true;
}

#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<byte*>(p) + offset - kHeapObjectTag)

#define FIELD_ADDR_CONST(p, offset) \
  (reinterpret_cast<const byte*>(p) + offset - kHeapObjectTag)

#define READ_FIELD(p, offset) \
  (*reinterpret_cast<Object* const*>(FIELD_ADDR_CONST(p, offset)))

#define ACQUIRE_READ_FIELD(p, offset)           \
  reinterpret_cast<Object*>(base::Acquire_Load( \
      reinterpret_cast<const base::AtomicWord*>(FIELD_ADDR_CONST(p, offset))))

#define NOBARRIER_READ_FIELD(p, offset)           \
  reinterpret_cast<Object*>(base::NoBarrier_Load( \
      reinterpret_cast<const base::AtomicWord*>(FIELD_ADDR_CONST(p, offset))))

#ifdef V8_CONCURRENT_MARKING
#define WRITE_FIELD(p, offset, value)                             \
  base::NoBarrier_Store(                                          \
      reinterpret_cast<base::AtomicWord*>(FIELD_ADDR(p, offset)), \
      reinterpret_cast<base::AtomicWord>(value));
#else
#define WRITE_FIELD(p, offset, value) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)) = value)
#endif

#define RELEASE_WRITE_FIELD(p, offset, value)                     \
  base::Release_Store(                                            \
      reinterpret_cast<base::AtomicWord*>(FIELD_ADDR(p, offset)), \
      reinterpret_cast<base::AtomicWord>(value));

#define NOBARRIER_WRITE_FIELD(p, offset, value)                   \
  base::NoBarrier_Store(                                          \
      reinterpret_cast<base::AtomicWord*>(FIELD_ADDR(p, offset)), \
      reinterpret_cast<base::AtomicWord>(value));

#define WRITE_BARRIER(heap, object, offset, value)          \
  heap->incremental_marking()->RecordWrite(                 \
      object, HeapObject::RawField(object, offset), value); \
  heap->RecordWrite(object, offset, value);

#define FIXED_ARRAY_ELEMENTS_WRITE_BARRIER(heap, array, start, length) \
  do {                                                                 \
    heap->RecordFixedArrayElements(array, start, length);              \
    heap->incremental_marking()->IterateBlackObject(array);            \
  } while (false)

#define CONDITIONAL_WRITE_BARRIER(heap, object, offset, value, mode) \
  if (mode != SKIP_WRITE_BARRIER) {                                  \
    if (mode == UPDATE_WRITE_BARRIER) {                              \
      heap->incremental_marking()->RecordWrite(                      \
          object, HeapObject::RawField(object, offset), value);      \
    }                                                                \
    heap->RecordWrite(object, offset, value);                        \
  }

#define READ_DOUBLE_FIELD(p, offset) \
  ReadDoubleValue(FIELD_ADDR_CONST(p, offset))

#define WRITE_DOUBLE_FIELD(p, offset, value) \
  WriteDoubleValue(FIELD_ADDR(p, offset), value)

#define READ_INT_FIELD(p, offset) \
  (*reinterpret_cast<const int*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_INT_FIELD(p, offset, value) \
  (*reinterpret_cast<int*>(FIELD_ADDR(p, offset)) = value)

#define READ_INTPTR_FIELD(p, offset) \
  (*reinterpret_cast<const intptr_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_INTPTR_FIELD(p, offset, value) \
  (*reinterpret_cast<intptr_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_UINT8_FIELD(p, offset) \
  (*reinterpret_cast<const uint8_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_UINT8_FIELD(p, offset, value) \
  (*reinterpret_cast<uint8_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_INT8_FIELD(p, offset) \
  (*reinterpret_cast<const int8_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_INT8_FIELD(p, offset, value) \
  (*reinterpret_cast<int8_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_UINT16_FIELD(p, offset) \
  (*reinterpret_cast<const uint16_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_UINT16_FIELD(p, offset, value) \
  (*reinterpret_cast<uint16_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_INT16_FIELD(p, offset) \
  (*reinterpret_cast<const int16_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_INT16_FIELD(p, offset, value) \
  (*reinterpret_cast<int16_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_UINT32_FIELD(p, offset) \
  (*reinterpret_cast<const uint32_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_UINT32_FIELD(p, offset, value) \
  (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_INT32_FIELD(p, offset) \
  (*reinterpret_cast<const int32_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_INT32_FIELD(p, offset, value) \
  (*reinterpret_cast<int32_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_FLOAT_FIELD(p, offset) \
  (*reinterpret_cast<const float*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_FLOAT_FIELD(p, offset, value) \
  (*reinterpret_cast<float*>(FIELD_ADDR(p, offset)) = value)

#define READ_UINT64_FIELD(p, offset) \
  (*reinterpret_cast<const uint64_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_UINT64_FIELD(p, offset, value) \
  (*reinterpret_cast<uint64_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_INT64_FIELD(p, offset) \
  (*reinterpret_cast<const int64_t*>(FIELD_ADDR_CONST(p, offset)))

#define WRITE_INT64_FIELD(p, offset, value) \
  (*reinterpret_cast<int64_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_BYTE_FIELD(p, offset) \
  (*reinterpret_cast<const byte*>(FIELD_ADDR_CONST(p, offset)))

#define NOBARRIER_READ_BYTE_FIELD(p, offset) \
  static_cast<byte>(base::NoBarrier_Load(    \
      reinterpret_cast<base::Atomic8*>(FIELD_ADDR(p, offset))))

#define WRITE_BYTE_FIELD(p, offset, value) \
  (*reinterpret_cast<byte*>(FIELD_ADDR(p, offset)) = value)

#define NOBARRIER_WRITE_BYTE_FIELD(p, offset, value)           \
  base::NoBarrier_Store(                                       \
      reinterpret_cast<base::Atomic8*>(FIELD_ADDR(p, offset)), \
      static_cast<base::Atomic8>(value));

Object** HeapObject::RawField(HeapObject* obj, int byte_offset) {
  return reinterpret_cast<Object**>(FIELD_ADDR(obj, byte_offset));
}


MapWord MapWord::FromMap(const Map* map) {
  return MapWord(reinterpret_cast<uintptr_t>(map));
}


Map* MapWord::ToMap() {
  return reinterpret_cast<Map*>(value_);
}

bool MapWord::IsForwardingAddress() const {
  return HAS_SMI_TAG(reinterpret_cast<Object*>(value_));
}


MapWord MapWord::FromForwardingAddress(HeapObject* object) {
  Address raw = reinterpret_cast<Address>(object) - kHeapObjectTag;
  return MapWord(reinterpret_cast<uintptr_t>(raw));
}


HeapObject* MapWord::ToForwardingAddress() {
  DCHECK(IsForwardingAddress());
  return HeapObject::FromAddress(reinterpret_cast<Address>(value_));
}


#ifdef VERIFY_HEAP
void HeapObject::VerifyObjectField(int offset) {
  VerifyPointer(READ_FIELD(this, offset));
}

void HeapObject::VerifySmiField(int offset) {
  CHECK(READ_FIELD(this, offset)->IsSmi());
}
#endif


Heap* HeapObject::GetHeap() const {
  Heap* heap = MemoryChunk::FromAddress(
                   reinterpret_cast<Address>(const_cast<HeapObject*>(this)))
                   ->heap();
  SLOW_DCHECK(heap != NULL);
  return heap;
}


Isolate* HeapObject::GetIsolate() const {
  return GetHeap()->isolate();
}

Map* HeapObject::map() const {
  return map_word().ToMap();
}


void HeapObject::set_map(Map* value) {
  if (value != nullptr) {
#ifdef VERIFY_HEAP
    value->GetHeap()->VerifyObjectLayoutChange(this, value);
#endif
  }
  set_map_word(MapWord::FromMap(value));
  if (value != nullptr) {
    // TODO(1600) We are passing NULL as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, nullptr, value);
  }
}


Map* HeapObject::synchronized_map() {
  return synchronized_map_word().ToMap();
}


void HeapObject::synchronized_set_map(Map* value) {
  if (value != nullptr) {
#ifdef VERIFY_HEAP
    value->GetHeap()->VerifyObjectLayoutChange(this, value);
#endif
  }
  synchronized_set_map_word(MapWord::FromMap(value));
  if (value != nullptr) {
    // TODO(1600) We are passing NULL as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, nullptr, value);
  }
}


// Unsafe accessor omitting write barrier.
void HeapObject::set_map_no_write_barrier(Map* value) {
  if (value != nullptr) {
#ifdef VERIFY_HEAP
    value->GetHeap()->VerifyObjectLayoutChange(this, value);
#endif
  }
  set_map_word(MapWord::FromMap(value));
}

void HeapObject::set_map_after_allocation(Map* value, WriteBarrierMode mode) {
  set_map_word(MapWord::FromMap(value));
  if (mode != SKIP_WRITE_BARRIER) {
    DCHECK(value != nullptr);
    // TODO(1600) We are passing NULL as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, nullptr, value);
  }
}

HeapObject** HeapObject::map_slot() {
  return reinterpret_cast<HeapObject**>(FIELD_ADDR(this, kMapOffset));
}

MapWord HeapObject::map_word() const {
  return MapWord(
      reinterpret_cast<uintptr_t>(NOBARRIER_READ_FIELD(this, kMapOffset)));
}


void HeapObject::set_map_word(MapWord map_word) {
  NOBARRIER_WRITE_FIELD(
      this, kMapOffset, reinterpret_cast<Object*>(map_word.value_));
}


MapWord HeapObject::synchronized_map_word() const {
  return MapWord(
      reinterpret_cast<uintptr_t>(ACQUIRE_READ_FIELD(this, kMapOffset)));
}


void HeapObject::synchronized_set_map_word(MapWord map_word) {
  RELEASE_WRITE_FIELD(
      this, kMapOffset, reinterpret_cast<Object*>(map_word.value_));
}


int HeapObject::Size() {
  return SizeFromMap(map());
}


double HeapNumber::value() const {
  return READ_DOUBLE_FIELD(this, kValueOffset);
}


void HeapNumber::set_value(double value) {
  WRITE_DOUBLE_FIELD(this, kValueOffset, value);
}

uint64_t HeapNumber::value_as_bits() const {
  return READ_UINT64_FIELD(this, kValueOffset);
}

void HeapNumber::set_value_as_bits(uint64_t bits) {
  WRITE_UINT64_FIELD(this, kValueOffset, bits);
}

int HeapNumber::get_exponent() {
  return ((READ_INT_FIELD(this, kExponentOffset) & kExponentMask) >>
          kExponentShift) - kExponentBias;
}


int HeapNumber::get_sign() {
  return READ_INT_FIELD(this, kExponentOffset) & kSignMask;
}

ACCESSORS(JSReceiver, properties, FixedArray, kPropertiesOffset)


Object** FixedArray::GetFirstElementAddress() {
  return reinterpret_cast<Object**>(FIELD_ADDR(this, OffsetOfElementAt(0)));
}


bool FixedArray::ContainsOnlySmisOrHoles() {
  Object* the_hole = GetHeap()->the_hole_value();
  Object** current = GetFirstElementAddress();
  for (int i = 0; i < length(); ++i) {
    Object* candidate = *current++;
    if (!candidate->IsSmi() && candidate != the_hole) return false;
  }
  return true;
}


FixedArrayBase* JSObject::elements() const {
  Object* array = READ_FIELD(this, kElementsOffset);
  return static_cast<FixedArrayBase*>(array);
}

Context* SloppyArgumentsElements::context() {
  return Context::cast(get(kContextIndex));
}

FixedArray* SloppyArgumentsElements::arguments() {
  return FixedArray::cast(get(kArgumentsIndex));
}

void SloppyArgumentsElements::set_arguments(FixedArray* arguments) {
  set(kArgumentsIndex, arguments);
}

uint32_t SloppyArgumentsElements::parameter_map_length() {
  return length() - kParameterMapStart;
}

Object* SloppyArgumentsElements::get_mapped_entry(uint32_t entry) {
  return get(entry + kParameterMapStart);
}

void SloppyArgumentsElements::set_mapped_entry(uint32_t entry, Object* object) {
  set(entry + kParameterMapStart, object);
}

void AllocationSite::Initialize() {
  set_transition_info(Smi::kZero);
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::kZero);
  set_pretenure_data(0);
  set_pretenure_create_count(0);
  set_dependent_code(DependentCode::cast(GetHeap()->empty_fixed_array()),
                     SKIP_WRITE_BARRIER);
}


bool AllocationSite::IsZombie() { return pretenure_decision() == kZombie; }


bool AllocationSite::IsMaybeTenure() {
  return pretenure_decision() == kMaybeTenure;
}


bool AllocationSite::PretenuringDecisionMade() {
  return pretenure_decision() != kUndecided;
}


void AllocationSite::MarkZombie() {
  DCHECK(!IsZombie());
  Initialize();
  set_pretenure_decision(kZombie);
}


ElementsKind AllocationSite::GetElementsKind() {
  DCHECK(!SitePointsToLiteral());
  int value = Smi::cast(transition_info())->value();
  return ElementsKindBits::decode(value);
}


void AllocationSite::SetElementsKind(ElementsKind kind) {
  int value = Smi::cast(transition_info())->value();
  set_transition_info(Smi::FromInt(ElementsKindBits::update(value, kind)),
                      SKIP_WRITE_BARRIER);
}


bool AllocationSite::CanInlineCall() {
  int value = Smi::cast(transition_info())->value();
  return DoNotInlineBit::decode(value) == 0;
}


void AllocationSite::SetDoNotInlineCall() {
  int value = Smi::cast(transition_info())->value();
  set_transition_info(Smi::FromInt(DoNotInlineBit::update(value, true)),
                      SKIP_WRITE_BARRIER);
}


bool AllocationSite::SitePointsToLiteral() {
  // If transition_info is a smi, then it represents an ElementsKind
  // for a constructed array. Otherwise, it must be a boilerplate
  // for an object or array literal.
  return transition_info()->IsJSArray() || transition_info()->IsJSObject();
}


// Heuristic: We only need to create allocation site info if the boilerplate
// elements kind is the initial elements kind.
AllocationSiteMode AllocationSite::GetMode(
    ElementsKind boilerplate_elements_kind) {
  if (IsFastSmiElementsKind(boilerplate_elements_kind)) {
    return TRACK_ALLOCATION_SITE;
  }

  return DONT_TRACK_ALLOCATION_SITE;
}

inline bool AllocationSite::CanTrack(InstanceType type) {
  if (FLAG_turbo) {
    // TurboFan doesn't care at all about String pretenuring feedback,
    // so don't bother even trying to track that.
    return type == JS_ARRAY_TYPE || type == JS_OBJECT_TYPE;
  }
  if (FLAG_allocation_site_pretenuring) {
    return type == JS_ARRAY_TYPE ||
        type == JS_OBJECT_TYPE ||
        type < FIRST_NONSTRING_TYPE;
  }
  return type == JS_ARRAY_TYPE;
}


AllocationSite::PretenureDecision AllocationSite::pretenure_decision() {
  int value = pretenure_data();
  return PretenureDecisionBits::decode(value);
}


void AllocationSite::set_pretenure_decision(PretenureDecision decision) {
  int value = pretenure_data();
  set_pretenure_data(PretenureDecisionBits::update(value, decision));
}


bool AllocationSite::deopt_dependent_code() {
  int value = pretenure_data();
  return DeoptDependentCodeBit::decode(value);
}


void AllocationSite::set_deopt_dependent_code(bool deopt) {
  int value = pretenure_data();
  set_pretenure_data(DeoptDependentCodeBit::update(value, deopt));
}


int AllocationSite::memento_found_count() {
  int value = pretenure_data();
  return MementoFoundCountBits::decode(value);
}


inline void AllocationSite::set_memento_found_count(int count) {
  int value = pretenure_data();
  // Verify that we can count more mementos than we can possibly find in one
  // new space collection.
  DCHECK((GetHeap()->MaxSemiSpaceSize() /
          (Heap::kMinObjectSizeInWords * kPointerSize +
           AllocationMemento::kSize)) < MementoFoundCountBits::kMax);
  DCHECK(count < MementoFoundCountBits::kMax);
  set_pretenure_data(MementoFoundCountBits::update(value, count));
}


int AllocationSite::memento_create_count() { return pretenure_create_count(); }


void AllocationSite::set_memento_create_count(int count) {
  set_pretenure_create_count(count);
}


bool AllocationSite::IncrementMementoFoundCount(int increment) {
  if (IsZombie()) return false;

  int value = memento_found_count();
  set_memento_found_count(value + increment);
  return memento_found_count() >= kPretenureMinimumCreated;
}


inline void AllocationSite::IncrementMementoCreateCount() {
  DCHECK(FLAG_allocation_site_pretenuring);
  int value = memento_create_count();
  set_memento_create_count(value + 1);
}


inline bool AllocationSite::MakePretenureDecision(
    PretenureDecision current_decision,
    double ratio,
    bool maximum_size_scavenge) {
  // Here we just allow state transitions from undecided or maybe tenure
  // to don't tenure, maybe tenure, or tenure.
  if ((current_decision == kUndecided || current_decision == kMaybeTenure)) {
    if (ratio >= kPretenureRatio) {
      // We just transition into tenure state when the semi-space was at
      // maximum capacity.
      if (maximum_size_scavenge) {
        set_deopt_dependent_code(true);
        set_pretenure_decision(kTenure);
        // Currently we just need to deopt when we make a state transition to
        // tenure.
        return true;
      }
      set_pretenure_decision(kMaybeTenure);
    } else {
      set_pretenure_decision(kDontTenure);
    }
  }
  return false;
}


inline bool AllocationSite::DigestPretenuringFeedback(
    bool maximum_size_scavenge) {
  bool deopt = false;
  int create_count = memento_create_count();
  int found_count = memento_found_count();
  bool minimum_mementos_created = create_count >= kPretenureMinimumCreated;
  double ratio =
      minimum_mementos_created || FLAG_trace_pretenuring_statistics ?
          static_cast<double>(found_count) / create_count : 0.0;
  PretenureDecision current_decision = pretenure_decision();

  if (minimum_mementos_created) {
    deopt = MakePretenureDecision(
        current_decision, ratio, maximum_size_scavenge);
  }

  if (FLAG_trace_pretenuring_statistics) {
    PrintIsolate(GetIsolate(),
                 "pretenuring: AllocationSite(%p): (created, found, ratio) "
                 "(%d, %d, %f) %s => %s\n",
                 static_cast<void*>(this), create_count, found_count, ratio,
                 PretenureDecisionName(current_decision),
                 PretenureDecisionName(pretenure_decision()));
  }

  // Clear feedback calculation fields until the next gc.
  set_memento_found_count(0);
  set_memento_create_count(0);
  return deopt;
}


bool AllocationMemento::IsValid() {
  return allocation_site()->IsAllocationSite() &&
         !AllocationSite::cast(allocation_site())->IsZombie();
}


AllocationSite* AllocationMemento::GetAllocationSite() {
  DCHECK(IsValid());
  return AllocationSite::cast(allocation_site());
}

Address AllocationMemento::GetAllocationSiteUnchecked() {
  return reinterpret_cast<Address>(allocation_site());
}

void JSObject::EnsureCanContainHeapObjectElements(Handle<JSObject> object) {
  JSObject::ValidateElements(object);
  ElementsKind elements_kind = object->map()->elements_kind();
  if (!IsFastObjectElementsKind(elements_kind)) {
    if (IsFastHoleyElementsKind(elements_kind)) {
      TransitionElementsKind(object, FAST_HOLEY_ELEMENTS);
    } else {
      TransitionElementsKind(object, FAST_ELEMENTS);
    }
  }
}


void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Object** objects,
                                        uint32_t count,
                                        EnsureElementsMode mode) {
  ElementsKind current_kind = object->GetElementsKind();
  ElementsKind target_kind = current_kind;
  {
    DisallowHeapAllocation no_allocation;
    DCHECK(mode != ALLOW_COPIED_DOUBLE_ELEMENTS);
    bool is_holey = IsFastHoleyElementsKind(current_kind);
    if (current_kind == FAST_HOLEY_ELEMENTS) return;
    Object* the_hole = object->GetHeap()->the_hole_value();
    for (uint32_t i = 0; i < count; ++i) {
      Object* current = *objects++;
      if (current == the_hole) {
        is_holey = true;
        target_kind = GetHoleyElementsKind(target_kind);
      } else if (!current->IsSmi()) {
        if (mode == ALLOW_CONVERTED_DOUBLE_ELEMENTS && current->IsNumber()) {
          if (IsFastSmiElementsKind(target_kind)) {
            if (is_holey) {
              target_kind = FAST_HOLEY_DOUBLE_ELEMENTS;
            } else {
              target_kind = FAST_DOUBLE_ELEMENTS;
            }
          }
        } else if (is_holey) {
          target_kind = FAST_HOLEY_ELEMENTS;
          break;
        } else {
          target_kind = FAST_ELEMENTS;
        }
      }
    }
  }
  if (target_kind != current_kind) {
    TransitionElementsKind(object, target_kind);
  }
}


void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Handle<FixedArrayBase> elements,
                                        uint32_t length,
                                        EnsureElementsMode mode) {
  Heap* heap = object->GetHeap();
  if (elements->map() != heap->fixed_double_array_map()) {
    DCHECK(elements->map() == heap->fixed_array_map() ||
           elements->map() == heap->fixed_cow_array_map());
    if (mode == ALLOW_COPIED_DOUBLE_ELEMENTS) {
      mode = DONT_ALLOW_DOUBLE_ELEMENTS;
    }
    Object** objects =
        Handle<FixedArray>::cast(elements)->GetFirstElementAddress();
    EnsureCanContainElements(object, objects, length, mode);
    return;
  }

  DCHECK(mode == ALLOW_COPIED_DOUBLE_ELEMENTS);
  if (object->GetElementsKind() == FAST_HOLEY_SMI_ELEMENTS) {
    TransitionElementsKind(object, FAST_HOLEY_DOUBLE_ELEMENTS);
  } else if (object->GetElementsKind() == FAST_SMI_ELEMENTS) {
    Handle<FixedDoubleArray> double_array =
        Handle<FixedDoubleArray>::cast(elements);
    for (uint32_t i = 0; i < length; ++i) {
      if (double_array->is_the_hole(i)) {
        TransitionElementsKind(object, FAST_HOLEY_DOUBLE_ELEMENTS);
        return;
      }
    }
    TransitionElementsKind(object, FAST_DOUBLE_ELEMENTS);
  }
}


void JSObject::SetMapAndElements(Handle<JSObject> object,
                                 Handle<Map> new_map,
                                 Handle<FixedArrayBase> value) {
  JSObject::MigrateToMap(object, new_map);
  DCHECK((object->map()->has_fast_smi_or_object_elements() ||
          (*value == object->GetHeap()->empty_fixed_array()) ||
          object->map()->has_fast_string_wrapper_elements()) ==
         (value->map() == object->GetHeap()->fixed_array_map() ||
          value->map() == object->GetHeap()->fixed_cow_array_map()));
  DCHECK((*value == object->GetHeap()->empty_fixed_array()) ||
         (object->map()->has_fast_double_elements() ==
          value->IsFixedDoubleArray()));
  object->set_elements(*value);
}


void JSObject::set_elements(FixedArrayBase* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kElementsOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kElementsOffset, value, mode);
}


void JSObject::initialize_elements() {
  FixedArrayBase* elements = map()->GetInitialElements();
  WRITE_FIELD(this, kElementsOffset, elements);
}


InterceptorInfo* JSObject::GetIndexedInterceptor() {
  return map()->GetIndexedInterceptor();
}

InterceptorInfo* JSObject::GetNamedInterceptor() {
  return map()->GetNamedInterceptor();
}

double Oddball::to_number_raw() const {
  return READ_DOUBLE_FIELD(this, kToNumberRawOffset);
}

void Oddball::set_to_number_raw(double value) {
  WRITE_DOUBLE_FIELD(this, kToNumberRawOffset, value);
}

void Oddball::set_to_number_raw_as_bits(uint64_t bits) {
  WRITE_UINT64_FIELD(this, kToNumberRawOffset, bits);
}

ACCESSORS(Oddball, to_string, String, kToStringOffset)
ACCESSORS(Oddball, to_number, Object, kToNumberOffset)
ACCESSORS(Oddball, type_of, String, kTypeOfOffset)


byte Oddball::kind() const {
  return Smi::cast(READ_FIELD(this, kKindOffset))->value();
}


void Oddball::set_kind(byte value) {
  WRITE_FIELD(this, kKindOffset, Smi::FromInt(value));
}


// static
Handle<Object> Oddball::ToNumber(Handle<Oddball> input) {
  return handle(input->to_number(), input->GetIsolate());
}


ACCESSORS(Cell, value, Object, kValueOffset)
ACCESSORS(PropertyCell, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS(PropertyCell, property_details_raw, Object, kDetailsOffset)
ACCESSORS(PropertyCell, value, Object, kValueOffset)


PropertyDetails PropertyCell::property_details() {
  return PropertyDetails(Smi::cast(property_details_raw()));
}


void PropertyCell::set_property_details(PropertyDetails details) {
  set_property_details_raw(details.AsSmi());
}


Object* WeakCell::value() const { return READ_FIELD(this, kValueOffset); }


void WeakCell::clear() {
  // Either the garbage collector is clearing the cell or we are simply
  // initializing the root empty weak cell.
  DCHECK(GetHeap()->gc_state() == Heap::MARK_COMPACT ||
         this == GetHeap()->empty_weak_cell());
  WRITE_FIELD(this, kValueOffset, Smi::kZero);
}


void WeakCell::initialize(HeapObject* val) {
  WRITE_FIELD(this, kValueOffset, val);
  // We just have to execute the generational barrier here because we never
  // mark through a weak cell and collect evacuation candidates when we process
  // all weak cells.
  WriteBarrierMode mode =
      ObjectMarking::IsBlack(this, MarkingState::Internal(this))
          ? UPDATE_WRITE_BARRIER
          : UPDATE_WEAK_WRITE_BARRIER;
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kValueOffset, val, mode);
}

bool WeakCell::cleared() const { return value() == Smi::kZero; }

Object* WeakCell::next() const { return READ_FIELD(this, kNextOffset); }


void WeakCell::set_next(Object* val, WriteBarrierMode mode) {
  WRITE_FIELD(this, kNextOffset, val);
  if (mode == UPDATE_WRITE_BARRIER) {
    WRITE_BARRIER(GetHeap(), this, kNextOffset, val);
  }
}


void WeakCell::clear_next(Object* the_hole_value) {
  DCHECK_EQ(GetHeap()->the_hole_value(), the_hole_value);
  set_next(the_hole_value, SKIP_WRITE_BARRIER);
}

bool WeakCell::next_cleared() { return next()->IsTheHole(GetIsolate()); }

int JSObject::GetHeaderSize() { return GetHeaderSize(map()->instance_type()); }


int JSObject::GetHeaderSize(InstanceType type) {
  // Check for the most common kind of JavaScript object before
  // falling into the generic switch. This speeds up the internal
  // field operations considerably on average.
  if (type == JS_OBJECT_TYPE) return JSObject::kHeaderSize;
  switch (type) {
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_GENERATOR_OBJECT_TYPE:
      return JSGeneratorObject::kSize;
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
      return JSAsyncGeneratorObject::kSize;
    case JS_GLOBAL_PROXY_TYPE:
      return JSGlobalProxy::kSize;
    case JS_GLOBAL_OBJECT_TYPE:
      return JSGlobalObject::kSize;
    case JS_BOUND_FUNCTION_TYPE:
      return JSBoundFunction::kSize;
    case JS_FUNCTION_TYPE:
      return JSFunction::kSize;
    case JS_VALUE_TYPE:
      return JSValue::kSize;
    case JS_DATE_TYPE:
      return JSDate::kSize;
    case JS_ARRAY_TYPE:
      return JSArray::kSize;
    case JS_ARRAY_BUFFER_TYPE:
      return JSArrayBuffer::kSize;
    case JS_TYPED_ARRAY_TYPE:
      return JSTypedArray::kSize;
    case JS_DATA_VIEW_TYPE:
      return JSDataView::kSize;
    case JS_SET_TYPE:
      return JSSet::kSize;
    case JS_MAP_TYPE:
      return JSMap::kSize;
    case JS_SET_ITERATOR_TYPE:
      return JSSetIterator::kSize;
    case JS_MAP_ITERATOR_TYPE:
      return JSMapIterator::kSize;
    case JS_WEAK_MAP_TYPE:
      return JSWeakMap::kSize;
    case JS_WEAK_SET_TYPE:
      return JSWeakSet::kSize;
    case JS_PROMISE_CAPABILITY_TYPE:
      return JSPromiseCapability::kSize;
    case JS_PROMISE_TYPE:
      return JSPromise::kSize;
    case JS_REGEXP_TYPE:
      return JSRegExp::kSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kSize;
    case JS_ARGUMENTS_TYPE:
      return JSArgumentsObject::kHeaderSize;
    case JS_ERROR_TYPE:
      return JSObject::kHeaderSize;
    case JS_STRING_ITERATOR_TYPE:
      return JSStringIterator::kSize;
    case JS_MODULE_NAMESPACE_TYPE:
      return JSModuleNamespace::kHeaderSize;
    default:
      if (type >= FIRST_ARRAY_ITERATOR_TYPE &&
          type <= LAST_ARRAY_ITERATOR_TYPE) {
        return JSArrayIterator::kSize;
      }
      UNREACHABLE();
      return 0;
  }
}

inline bool IsSpecialReceiverInstanceType(InstanceType instance_type) {
  return instance_type <= LAST_SPECIAL_RECEIVER_TYPE;
}

int JSObject::GetEmbedderFieldCount(Map* map) {
  int instance_size = map->instance_size();
  if (instance_size == kVariableSizeSentinel) return 0;
  InstanceType instance_type = map->instance_type();
  return ((instance_size - GetHeaderSize(instance_type)) >> kPointerSizeLog2) -
         map->GetInObjectProperties();
}

int JSObject::GetEmbedderFieldCount() { return GetEmbedderFieldCount(map()); }

int JSObject::GetEmbedderFieldOffset(int index) {
  DCHECK(index < GetEmbedderFieldCount() && index >= 0);
  return GetHeaderSize() + (kPointerSize * index);
}

Object* JSObject::GetEmbedderField(int index) {
  DCHECK(index < GetEmbedderFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  return READ_FIELD(this, GetHeaderSize() + (kPointerSize * index));
}

void JSObject::SetEmbedderField(int index, Object* value) {
  DCHECK(index < GetEmbedderFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  int offset = GetHeaderSize() + (kPointerSize * index);
  WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(GetHeap(), this, offset, value);
}

void JSObject::SetEmbedderField(int index, Smi* value) {
  DCHECK(index < GetEmbedderFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  int offset = GetHeaderSize() + (kPointerSize * index);
  WRITE_FIELD(this, offset, value);
}


bool JSObject::IsUnboxedDoubleField(FieldIndex index) {
  if (!FLAG_unbox_double_fields) return false;
  return map()->IsUnboxedDoubleField(index);
}


bool Map::IsUnboxedDoubleField(FieldIndex index) {
  if (!FLAG_unbox_double_fields) return false;
  if (index.is_hidden_field() || !index.is_inobject()) return false;
  return !layout_descriptor()->IsTagged(index.property_index());
}


// Access fast-case object properties at index. The use of these routines
// is needed to correctly distinguish between properties stored in-object and
// properties stored in the properties array.
Object* JSObject::RawFastPropertyAt(FieldIndex index) {
  DCHECK(!IsUnboxedDoubleField(index));
  if (index.is_inobject()) {
    return READ_FIELD(this, index.offset());
  } else {
    return properties()->get(index.outobject_array_index());
  }
}


double JSObject::RawFastDoublePropertyAt(FieldIndex index) {
  DCHECK(IsUnboxedDoubleField(index));
  return READ_DOUBLE_FIELD(this, index.offset());
}

uint64_t JSObject::RawFastDoublePropertyAsBitsAt(FieldIndex index) {
  DCHECK(IsUnboxedDoubleField(index));
  return READ_UINT64_FIELD(this, index.offset());
}

void JSObject::RawFastPropertyAtPut(FieldIndex index, Object* value) {
  if (index.is_inobject()) {
    int offset = index.offset();
    WRITE_FIELD(this, offset, value);
    WRITE_BARRIER(GetHeap(), this, offset, value);
  } else {
    properties()->set(index.outobject_array_index(), value);
  }
}

void JSObject::RawFastDoublePropertyAsBitsAtPut(FieldIndex index,
                                                uint64_t bits) {
  WRITE_UINT64_FIELD(this, index.offset(), bits);
}

void JSObject::FastPropertyAtPut(FieldIndex index, Object* value) {
  if (IsUnboxedDoubleField(index)) {
    DCHECK(value->IsMutableHeapNumber());
    // Ensure that all bits of the double value are preserved.
    RawFastDoublePropertyAsBitsAtPut(index,
                                     HeapNumber::cast(value)->value_as_bits());
  } else {
    RawFastPropertyAtPut(index, value);
  }
}

void JSObject::WriteToField(int descriptor, PropertyDetails details,
                            Object* value) {
  DCHECK_EQ(kField, details.location());
  DCHECK_EQ(kData, details.kind());
  DisallowHeapAllocation no_gc;
  FieldIndex index = FieldIndex::ForDescriptor(map(), descriptor);
  if (details.representation().IsDouble()) {
    // Nothing more to be done.
    if (value->IsUninitialized(this->GetIsolate())) {
      return;
    }
    // Manipulating the signaling NaN used for the hole and uninitialized
    // double field sentinel in C++, e.g. with bit_cast or value()/set_value(),
    // will change its value on ia32 (the x87 stack is used to return values
    // and stores to the stack silently clear the signalling bit).
    uint64_t bits;
    if (value->IsSmi()) {
      bits = bit_cast<uint64_t>(static_cast<double>(Smi::cast(value)->value()));
    } else {
      DCHECK(value->IsHeapNumber());
      bits = HeapNumber::cast(value)->value_as_bits();
    }
    if (IsUnboxedDoubleField(index)) {
      RawFastDoublePropertyAsBitsAtPut(index, bits);
    } else {
      HeapNumber* box = HeapNumber::cast(RawFastPropertyAt(index));
      DCHECK(box->IsMutableHeapNumber());
      box->set_value_as_bits(bits);
    }
  } else {
    RawFastPropertyAtPut(index, value);
  }
}

int JSObject::GetInObjectPropertyOffset(int index) {
  return map()->GetInObjectPropertyOffset(index);
}


Object* JSObject::InObjectPropertyAt(int index) {
  int offset = GetInObjectPropertyOffset(index);
  return READ_FIELD(this, offset);
}


Object* JSObject::InObjectPropertyAtPut(int index,
                                        Object* value,
                                        WriteBarrierMode mode) {
  // Adjust for the number of properties stored in the object.
  int offset = GetInObjectPropertyOffset(index);
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);
  return value;
}


void JSObject::InitializeBody(Map* map, int start_offset,
                              Object* pre_allocated_value,
                              Object* filler_value) {
  DCHECK(!filler_value->IsHeapObject() ||
         !GetHeap()->InNewSpace(filler_value));
  DCHECK(!pre_allocated_value->IsHeapObject() ||
         !GetHeap()->InNewSpace(pre_allocated_value));
  int size = map->instance_size();
  int offset = start_offset;
  if (filler_value != pre_allocated_value) {
    int end_of_pre_allocated_offset =
        size - (map->unused_property_fields() * kPointerSize);
    DCHECK_LE(kHeaderSize, end_of_pre_allocated_offset);
    while (offset < end_of_pre_allocated_offset) {
      WRITE_FIELD(this, offset, pre_allocated_value);
      offset += kPointerSize;
    }
  }
  while (offset < size) {
    WRITE_FIELD(this, offset, filler_value);
    offset += kPointerSize;
  }
}


bool Map::TooManyFastProperties(StoreFromKeyed store_mode) {
  if (unused_property_fields() != 0) return false;
  if (is_prototype_map()) return false;
  int minimum = store_mode == CERTAINLY_NOT_STORE_FROM_KEYED ? 128 : 12;
  int limit = Max(minimum, GetInObjectProperties());
  int external = NumberOfFields() - GetInObjectProperties();
  return external > limit;
}


void Struct::InitializeBody(int object_size) {
  Object* value = GetHeap()->undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}

bool Object::ToArrayLength(uint32_t* index) { return Object::ToUint32(index); }


bool Object::ToArrayIndex(uint32_t* index) {
  return Object::ToUint32(index) && *index != kMaxUInt32;
}


void Object::VerifyApiCallResultType() {
#if DEBUG
  if (IsSmi()) return;
  DCHECK(IsHeapObject());
  Isolate* isolate = HeapObject::cast(this)->GetIsolate();
  if (!(IsString() || IsSymbol() || IsJSReceiver() || IsHeapNumber() ||
        IsUndefined(isolate) || IsTrue(isolate) || IsFalse(isolate) ||
        IsNull(isolate))) {
    FATAL("API call returned invalid object");
  }
#endif  // DEBUG
}


Object* FixedArray::get(int index) const {
  SLOW_DCHECK(index >= 0 && index < this->length());
  return NOBARRIER_READ_FIELD(this, kHeaderSize + index * kPointerSize);
}

Handle<Object> FixedArray::get(FixedArray* array, int index, Isolate* isolate) {
  return handle(array->get(index), isolate);
}

template <class T>
MaybeHandle<T> FixedArray::GetValue(Isolate* isolate, int index) const {
  Object* obj = get(index);
  if (obj->IsUndefined(isolate)) return MaybeHandle<T>();
  return Handle<T>(T::cast(obj), isolate);
}

template <class T>
Handle<T> FixedArray::GetValueChecked(Isolate* isolate, int index) const {
  Object* obj = get(index);
  CHECK(!obj->IsUndefined(isolate));
  return Handle<T>(T::cast(obj), isolate);
}
bool FixedArray::is_the_hole(Isolate* isolate, int index) {
  return get(index)->IsTheHole(isolate);
}

void FixedArray::set(int index, Smi* value) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map());
  DCHECK(index >= 0 && index < this->length());
  DCHECK(reinterpret_cast<Object*>(value)->IsSmi());
  int offset = kHeaderSize + index * kPointerSize;
  NOBARRIER_WRITE_FIELD(this, offset, value);
}


void FixedArray::set(int index, Object* value) {
  DCHECK_NE(GetHeap()->fixed_cow_array_map(), map());
  DCHECK(IsFixedArray());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kPointerSize;
  NOBARRIER_WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(GetHeap(), this, offset, value);
}


double FixedDoubleArray::get_scalar(int index) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  DCHECK(index >= 0 && index < this->length());
  DCHECK(!is_the_hole(index));
  return READ_DOUBLE_FIELD(this, kHeaderSize + index * kDoubleSize);
}


uint64_t FixedDoubleArray::get_representation(int index) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  DCHECK(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kDoubleSize;
  return READ_UINT64_FIELD(this, offset);
}

Handle<Object> FixedDoubleArray::get(FixedDoubleArray* array, int index,
                                     Isolate* isolate) {
  if (array->is_the_hole(index)) {
    return isolate->factory()->the_hole_value();
  } else {
    return isolate->factory()->NewNumber(array->get_scalar(index));
  }
}


void FixedDoubleArray::set(int index, double value) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  if (std::isnan(value)) {
    WRITE_DOUBLE_FIELD(this, offset, std::numeric_limits<double>::quiet_NaN());
  } else {
    WRITE_DOUBLE_FIELD(this, offset, value);
  }
  DCHECK(!is_the_hole(index));
}

void FixedDoubleArray::set_the_hole(Isolate* isolate, int index) {
  set_the_hole(index);
}

void FixedDoubleArray::set_the_hole(int index) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  WRITE_UINT64_FIELD(this, offset, kHoleNanInt64);
}

bool FixedDoubleArray::is_the_hole(Isolate* isolate, int index) {
  return is_the_hole(index);
}

bool FixedDoubleArray::is_the_hole(int index) {
  return get_representation(index) == kHoleNanInt64;
}


double* FixedDoubleArray::data_start() {
  return reinterpret_cast<double*>(FIELD_ADDR(this, kHeaderSize));
}


void FixedDoubleArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}


Object* WeakFixedArray::Get(int index) const {
  Object* raw = FixedArray::cast(this)->get(index + kFirstIndex);
  if (raw->IsSmi()) return raw;
  DCHECK(raw->IsWeakCell());
  return WeakCell::cast(raw)->value();
}


bool WeakFixedArray::IsEmptySlot(int index) const {
  DCHECK(index < Length());
  return Get(index)->IsSmi();
}


void WeakFixedArray::Clear(int index) {
  FixedArray::cast(this)->set(index + kFirstIndex, Smi::kZero);
}


int WeakFixedArray::Length() const {
  return FixedArray::cast(this)->length() - kFirstIndex;
}


int WeakFixedArray::last_used_index() const {
  return Smi::cast(FixedArray::cast(this)->get(kLastUsedIndexIndex))->value();
}


void WeakFixedArray::set_last_used_index(int index) {
  FixedArray::cast(this)->set(kLastUsedIndexIndex, Smi::FromInt(index));
}


template <class T>
T* WeakFixedArray::Iterator::Next() {
  if (list_ != NULL) {
    // Assert that list did not change during iteration.
    DCHECK_EQ(last_used_index_, list_->last_used_index());
    while (index_ < list_->Length()) {
      Object* item = list_->Get(index_++);
      if (item != Empty()) return T::cast(item);
    }
    list_ = NULL;
  }
  return NULL;
}

int ArrayList::Length() const {
  if (FixedArray::cast(this)->length() == 0) return 0;
  return Smi::cast(FixedArray::cast(this)->get(kLengthIndex))->value();
}


void ArrayList::SetLength(int length) {
  return FixedArray::cast(this)->set(kLengthIndex, Smi::FromInt(length));
}

Object* ArrayList::Get(int index) const {
  return FixedArray::cast(this)->get(kFirstIndex + index);
}


Object** ArrayList::Slot(int index) {
  return data_start() + kFirstIndex + index;
}

void ArrayList::Set(int index, Object* obj, WriteBarrierMode mode) {
  FixedArray::cast(this)->set(kFirstIndex + index, obj, mode);
}


void ArrayList::Clear(int index, Object* undefined) {
  DCHECK(undefined->IsUndefined(GetIsolate()));
  FixedArray::cast(this)
      ->set(kFirstIndex + index, undefined, SKIP_WRITE_BARRIER);
}

int RegExpMatchInfo::NumberOfCaptureRegisters() {
  DCHECK_GE(length(), kLastMatchOverhead);
  Object* obj = get(kNumberOfCapturesIndex);
  return Smi::cast(obj)->value();
}

void RegExpMatchInfo::SetNumberOfCaptureRegisters(int value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kNumberOfCapturesIndex, Smi::FromInt(value));
}

String* RegExpMatchInfo::LastSubject() {
  DCHECK_GE(length(), kLastMatchOverhead);
  Object* obj = get(kLastSubjectIndex);
  return String::cast(obj);
}

void RegExpMatchInfo::SetLastSubject(String* value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kLastSubjectIndex, value);
}

Object* RegExpMatchInfo::LastInput() {
  DCHECK_GE(length(), kLastMatchOverhead);
  return get(kLastInputIndex);
}

void RegExpMatchInfo::SetLastInput(Object* value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kLastInputIndex, value);
}

int RegExpMatchInfo::Capture(int i) {
  DCHECK_LT(i, NumberOfCaptureRegisters());
  Object* obj = get(kFirstCaptureIndex + i);
  return Smi::cast(obj)->value();
}

void RegExpMatchInfo::SetCapture(int i, int value) {
  DCHECK_LT(i, NumberOfCaptureRegisters());
  set(kFirstCaptureIndex + i, Smi::FromInt(value));
}

WriteBarrierMode HeapObject::GetWriteBarrierMode(
    const DisallowHeapAllocation& promise) {
  Heap* heap = GetHeap();
  if (heap->incremental_marking()->IsMarking()) return UPDATE_WRITE_BARRIER;
  if (heap->InNewSpace(this)) return SKIP_WRITE_BARRIER;
  return UPDATE_WRITE_BARRIER;
}


AllocationAlignment HeapObject::RequiredAlignment() {
#ifdef V8_HOST_ARCH_32_BIT
  if ((IsFixedFloat64Array() || IsFixedDoubleArray()) &&
      FixedArrayBase::cast(this)->length() != 0) {
    return kDoubleAligned;
  }
  if (IsHeapNumber()) return kDoubleUnaligned;
#endif  // V8_HOST_ARCH_32_BIT
  return kWordAligned;
}


void FixedArray::set(int index,
                     Object* value,
                     WriteBarrierMode mode) {
  DCHECK_NE(map(), GetHeap()->fixed_cow_array_map());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kPointerSize;
  NOBARRIER_WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);
}


void FixedArray::NoWriteBarrierSet(FixedArray* array,
                                   int index,
                                   Object* value) {
  DCHECK_NE(array->map(), array->GetHeap()->fixed_cow_array_map());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, array->length());
  DCHECK(!array->GetHeap()->InNewSpace(value));
  NOBARRIER_WRITE_FIELD(array, kHeaderSize + index * kPointerSize, value);
}

void FixedArray::set_undefined(int index) {
  set_undefined(GetIsolate(), index);
}

void FixedArray::set_undefined(Isolate* isolate, int index) {
  FixedArray::NoWriteBarrierSet(this, index,
                                isolate->heap()->undefined_value());
}

void FixedArray::set_null(int index) { set_null(GetIsolate(), index); }

void FixedArray::set_null(Isolate* isolate, int index) {
  FixedArray::NoWriteBarrierSet(this, index, isolate->heap()->null_value());
}

void FixedArray::set_the_hole(int index) { set_the_hole(GetIsolate(), index); }

void FixedArray::set_the_hole(Isolate* isolate, int index) {
  FixedArray::NoWriteBarrierSet(this, index, isolate->heap()->the_hole_value());
}

void FixedArray::FillWithHoles(int from, int to) {
  Isolate* isolate = GetIsolate();
  for (int i = from; i < to; i++) {
    set_the_hole(isolate, i);
  }
}


Object** FixedArray::data_start() {
  return HeapObject::RawField(this, kHeaderSize);
}


Object** FixedArray::RawFieldOfElementAt(int index) {
  return HeapObject::RawField(this, OffsetOfElementAt(index));
}

bool DescriptorArray::IsEmpty() {
  DCHECK(length() >= kFirstIndex ||
         this == GetHeap()->empty_descriptor_array());
  return length() < kFirstIndex;
}


int DescriptorArray::number_of_descriptors() {
  DCHECK(length() >= kFirstIndex || IsEmpty());
  int len = length();
  return len == 0 ? 0 : Smi::cast(get(kDescriptorLengthIndex))->value();
}


int DescriptorArray::number_of_descriptors_storage() {
  int len = length();
  return len == 0 ? 0 : (len - kFirstIndex) / kEntrySize;
}


int DescriptorArray::NumberOfSlackDescriptors() {
  return number_of_descriptors_storage() - number_of_descriptors();
}


void DescriptorArray::SetNumberOfDescriptors(int number_of_descriptors) {
  WRITE_FIELD(
      this, kDescriptorLengthOffset, Smi::FromInt(number_of_descriptors));
}


inline int DescriptorArray::number_of_entries() {
  return number_of_descriptors();
}


bool DescriptorArray::HasEnumCache() {
  return !IsEmpty() && !get(kEnumCacheBridgeIndex)->IsSmi();
}


void DescriptorArray::CopyEnumCacheFrom(DescriptorArray* array) {
  set(kEnumCacheBridgeIndex, array->get(kEnumCacheBridgeIndex));
}


FixedArray* DescriptorArray::GetEnumCache() {
  DCHECK(HasEnumCache());
  FixedArray* bridge = FixedArray::cast(get(kEnumCacheBridgeIndex));
  return FixedArray::cast(bridge->get(kEnumCacheBridgeCacheIndex));
}


bool DescriptorArray::HasEnumIndicesCache() {
  if (IsEmpty()) return false;
  Object* object = get(kEnumCacheBridgeIndex);
  if (object->IsSmi()) return false;
  FixedArray* bridge = FixedArray::cast(object);
  return !bridge->get(kEnumCacheBridgeIndicesCacheIndex)->IsSmi();
}


FixedArray* DescriptorArray::GetEnumIndicesCache() {
  DCHECK(HasEnumIndicesCache());
  FixedArray* bridge = FixedArray::cast(get(kEnumCacheBridgeIndex));
  return FixedArray::cast(bridge->get(kEnumCacheBridgeIndicesCacheIndex));
}


// Perform a binary search in a fixed array.
template <SearchMode search_mode, typename T>
int BinarySearch(T* array, Name* name, int valid_entries,
                 int* out_insertion_index) {
  DCHECK(search_mode == ALL_ENTRIES || out_insertion_index == NULL);
  int low = 0;
  int high = array->number_of_entries() - 1;
  uint32_t hash = name->hash_field();
  int limit = high;

  DCHECK(low <= high);

  while (low != high) {
    int mid = low + (high - low) / 2;
    Name* mid_name = array->GetSortedKey(mid);
    uint32_t mid_hash = mid_name->hash_field();

    if (mid_hash >= hash) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }

  for (; low <= limit; ++low) {
    int sort_index = array->GetSortedKeyIndex(low);
    Name* entry = array->GetKey(sort_index);
    uint32_t current_hash = entry->hash_field();
    if (current_hash != hash) {
      if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
        *out_insertion_index = sort_index + (current_hash > hash ? 0 : 1);
      }
      return T::kNotFound;
    }
    if (entry == name) {
      if (search_mode == ALL_ENTRIES || sort_index < valid_entries) {
        return sort_index;
      }
      return T::kNotFound;
    }
  }

  if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
    *out_insertion_index = limit + 1;
  }
  return T::kNotFound;
}


// Perform a linear search in this fixed array. len is the number of entry
// indices that are valid.
template <SearchMode search_mode, typename T>
int LinearSearch(T* array, Name* name, int valid_entries,
                 int* out_insertion_index) {
  if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
    uint32_t hash = name->hash_field();
    int len = array->number_of_entries();
    for (int number = 0; number < len; number++) {
      int sorted_index = array->GetSortedKeyIndex(number);
      Name* entry = array->GetKey(sorted_index);
      uint32_t current_hash = entry->hash_field();
      if (current_hash > hash) {
        *out_insertion_index = sorted_index;
        return T::kNotFound;
      }
      if (entry == name) return sorted_index;
    }
    *out_insertion_index = len;
    return T::kNotFound;
  } else {
    DCHECK_LE(valid_entries, array->number_of_entries());
    DCHECK_NULL(out_insertion_index);  // Not supported here.
    for (int number = 0; number < valid_entries; number++) {
      if (array->GetKey(number) == name) return number;
    }
    return T::kNotFound;
  }
}


template <SearchMode search_mode, typename T>
int Search(T* array, Name* name, int valid_entries, int* out_insertion_index) {
  SLOW_DCHECK(array->IsSortedNoDuplicates());

  if (valid_entries == 0) {
    if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
      *out_insertion_index = 0;
    }
    return T::kNotFound;
  }

  // Fast case: do linear search for small arrays.
  const int kMaxElementsForLinearSearch = 8;
  if (valid_entries <= kMaxElementsForLinearSearch) {
    return LinearSearch<search_mode>(array, name, valid_entries,
                                     out_insertion_index);
  }

  // Slow case: perform binary search.
  return BinarySearch<search_mode>(array, name, valid_entries,
                                   out_insertion_index);
}


int DescriptorArray::Search(Name* name, int valid_descriptors) {
  DCHECK(name->IsUniqueName());
  return internal::Search<VALID_ENTRIES>(this, name, valid_descriptors, NULL);
}

int DescriptorArray::SearchWithCache(Isolate* isolate, Name* name, Map* map) {
  DCHECK(name->IsUniqueName());
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return kNotFound;

  DescriptorLookupCache* cache = isolate->descriptor_lookup_cache();
  int number = cache->Lookup(map, name);

  if (number == DescriptorLookupCache::kAbsent) {
    number = Search(name, number_of_own_descriptors);
    cache->Update(map, name, number);
  }

  return number;
}

PropertyDetails Map::GetLastDescriptorDetails() {
  return instance_descriptors()->GetDetails(LastAdded());
}


int Map::LastAdded() {
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DCHECK(number_of_own_descriptors > 0);
  return number_of_own_descriptors - 1;
}


int Map::NumberOfOwnDescriptors() {
  return NumberOfOwnDescriptorsBits::decode(bit_field3());
}


void Map::SetNumberOfOwnDescriptors(int number) {
  DCHECK(number <= instance_descriptors()->number_of_descriptors());
  set_bit_field3(NumberOfOwnDescriptorsBits::update(bit_field3(), number));
}


int Map::EnumLength() { return EnumLengthBits::decode(bit_field3()); }


void Map::SetEnumLength(int length) {
  if (length != kInvalidEnumCacheSentinel) {
    DCHECK(length >= 0);
    DCHECK(length == 0 || instance_descriptors()->HasEnumCache());
    DCHECK(length <= NumberOfOwnDescriptors());
  }
  set_bit_field3(EnumLengthBits::update(bit_field3(), length));
}


FixedArrayBase* Map::GetInitialElements() {
  FixedArrayBase* result = nullptr;
  if (has_fast_elements() || has_fast_string_wrapper_elements()) {
    result = GetHeap()->empty_fixed_array();
  } else if (has_fast_sloppy_arguments_elements()) {
    result = GetHeap()->empty_sloppy_arguments_elements();
  } else if (has_fixed_typed_array_elements()) {
    result = GetHeap()->EmptyFixedTypedArrayForMap(this);
  } else {
    UNREACHABLE();
  }
  DCHECK(!GetHeap()->InNewSpace(result));
  return result;
}

Object** DescriptorArray::GetKeySlot(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  return RawFieldOfElementAt(ToKeyIndex(descriptor_number));
}


Object** DescriptorArray::GetDescriptorStartSlot(int descriptor_number) {
  return GetKeySlot(descriptor_number);
}


Object** DescriptorArray::GetDescriptorEndSlot(int descriptor_number) {
  return GetValueSlot(descriptor_number - 1) + 1;
}


Name* DescriptorArray::GetKey(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  return Name::cast(get(ToKeyIndex(descriptor_number)));
}


int DescriptorArray::GetSortedKeyIndex(int descriptor_number) {
  return GetDetails(descriptor_number).pointer();
}


Name* DescriptorArray::GetSortedKey(int descriptor_number) {
  return GetKey(GetSortedKeyIndex(descriptor_number));
}


void DescriptorArray::SetSortedKey(int descriptor_index, int pointer) {
  PropertyDetails details = GetDetails(descriptor_index);
  set(ToDetailsIndex(descriptor_index), details.set_pointer(pointer).AsSmi());
}


Object** DescriptorArray::GetValueSlot(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  return RawFieldOfElementAt(ToValueIndex(descriptor_number));
}


int DescriptorArray::GetValueOffset(int descriptor_number) {
  return OffsetOfElementAt(ToValueIndex(descriptor_number));
}


Object* DescriptorArray::GetValue(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  return get(ToValueIndex(descriptor_number));
}


void DescriptorArray::SetValue(int descriptor_index, Object* value) {
  set(ToValueIndex(descriptor_index), value);
}


PropertyDetails DescriptorArray::GetDetails(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  Object* details = get(ToDetailsIndex(descriptor_number));
  return PropertyDetails(Smi::cast(details));
}

int DescriptorArray::GetFieldIndex(int descriptor_number) {
  DCHECK(GetDetails(descriptor_number).location() == kField);
  return GetDetails(descriptor_number).field_index();
}

FieldType* DescriptorArray::GetFieldType(int descriptor_number) {
  DCHECK(GetDetails(descriptor_number).location() == kField);
  Object* wrapped_type = GetValue(descriptor_number);
  return Map::UnwrapFieldType(wrapped_type);
}

void DescriptorArray::Get(int descriptor_number, Descriptor* desc) {
  desc->Init(handle(GetKey(descriptor_number), GetIsolate()),
             handle(GetValue(descriptor_number), GetIsolate()),
             GetDetails(descriptor_number));
}

void DescriptorArray::Set(int descriptor_number, Name* key, Object* value,
                          PropertyDetails details) {
  // Range check.
  DCHECK(descriptor_number < number_of_descriptors());
  set(ToKeyIndex(descriptor_number), key);
  set(ToValueIndex(descriptor_number), value);
  set(ToDetailsIndex(descriptor_number), details.AsSmi());
}

void DescriptorArray::Set(int descriptor_number, Descriptor* desc) {
  Name* key = *desc->GetKey();
  Object* value = *desc->GetValue();
  Set(descriptor_number, key, value, desc->GetDetails());
}


void DescriptorArray::Append(Descriptor* desc) {
  DisallowHeapAllocation no_gc;
  int descriptor_number = number_of_descriptors();
  SetNumberOfDescriptors(descriptor_number + 1);
  Set(descriptor_number, desc);

  uint32_t hash = desc->GetKey()->Hash();

  int insertion;

  for (insertion = descriptor_number; insertion > 0; --insertion) {
    Name* key = GetSortedKey(insertion - 1);
    if (key->Hash() <= hash) break;
    SetSortedKey(insertion, GetSortedKeyIndex(insertion - 1));
  }

  SetSortedKey(insertion, descriptor_number);
}


void DescriptorArray::SwapSortedKeys(int first, int second) {
  int first_key = GetSortedKeyIndex(first);
  SetSortedKey(first, GetSortedKeyIndex(second));
  SetSortedKey(second, first_key);
}


int HashTableBase::NumberOfElements() {
  return Smi::cast(get(kNumberOfElementsIndex))->value();
}


int HashTableBase::NumberOfDeletedElements() {
  return Smi::cast(get(kNumberOfDeletedElementsIndex))->value();
}


int HashTableBase::Capacity() {
  return Smi::cast(get(kCapacityIndex))->value();
}


void HashTableBase::ElementAdded() {
  SetNumberOfElements(NumberOfElements() + 1);
}


void HashTableBase::ElementRemoved() {
  SetNumberOfElements(NumberOfElements() - 1);
  SetNumberOfDeletedElements(NumberOfDeletedElements() + 1);
}


void HashTableBase::ElementsRemoved(int n) {
  SetNumberOfElements(NumberOfElements() - n);
  SetNumberOfDeletedElements(NumberOfDeletedElements() + n);
}


// static
int HashTableBase::ComputeCapacity(int at_least_space_for) {
  // Add 50% slack to make slot collisions sufficiently unlikely.
  // See matching computation in HashTable::HasSufficientCapacityToAdd().
  // Must be kept in sync with CodeStubAssembler::HashTableComputeCapacity().
  int raw_cap = at_least_space_for + (at_least_space_for >> 1);
  int capacity = base::bits::RoundUpToPowerOfTwo32(raw_cap);
  return Max(capacity, kMinCapacity);
}

bool HashTableBase::IsKey(Isolate* isolate, Object* k) {
  Heap* heap = isolate->heap();
  return k != heap->the_hole_value() && k != heap->undefined_value();
}

void HashTableBase::SetNumberOfElements(int nof) {
  set(kNumberOfElementsIndex, Smi::FromInt(nof));
}


void HashTableBase::SetNumberOfDeletedElements(int nod) {
  set(kNumberOfDeletedElementsIndex, Smi::FromInt(nod));
}

template <typename Key>
Map* BaseShape<Key>::GetMap(Isolate* isolate) {
  return isolate->heap()->hash_table_map();
}

template <typename Derived, typename Shape, typename Key>
int HashTable<Derived, Shape, Key>::FindEntry(Key key) {
  return FindEntry(GetIsolate(), key);
}


template<typename Derived, typename Shape, typename Key>
int HashTable<Derived, Shape, Key>::FindEntry(Isolate* isolate, Key key) {
  return FindEntry(isolate, key, HashTable::Hash(key));
}

// Find entry for key otherwise return kNotFound.
template <typename Derived, typename Shape, typename Key>
int HashTable<Derived, Shape, Key>::FindEntry(Isolate* isolate, Key key,
                                              int32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  Object* undefined = isolate->heap()->undefined_value();
  Object* the_hole = isolate->heap()->the_hole_value();
  while (true) {
    Object* element = KeyAt(entry);
    // Empty entry. Uses raw unchecked accessors because it is called by the
    // string table during bootstrapping.
    if (element == undefined) break;
    if (element != the_hole && Shape::IsMatch(key, element)) return entry;
    entry = NextProbe(entry, count++, capacity);
  }
  return kNotFound;
}

template <typename Derived, typename Shape, typename Key>
bool HashTable<Derived, Shape, Key>::Has(Key key) {
  return FindEntry(key) != kNotFound;
}

template <typename Derived, typename Shape, typename Key>
bool HashTable<Derived, Shape, Key>::Has(Isolate* isolate, Key key) {
  return FindEntry(isolate, key) != kNotFound;
}

bool ObjectHashSet::Has(Isolate* isolate, Handle<Object> key, int32_t hash) {
  return FindEntry(isolate, key, hash) != kNotFound;
}

bool ObjectHashSet::Has(Isolate* isolate, Handle<Object> key) {
  Object* hash = key->GetHash();
  if (!hash->IsSmi()) return false;
  return FindEntry(isolate, key, Smi::cast(hash)->value()) != kNotFound;
}

bool StringSetShape::IsMatch(String* key, Object* value) {
  return value->IsString() && key->Equals(String::cast(value));
}

uint32_t StringSetShape::Hash(String* key) { return key->Hash(); }

uint32_t StringSetShape::HashForObject(String* key, Object* object) {
  return object->IsString() ? String::cast(object)->Hash() : 0;
}

bool SeededNumberDictionary::requires_slow_elements() {
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return false;
  return 0 !=
      (Smi::cast(max_index_object)->value() & kRequiresSlowElementsMask);
}


uint32_t SeededNumberDictionary::max_number_key() {
  DCHECK(!requires_slow_elements());
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return 0;
  uint32_t value = static_cast<uint32_t>(Smi::cast(max_index_object)->value());
  return value >> kRequiresSlowElementsTagSize;
}


void SeededNumberDictionary::set_requires_slow_elements() {
  set(kMaxNumberKeyIndex, Smi::FromInt(kRequiresSlowElementsMask));
}


template <class T>
PodArray<T>* PodArray<T>::cast(Object* object) {
  SLOW_DCHECK(object->IsByteArray());
  return reinterpret_cast<PodArray<T>*>(object);
}
template <class T>
const PodArray<T>* PodArray<T>::cast(const Object* object) {
  SLOW_DCHECK(object->IsByteArray());
  return reinterpret_cast<const PodArray<T>*>(object);
}

// static
template <class T>
Handle<PodArray<T>> PodArray<T>::New(Isolate* isolate, int length,
                                     PretenureFlag pretenure) {
  return Handle<PodArray<T>>::cast(
      isolate->factory()->NewByteArray(length * sizeof(T), pretenure));
}

// static
template <class Traits>
STATIC_CONST_MEMBER_DEFINITION const InstanceType
    FixedTypedArray<Traits>::kInstanceType;


template <class Traits>
FixedTypedArray<Traits>* FixedTypedArray<Traits>::cast(Object* object) {
  SLOW_DCHECK(object->IsHeapObject() &&
              HeapObject::cast(object)->map()->instance_type() ==
              Traits::kInstanceType);
  return reinterpret_cast<FixedTypedArray<Traits>*>(object);
}


template <class Traits>
const FixedTypedArray<Traits>*
FixedTypedArray<Traits>::cast(const Object* object) {
  SLOW_DCHECK(object->IsHeapObject() &&
              HeapObject::cast(object)->map()->instance_type() ==
              Traits::kInstanceType);
  return reinterpret_cast<FixedTypedArray<Traits>*>(object);
}


#define DEFINE_DEOPT_ELEMENT_ACCESSORS(name, type)       \
  type* DeoptimizationInputData::name() {                \
    return type::cast(get(k##name##Index));              \
  }                                                      \
  void DeoptimizationInputData::Set##name(type* value) { \
    set(k##name##Index, value);                          \
  }

DEFINE_DEOPT_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrAstId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OptimizationId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)
DEFINE_DEOPT_ELEMENT_ACCESSORS(WeakCellCache, Object)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)

#undef DEFINE_DEOPT_ELEMENT_ACCESSORS


#define DEFINE_DEOPT_ENTRY_ACCESSORS(name, type)                \
  type* DeoptimizationInputData::name(int i) {                  \
    return type::cast(get(IndexForEntry(i) + k##name##Offset)); \
  }                                                             \
  void DeoptimizationInputData::Set##name(int i, type* value) { \
    set(IndexForEntry(i) + k##name##Offset, value);             \
  }

DEFINE_DEOPT_ENTRY_ACCESSORS(AstIdRaw, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(TranslationIndex, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(ArgumentsStackHeight, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(Pc, Smi)

#undef DEFINE_DEOPT_ENTRY_ACCESSORS


BailoutId DeoptimizationInputData::AstId(int i) {
  return BailoutId(AstIdRaw(i)->value());
}


void DeoptimizationInputData::SetAstId(int i, BailoutId value) {
  SetAstIdRaw(i, Smi::FromInt(value.ToInt()));
}


int DeoptimizationInputData::DeoptCount() {
  return (length() - kFirstDeoptEntryIndex) / kDeoptEntrySize;
}


int DeoptimizationOutputData::DeoptPoints() { return length() / 2; }


BailoutId DeoptimizationOutputData::AstId(int index) {
  return BailoutId(Smi::cast(get(index * 2))->value());
}


void DeoptimizationOutputData::SetAstId(int index, BailoutId id) {
  set(index * 2, Smi::FromInt(id.ToInt()));
}


Smi* DeoptimizationOutputData::PcAndState(int index) {
  return Smi::cast(get(1 + index * 2));
}


void DeoptimizationOutputData::SetPcAndState(int index, Smi* offset) {
  set(1 + index * 2, offset);
}

int HandlerTable::GetRangeStart(int index) const {
  return Smi::cast(get(index * kRangeEntrySize + kRangeStartIndex))->value();
}

int HandlerTable::GetRangeEnd(int index) const {
  return Smi::cast(get(index * kRangeEntrySize + kRangeEndIndex))->value();
}

int HandlerTable::GetRangeHandler(int index) const {
  return HandlerOffsetField::decode(
      Smi::cast(get(index * kRangeEntrySize + kRangeHandlerIndex))->value());
}

int HandlerTable::GetRangeData(int index) const {
  return Smi::cast(get(index * kRangeEntrySize + kRangeDataIndex))->value();
}

void HandlerTable::SetRangeStart(int index, int value) {
  set(index * kRangeEntrySize + kRangeStartIndex, Smi::FromInt(value));
}


void HandlerTable::SetRangeEnd(int index, int value) {
  set(index * kRangeEntrySize + kRangeEndIndex, Smi::FromInt(value));
}


void HandlerTable::SetRangeHandler(int index, int offset,
                                   CatchPrediction prediction) {
  int value = HandlerOffsetField::encode(offset) |
              HandlerPredictionField::encode(prediction);
  set(index * kRangeEntrySize + kRangeHandlerIndex, Smi::FromInt(value));
}

void HandlerTable::SetRangeData(int index, int value) {
  set(index * kRangeEntrySize + kRangeDataIndex, Smi::FromInt(value));
}


void HandlerTable::SetReturnOffset(int index, int value) {
  set(index * kReturnEntrySize + kReturnOffsetIndex, Smi::FromInt(value));
}

void HandlerTable::SetReturnHandler(int index, int offset) {
  int value = HandlerOffsetField::encode(offset);
  set(index * kReturnEntrySize + kReturnHandlerIndex, Smi::FromInt(value));
}

int HandlerTable::NumberOfRangeEntries() const {
  return length() / kRangeEntrySize;
}

template <typename Derived, typename Shape, typename Key>
HashTable<Derived, Shape, Key>*
HashTable<Derived, Shape, Key>::cast(Object* obj) {
  SLOW_DCHECK(obj->IsHashTable());
  return reinterpret_cast<HashTable*>(obj);
}


template <typename Derived, typename Shape, typename Key>
const HashTable<Derived, Shape, Key>*
HashTable<Derived, Shape, Key>::cast(const Object* obj) {
  SLOW_DCHECK(obj->IsHashTable());
  return reinterpret_cast<const HashTable*>(obj);
}


SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)

SMI_ACCESSORS(FreeSpace, size, kSizeOffset)
NOBARRIER_SMI_ACCESSORS(FreeSpace, size, kSizeOffset)

SMI_ACCESSORS(String, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(String, length, kLengthOffset)


int FreeSpace::Size() { return size(); }


FreeSpace* FreeSpace::next() {
  DCHECK(map() == GetHeap()->root(Heap::kFreeSpaceMapRootIndex) ||
         (!GetHeap()->deserialization_complete() && map() == NULL));
  DCHECK_LE(kNextOffset + kPointerSize, nobarrier_size());
  return reinterpret_cast<FreeSpace*>(
      Memory::Address_at(address() + kNextOffset));
}


void FreeSpace::set_next(FreeSpace* next) {
  DCHECK(map() == GetHeap()->root(Heap::kFreeSpaceMapRootIndex) ||
         (!GetHeap()->deserialization_complete() && map() == NULL));
  DCHECK_LE(kNextOffset + kPointerSize, nobarrier_size());
  base::NoBarrier_Store(
      reinterpret_cast<base::AtomicWord*>(address() + kNextOffset),
      reinterpret_cast<base::AtomicWord>(next));
}


FreeSpace* FreeSpace::cast(HeapObject* o) {
  SLOW_DCHECK(!o->GetHeap()->deserialization_complete() || o->IsFreeSpace());
  return reinterpret_cast<FreeSpace*>(o);
}


uint32_t Name::hash_field() {
  return READ_UINT32_FIELD(this, kHashFieldOffset);
}


void Name::set_hash_field(uint32_t value) {
  WRITE_UINT32_FIELD(this, kHashFieldOffset, value);
#if V8_HOST_ARCH_64_BIT
#if V8_TARGET_LITTLE_ENDIAN
  WRITE_UINT32_FIELD(this, kHashFieldSlot + kIntSize, 0);
#else
  WRITE_UINT32_FIELD(this, kHashFieldSlot, 0);
#endif
#endif
}


bool Name::Equals(Name* other) {
  if (other == this) return true;
  if ((this->IsInternalizedString() && other->IsInternalizedString()) ||
      this->IsSymbol() || other->IsSymbol()) {
    return false;
  }
  return String::cast(this)->SlowEquals(String::cast(other));
}


bool Name::Equals(Handle<Name> one, Handle<Name> two) {
  if (one.is_identical_to(two)) return true;
  if ((one->IsInternalizedString() && two->IsInternalizedString()) ||
      one->IsSymbol() || two->IsSymbol()) {
    return false;
  }
  return String::SlowEquals(Handle<String>::cast(one),
                            Handle<String>::cast(two));
}


ACCESSORS(Symbol, name, Object, kNameOffset)
SMI_ACCESSORS(Symbol, flags, kFlagsOffset)
BOOL_ACCESSORS(Symbol, flags, is_private, kPrivateBit)
BOOL_ACCESSORS(Symbol, flags, is_well_known_symbol, kWellKnownSymbolBit)
BOOL_ACCESSORS(Symbol, flags, is_public, kPublicBit)

bool String::Equals(String* other) {
  if (other == this) return true;
  if (this->IsInternalizedString() && other->IsInternalizedString()) {
    return false;
  }
  return SlowEquals(other);
}


bool String::Equals(Handle<String> one, Handle<String> two) {
  if (one.is_identical_to(two)) return true;
  if (one->IsInternalizedString() && two->IsInternalizedString()) {
    return false;
  }
  return SlowEquals(one, two);
}


Handle<String> String::Flatten(Handle<String> string, PretenureFlag pretenure) {
  if (string->IsConsString()) {
    Handle<ConsString> cons = Handle<ConsString>::cast(string);
    if (cons->IsFlat()) {
      string = handle(cons->first());
    } else {
      return SlowFlatten(cons, pretenure);
    }
  }
  if (string->IsThinString()) {
    string = handle(Handle<ThinString>::cast(string)->actual());
    DCHECK(!string->IsConsString());
  }
  return string;
}


uint16_t String::Get(int index) {
  DCHECK(index >= 0 && index < length());
  switch (StringShape(this).full_representation_tag()) {
    case kSeqStringTag | kOneByteStringTag:
      return SeqOneByteString::cast(this)->SeqOneByteStringGet(index);
    case kSeqStringTag | kTwoByteStringTag:
      return SeqTwoByteString::cast(this)->SeqTwoByteStringGet(index);
    case kConsStringTag | kOneByteStringTag:
    case kConsStringTag | kTwoByteStringTag:
      return ConsString::cast(this)->ConsStringGet(index);
    case kExternalStringTag | kOneByteStringTag:
      return ExternalOneByteString::cast(this)->ExternalOneByteStringGet(index);
    case kExternalStringTag | kTwoByteStringTag:
      return ExternalTwoByteString::cast(this)->ExternalTwoByteStringGet(index);
    case kSlicedStringTag | kOneByteStringTag:
    case kSlicedStringTag | kTwoByteStringTag:
      return SlicedString::cast(this)->SlicedStringGet(index);
    case kThinStringTag | kOneByteStringTag:
    case kThinStringTag | kTwoByteStringTag:
      return ThinString::cast(this)->ThinStringGet(index);
    default:
      break;
  }

  UNREACHABLE();
  return 0;
}


void String::Set(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  DCHECK(StringShape(this).IsSequential());

  return this->IsOneByteRepresentation()
      ? SeqOneByteString::cast(this)->SeqOneByteStringSet(index, value)
      : SeqTwoByteString::cast(this)->SeqTwoByteStringSet(index, value);
}


bool String::IsFlat() {
  if (!StringShape(this).IsCons()) return true;
  return ConsString::cast(this)->second()->length() == 0;
}


String* String::GetUnderlying() {
  // Giving direct access to underlying string only makes sense if the
  // wrapping string is already flattened.
  DCHECK(this->IsFlat());
  DCHECK(StringShape(this).IsIndirect());
  STATIC_ASSERT(ConsString::kFirstOffset == SlicedString::kParentOffset);
  STATIC_ASSERT(ConsString::kFirstOffset == ThinString::kActualOffset);
  const int kUnderlyingOffset = SlicedString::kParentOffset;
  return String::cast(READ_FIELD(this, kUnderlyingOffset));
}


template<class Visitor>
ConsString* String::VisitFlat(Visitor* visitor,
                              String* string,
                              const int offset) {
  int slice_offset = offset;
  const int length = string->length();
  DCHECK(offset <= length);
  while (true) {
    int32_t type = string->map()->instance_type();
    switch (type & (kStringRepresentationMask | kStringEncodingMask)) {
      case kSeqStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            SeqOneByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return NULL;

      case kSeqStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            SeqTwoByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return NULL;

      case kExternalStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            ExternalOneByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return NULL;

      case kExternalStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            ExternalTwoByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return NULL;

      case kSlicedStringTag | kOneByteStringTag:
      case kSlicedStringTag | kTwoByteStringTag: {
        SlicedString* slicedString = SlicedString::cast(string);
        slice_offset += slicedString->offset();
        string = slicedString->parent();
        continue;
      }

      case kConsStringTag | kOneByteStringTag:
      case kConsStringTag | kTwoByteStringTag:
        return ConsString::cast(string);

      case kThinStringTag | kOneByteStringTag:
      case kThinStringTag | kTwoByteStringTag:
        string = ThinString::cast(string)->actual();
        continue;

      default:
        UNREACHABLE();
        return NULL;
    }
  }
}


template <>
inline Vector<const uint8_t> String::GetCharVector() {
  String::FlatContent flat = GetFlatContent();
  DCHECK(flat.IsOneByte());
  return flat.ToOneByteVector();
}


template <>
inline Vector<const uc16> String::GetCharVector() {
  String::FlatContent flat = GetFlatContent();
  DCHECK(flat.IsTwoByte());
  return flat.ToUC16Vector();
}

uint32_t String::ToValidIndex(Object* number) {
  uint32_t index = PositiveNumberToUint32(number);
  uint32_t length_value = static_cast<uint32_t>(length());
  if (index > length_value) return length_value;
  return index;
}

uint16_t SeqOneByteString::SeqOneByteStringGet(int index) {
  DCHECK(index >= 0 && index < length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}


void SeqOneByteString::SeqOneByteStringSet(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length() && value <= kMaxOneByteCharCode);
  WRITE_BYTE_FIELD(this, kHeaderSize + index * kCharSize,
                   static_cast<byte>(value));
}


Address SeqOneByteString::GetCharsAddress() {
  return FIELD_ADDR(this, kHeaderSize);
}


uint8_t* SeqOneByteString::GetChars() {
  return reinterpret_cast<uint8_t*>(GetCharsAddress());
}


Address SeqTwoByteString::GetCharsAddress() {
  return FIELD_ADDR(this, kHeaderSize);
}


uc16* SeqTwoByteString::GetChars() {
  return reinterpret_cast<uc16*>(FIELD_ADDR(this, kHeaderSize));
}


uint16_t SeqTwoByteString::SeqTwoByteStringGet(int index) {
  DCHECK(index >= 0 && index < length());
  return READ_UINT16_FIELD(this, kHeaderSize + index * kShortSize);
}


void SeqTwoByteString::SeqTwoByteStringSet(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  WRITE_UINT16_FIELD(this, kHeaderSize + index * kShortSize, value);
}


int SeqTwoByteString::SeqTwoByteStringSize(InstanceType instance_type) {
  return SizeFor(length());
}


int SeqOneByteString::SeqOneByteStringSize(InstanceType instance_type) {
  return SizeFor(length());
}


String* SlicedString::parent() {
  return String::cast(READ_FIELD(this, kParentOffset));
}


void SlicedString::set_parent(String* parent, WriteBarrierMode mode) {
  DCHECK(parent->IsSeqString() || parent->IsExternalString());
  WRITE_FIELD(this, kParentOffset, parent);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kParentOffset, parent, mode);
}


SMI_ACCESSORS(SlicedString, offset, kOffsetOffset)


String* ConsString::first() {
  return String::cast(READ_FIELD(this, kFirstOffset));
}


Object* ConsString::unchecked_first() {
  return READ_FIELD(this, kFirstOffset);
}


void ConsString::set_first(String* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kFirstOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kFirstOffset, value, mode);
}


String* ConsString::second() {
  return String::cast(READ_FIELD(this, kSecondOffset));
}


Object* ConsString::unchecked_second() {
  return READ_FIELD(this, kSecondOffset);
}


void ConsString::set_second(String* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kSecondOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kSecondOffset, value, mode);
}

ACCESSORS(ThinString, actual, String, kActualOffset);

bool ExternalString::is_short() {
  InstanceType type = map()->instance_type();
  return (type & kShortExternalStringMask) == kShortExternalStringTag;
}


const ExternalOneByteString::Resource* ExternalOneByteString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalOneByteString::update_data_cache() {
  if (is_short()) return;
  const char** data_field =
      reinterpret_cast<const char**>(FIELD_ADDR(this, kResourceDataOffset));
  *data_field = resource()->data();
}


void ExternalOneByteString::set_resource(
    const ExternalOneByteString::Resource* resource) {
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(resource), kPointerSize));
  *reinterpret_cast<const Resource**>(
      FIELD_ADDR(this, kResourceOffset)) = resource;
  if (resource != NULL) update_data_cache();
}


const uint8_t* ExternalOneByteString::GetChars() {
  return reinterpret_cast<const uint8_t*>(resource()->data());
}


uint16_t ExternalOneByteString::ExternalOneByteStringGet(int index) {
  DCHECK(index >= 0 && index < length());
  return GetChars()[index];
}


const ExternalTwoByteString::Resource* ExternalTwoByteString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalTwoByteString::update_data_cache() {
  if (is_short()) return;
  const uint16_t** data_field =
      reinterpret_cast<const uint16_t**>(FIELD_ADDR(this, kResourceDataOffset));
  *data_field = resource()->data();
}


void ExternalTwoByteString::set_resource(
    const ExternalTwoByteString::Resource* resource) {
  *reinterpret_cast<const Resource**>(
      FIELD_ADDR(this, kResourceOffset)) = resource;
  if (resource != NULL) update_data_cache();
}


const uint16_t* ExternalTwoByteString::GetChars() {
  return resource()->data();
}


uint16_t ExternalTwoByteString::ExternalTwoByteStringGet(int index) {
  DCHECK(index >= 0 && index < length());
  return GetChars()[index];
}


const uint16_t* ExternalTwoByteString::ExternalTwoByteStringGetData(
      unsigned start) {
  return GetChars() + start;
}


int ConsStringIterator::OffsetForDepth(int depth) { return depth & kDepthMask; }


void ConsStringIterator::PushLeft(ConsString* string) {
  frames_[depth_++ & kDepthMask] = string;
}


void ConsStringIterator::PushRight(ConsString* string) {
  // Inplace update.
  frames_[(depth_-1) & kDepthMask] = string;
}


void ConsStringIterator::AdjustMaximumDepth() {
  if (depth_ > maximum_depth_) maximum_depth_ = depth_;
}


void ConsStringIterator::Pop() {
  DCHECK(depth_ > 0);
  DCHECK(depth_ <= maximum_depth_);
  depth_--;
}


uint16_t StringCharacterStream::GetNext() {
  DCHECK(buffer8_ != NULL && end_ != NULL);
  // Advance cursor if needed.
  if (buffer8_ == end_) HasMore();
  DCHECK(buffer8_ < end_);
  return is_one_byte_ ? *buffer8_++ : *buffer16_++;
}


StringCharacterStream::StringCharacterStream(String* string, int offset)
    : is_one_byte_(false) {
  Reset(string, offset);
}


void StringCharacterStream::Reset(String* string, int offset) {
  buffer8_ = NULL;
  end_ = NULL;
  ConsString* cons_string = String::VisitFlat(this, string, offset);
  iter_.Reset(cons_string, offset);
  if (cons_string != NULL) {
    string = iter_.Next(&offset);
    if (string != NULL) String::VisitFlat(this, string, offset);
  }
}


bool StringCharacterStream::HasMore() {
  if (buffer8_ != end_) return true;
  int offset;
  String* string = iter_.Next(&offset);
  DCHECK_EQ(offset, 0);
  if (string == NULL) return false;
  String::VisitFlat(this, string);
  DCHECK(buffer8_ != end_);
  return true;
}


void StringCharacterStream::VisitOneByteString(
    const uint8_t* chars, int length) {
  is_one_byte_ = true;
  buffer8_ = chars;
  end_ = chars + length;
}


void StringCharacterStream::VisitTwoByteString(
    const uint16_t* chars, int length) {
  is_one_byte_ = false;
  buffer16_ = chars;
  end_ = reinterpret_cast<const uint8_t*>(chars + length);
}


int ByteArray::Size() { return RoundUp(length() + kHeaderSize, kPointerSize); }

byte ByteArray::get(int index) {
  DCHECK(index >= 0 && index < this->length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}

void ByteArray::set(int index, byte value) {
  DCHECK(index >= 0 && index < this->length());
  WRITE_BYTE_FIELD(this, kHeaderSize + index * kCharSize, value);
}

void ByteArray::copy_in(int index, const byte* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->length());
  byte* dst_addr = FIELD_ADDR(this, kHeaderSize + index * kCharSize);
  memcpy(dst_addr, buffer, length);
}

void ByteArray::copy_out(int index, byte* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->length());
  const byte* src_addr = FIELD_ADDR(this, kHeaderSize + index * kCharSize);
  memcpy(buffer, src_addr, length);
}

int ByteArray::get_int(int index) {
  DCHECK(index >= 0 && index < this->length() / kIntSize);
  return READ_INT_FIELD(this, kHeaderSize + index * kIntSize);
}

void ByteArray::set_int(int index, int value) {
  DCHECK(index >= 0 && index < this->length() / kIntSize);
  WRITE_INT_FIELD(this, kHeaderSize + index * kIntSize, value);
}

ByteArray* ByteArray::FromDataStartAddress(Address address) {
  DCHECK_TAG_ALIGNED(address);
  return reinterpret_cast<ByteArray*>(address - kHeaderSize + kHeapObjectTag);
}


int ByteArray::ByteArraySize() { return SizeFor(this->length()); }


Address ByteArray::GetDataStartAddress() {
  return reinterpret_cast<Address>(this) - kHeapObjectTag + kHeaderSize;
}


byte BytecodeArray::get(int index) {
  DCHECK(index >= 0 && index < this->length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}


void BytecodeArray::set(int index, byte value) {
  DCHECK(index >= 0 && index < this->length());
  WRITE_BYTE_FIELD(this, kHeaderSize + index * kCharSize, value);
}


void BytecodeArray::set_frame_size(int frame_size) {
  DCHECK_GE(frame_size, 0);
  DCHECK(IsAligned(frame_size, static_cast<unsigned>(kPointerSize)));
  WRITE_INT_FIELD(this, kFrameSizeOffset, frame_size);
}


int BytecodeArray::frame_size() const {
  return READ_INT_FIELD(this, kFrameSizeOffset);
}


int BytecodeArray::register_count() const {
  return frame_size() / kPointerSize;
}


void BytecodeArray::set_parameter_count(int number_of_parameters) {
  DCHECK_GE(number_of_parameters, 0);
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  WRITE_INT_FIELD(this, kParameterSizeOffset,
                  (number_of_parameters << kPointerSizeLog2));
}

int BytecodeArray::interrupt_budget() const {
  return READ_INT_FIELD(this, kInterruptBudgetOffset);
}

void BytecodeArray::set_interrupt_budget(int interrupt_budget) {
  DCHECK_GE(interrupt_budget, 0);
  WRITE_INT_FIELD(this, kInterruptBudgetOffset, interrupt_budget);
}

int BytecodeArray::osr_loop_nesting_level() const {
  return READ_INT8_FIELD(this, kOSRNestingLevelOffset);
}

void BytecodeArray::set_osr_loop_nesting_level(int depth) {
  DCHECK(0 <= depth && depth <= AbstractCode::kMaxLoopNestingMarker);
  STATIC_ASSERT(AbstractCode::kMaxLoopNestingMarker < kMaxInt8);
  WRITE_INT8_FIELD(this, kOSRNestingLevelOffset, depth);
}

BytecodeArray::Age BytecodeArray::bytecode_age() const {
  return static_cast<Age>(READ_INT8_FIELD(this, kBytecodeAgeOffset));
}

void BytecodeArray::set_bytecode_age(BytecodeArray::Age age) {
  DCHECK_GE(age, kFirstBytecodeAge);
  DCHECK_LE(age, kLastBytecodeAge);
  STATIC_ASSERT(kLastBytecodeAge <= kMaxInt8);
  WRITE_INT8_FIELD(this, kBytecodeAgeOffset, static_cast<int8_t>(age));
}

int BytecodeArray::parameter_count() const {
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  return READ_INT_FIELD(this, kParameterSizeOffset) >> kPointerSizeLog2;
}

ACCESSORS(BytecodeArray, constant_pool, FixedArray, kConstantPoolOffset)
ACCESSORS(BytecodeArray, handler_table, FixedArray, kHandlerTableOffset)
ACCESSORS(BytecodeArray, source_position_table, Object,
          kSourcePositionTableOffset)

Address BytecodeArray::GetFirstBytecodeAddress() {
  return reinterpret_cast<Address>(this) - kHeapObjectTag + kHeaderSize;
}

ByteArray* BytecodeArray::SourcePositionTable() {
  Object* maybe_table = source_position_table();
  if (maybe_table->IsByteArray()) return ByteArray::cast(maybe_table);
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  return SourcePositionTableWithFrameCache::cast(maybe_table)
      ->source_position_table();
}

int BytecodeArray::BytecodeArraySize() { return SizeFor(this->length()); }

int BytecodeArray::SizeIncludingMetadata() {
  int size = BytecodeArraySize();
  size += constant_pool()->Size();
  size += handler_table()->Size();
  size += SourcePositionTable()->Size();
  return size;
}

ACCESSORS(FixedTypedArrayBase, base_pointer, Object, kBasePointerOffset)


void* FixedTypedArrayBase::external_pointer() const {
  intptr_t ptr = READ_INTPTR_FIELD(this, kExternalPointerOffset);
  return reinterpret_cast<void*>(ptr);
}


void FixedTypedArrayBase::set_external_pointer(void* value,
                                               WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kExternalPointerOffset, ptr);
}


void* FixedTypedArrayBase::DataPtr() {
  return reinterpret_cast<void*>(
      reinterpret_cast<intptr_t>(base_pointer()) +
      reinterpret_cast<intptr_t>(external_pointer()));
}


int FixedTypedArrayBase::ElementSize(InstanceType type) {
  int element_size;
  switch (type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                       \
    case FIXED_##TYPE##_ARRAY_TYPE:                                           \
      element_size = size;                                                    \
      break;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    default:
      UNREACHABLE();
      return 0;
  }
  return element_size;
}


int FixedTypedArrayBase::DataSize(InstanceType type) {
  if (base_pointer() == Smi::kZero) return 0;
  return length() * ElementSize(type);
}


int FixedTypedArrayBase::DataSize() {
  return DataSize(map()->instance_type());
}


int FixedTypedArrayBase::size() {
  return OBJECT_POINTER_ALIGN(kDataOffset + DataSize());
}


int FixedTypedArrayBase::TypedArraySize(InstanceType type) {
  return OBJECT_POINTER_ALIGN(kDataOffset + DataSize(type));
}


int FixedTypedArrayBase::TypedArraySize(InstanceType type, int length) {
  return OBJECT_POINTER_ALIGN(kDataOffset + length * ElementSize(type));
}


uint8_t Uint8ArrayTraits::defaultValue() { return 0; }


uint8_t Uint8ClampedArrayTraits::defaultValue() { return 0; }


int8_t Int8ArrayTraits::defaultValue() { return 0; }


uint16_t Uint16ArrayTraits::defaultValue() { return 0; }


int16_t Int16ArrayTraits::defaultValue() { return 0; }


uint32_t Uint32ArrayTraits::defaultValue() { return 0; }


int32_t Int32ArrayTraits::defaultValue() { return 0; }


float Float32ArrayTraits::defaultValue() {
  return std::numeric_limits<float>::quiet_NaN();
}


double Float64ArrayTraits::defaultValue() {
  return std::numeric_limits<double>::quiet_NaN();
}


template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::get_scalar(int index) {
  DCHECK((index >= 0) && (index < this->length()));
  ElementType* ptr = reinterpret_cast<ElementType*>(DataPtr());
  return ptr[index];
}


template <class Traits>
void FixedTypedArray<Traits>::set(int index, ElementType value) {
  CHECK((index >= 0) && (index < this->length()));
  ElementType* ptr = reinterpret_cast<ElementType*>(DataPtr());
  ptr[index] = value;
}

template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::from(int value) {
  return static_cast<ElementType>(value);
}

template <>
inline uint8_t FixedTypedArray<Uint8ClampedArrayTraits>::from(int value) {
  if (value < 0) return 0;
  if (value > 0xFF) return 0xFF;
  return static_cast<uint8_t>(value);
}

template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::from(uint32_t value) {
  return static_cast<ElementType>(value);
}

template <>
inline uint8_t FixedTypedArray<Uint8ClampedArrayTraits>::from(uint32_t value) {
  // We need this special case for Uint32 -> Uint8Clamped, because the highest
  // Uint32 values will be negative as an int, clamping to 0, rather than 255.
  if (value > 0xFF) return 0xFF;
  return static_cast<uint8_t>(value);
}

template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::from(double value) {
  return static_cast<ElementType>(DoubleToInt32(value));
}

template <>
inline uint8_t FixedTypedArray<Uint8ClampedArrayTraits>::from(double value) {
  // Handle NaNs and less than zero values which clamp to zero.
  if (!(value > 0)) return 0;
  if (value > 0xFF) return 0xFF;
  return static_cast<uint8_t>(lrint(value));
}

template <>
inline float FixedTypedArray<Float32ArrayTraits>::from(double value) {
  return static_cast<float>(value);
}

template <>
inline double FixedTypedArray<Float64ArrayTraits>::from(double value) {
  return value;
}

template <class Traits>
Handle<Object> FixedTypedArray<Traits>::get(FixedTypedArray<Traits>* array,
                                            int index) {
  return Traits::ToHandle(array->GetIsolate(), array->get_scalar(index));
}


template <class Traits>
void FixedTypedArray<Traits>::SetValue(uint32_t index, Object* value) {
  ElementType cast_value = Traits::defaultValue();
  if (value->IsSmi()) {
    int int_value = Smi::cast(value)->value();
    cast_value = from(int_value);
  } else if (value->IsHeapNumber()) {
    double double_value = HeapNumber::cast(value)->value();
    cast_value = from(double_value);
  } else {
    // Clamp undefined to the default value. All other types have been
    // converted to a number type further up in the call chain.
    DCHECK(value->IsUndefined(GetIsolate()));
  }
  set(index, cast_value);
}


Handle<Object> Uint8ArrayTraits::ToHandle(Isolate* isolate, uint8_t scalar) {
  return handle(Smi::FromInt(scalar), isolate);
}


Handle<Object> Uint8ClampedArrayTraits::ToHandle(Isolate* isolate,
                                                 uint8_t scalar) {
  return handle(Smi::FromInt(scalar), isolate);
}


Handle<Object> Int8ArrayTraits::ToHandle(Isolate* isolate, int8_t scalar) {
  return handle(Smi::FromInt(scalar), isolate);
}


Handle<Object> Uint16ArrayTraits::ToHandle(Isolate* isolate, uint16_t scalar) {
  return handle(Smi::FromInt(scalar), isolate);
}


Handle<Object> Int16ArrayTraits::ToHandle(Isolate* isolate, int16_t scalar) {
  return handle(Smi::FromInt(scalar), isolate);
}


Handle<Object> Uint32ArrayTraits::ToHandle(Isolate* isolate, uint32_t scalar) {
  return isolate->factory()->NewNumberFromUint(scalar);
}


Handle<Object> Int32ArrayTraits::ToHandle(Isolate* isolate, int32_t scalar) {
  return isolate->factory()->NewNumberFromInt(scalar);
}


Handle<Object> Float32ArrayTraits::ToHandle(Isolate* isolate, float scalar) {
  return isolate->factory()->NewNumber(scalar);
}


Handle<Object> Float64ArrayTraits::ToHandle(Isolate* isolate, double scalar) {
  return isolate->factory()->NewNumber(scalar);
}


int Map::visitor_id() {
  return READ_BYTE_FIELD(this, kVisitorIdOffset);
}


void Map::set_visitor_id(int id) {
  DCHECK(0 <= id && id < 256);
  WRITE_BYTE_FIELD(this, kVisitorIdOffset, static_cast<byte>(id));
}


int Map::instance_size() {
  return NOBARRIER_READ_BYTE_FIELD(
      this, kInstanceSizeOffset) << kPointerSizeLog2;
}


int Map::inobject_properties_or_constructor_function_index() {
  return READ_BYTE_FIELD(this,
                         kInObjectPropertiesOrConstructorFunctionIndexOffset);
}


void Map::set_inobject_properties_or_constructor_function_index(int value) {
  DCHECK(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this, kInObjectPropertiesOrConstructorFunctionIndexOffset,
                   static_cast<byte>(value));
}


int Map::GetInObjectProperties() {
  DCHECK(IsJSObjectMap());
  return inobject_properties_or_constructor_function_index();
}


void Map::SetInObjectProperties(int value) {
  DCHECK(IsJSObjectMap());
  set_inobject_properties_or_constructor_function_index(value);
}


int Map::GetConstructorFunctionIndex() {
  DCHECK(IsPrimitiveMap());
  return inobject_properties_or_constructor_function_index();
}


void Map::SetConstructorFunctionIndex(int value) {
  DCHECK(IsPrimitiveMap());
  set_inobject_properties_or_constructor_function_index(value);
}


int Map::GetInObjectPropertyOffset(int index) {
  // Adjust for the number of properties stored in the object.
  index -= GetInObjectProperties();
  DCHECK(index <= 0);
  return instance_size() + (index * kPointerSize);
}


Handle<Map> Map::AddMissingTransitionsForTesting(
    Handle<Map> split_map, Handle<DescriptorArray> descriptors,
    Handle<LayoutDescriptor> full_layout_descriptor) {
  return AddMissingTransitions(split_map, descriptors, full_layout_descriptor);
}


int HeapObject::SizeFromMap(Map* map) {
  int instance_size = map->instance_size();
  if (instance_size != kVariableSizeSentinel) return instance_size;
  // Only inline the most frequent cases.
  InstanceType instance_type = map->instance_type();
  if (instance_type == FIXED_ARRAY_TYPE ||
      instance_type == TRANSITION_ARRAY_TYPE) {
    return FixedArray::SizeFor(
        reinterpret_cast<FixedArray*>(this)->synchronized_length());
  }
  if (instance_type == ONE_BYTE_STRING_TYPE ||
      instance_type == ONE_BYTE_INTERNALIZED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqOneByteString::SizeFor(
        reinterpret_cast<SeqOneByteString*>(this)->synchronized_length());
  }
  if (instance_type == BYTE_ARRAY_TYPE) {
    return reinterpret_cast<ByteArray*>(this)->ByteArraySize();
  }
  if (instance_type == BYTECODE_ARRAY_TYPE) {
    return reinterpret_cast<BytecodeArray*>(this)->BytecodeArraySize();
  }
  if (instance_type == FREE_SPACE_TYPE) {
    return reinterpret_cast<FreeSpace*>(this)->nobarrier_size();
  }
  if (instance_type == STRING_TYPE ||
      instance_type == INTERNALIZED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqTwoByteString::SizeFor(
        reinterpret_cast<SeqTwoByteString*>(this)->synchronized_length());
  }
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
    return FixedDoubleArray::SizeFor(
        reinterpret_cast<FixedDoubleArray*>(this)->length());
  }
  if (instance_type >= FIRST_FIXED_TYPED_ARRAY_TYPE &&
      instance_type <= LAST_FIXED_TYPED_ARRAY_TYPE) {
    return reinterpret_cast<FixedTypedArrayBase*>(
        this)->TypedArraySize(instance_type);
  }
  DCHECK(instance_type == CODE_TYPE);
  return reinterpret_cast<Code*>(this)->CodeSize();
}


void Map::set_instance_size(int value) {
  DCHECK_EQ(0, value & (kPointerSize - 1));
  value >>= kPointerSizeLog2;
  DCHECK(0 <= value && value < 256);
  NOBARRIER_WRITE_BYTE_FIELD(
      this, kInstanceSizeOffset, static_cast<byte>(value));
}


void Map::clear_unused() { WRITE_BYTE_FIELD(this, kUnusedOffset, 0); }


InstanceType Map::instance_type() {
  return static_cast<InstanceType>(READ_BYTE_FIELD(this, kInstanceTypeOffset));
}


void Map::set_instance_type(InstanceType value) {
  WRITE_BYTE_FIELD(this, kInstanceTypeOffset, value);
}


int Map::unused_property_fields() {
  return READ_BYTE_FIELD(this, kUnusedPropertyFieldsOffset);
}


void Map::set_unused_property_fields(int value) {
  WRITE_BYTE_FIELD(this, kUnusedPropertyFieldsOffset, Min(value, 255));
}


byte Map::bit_field() const { return READ_BYTE_FIELD(this, kBitFieldOffset); }


void Map::set_bit_field(byte value) {
  WRITE_BYTE_FIELD(this, kBitFieldOffset, value);
}


byte Map::bit_field2() const { return READ_BYTE_FIELD(this, kBitField2Offset); }


void Map::set_bit_field2(byte value) {
  WRITE_BYTE_FIELD(this, kBitField2Offset, value);
}


void Map::set_non_instance_prototype(bool value) {
  if (value) {
    set_bit_field(bit_field() | (1 << kHasNonInstancePrototype));
  } else {
    set_bit_field(bit_field() & ~(1 << kHasNonInstancePrototype));
  }
}


bool Map::has_non_instance_prototype() {
  return ((1 << kHasNonInstancePrototype) & bit_field()) != 0;
}


void Map::set_is_constructor(bool value) {
  if (value) {
    set_bit_field(bit_field() | (1 << kIsConstructor));
  } else {
    set_bit_field(bit_field() & ~(1 << kIsConstructor));
  }
}


bool Map::is_constructor() const {
  return ((1 << kIsConstructor) & bit_field()) != 0;
}

void Map::set_has_hidden_prototype(bool value) {
  set_bit_field3(HasHiddenPrototype::update(bit_field3(), value));
}

bool Map::has_hidden_prototype() const {
  return HasHiddenPrototype::decode(bit_field3());
}


void Map::set_has_indexed_interceptor() {
  set_bit_field(bit_field() | (1 << kHasIndexedInterceptor));
}


bool Map::has_indexed_interceptor() {
  return ((1 << kHasIndexedInterceptor) & bit_field()) != 0;
}


void Map::set_is_undetectable() {
  set_bit_field(bit_field() | (1 << kIsUndetectable));
}


bool Map::is_undetectable() {
  return ((1 << kIsUndetectable) & bit_field()) != 0;
}


void Map::set_has_named_interceptor() {
  set_bit_field(bit_field() | (1 << kHasNamedInterceptor));
}


bool Map::has_named_interceptor() {
  return ((1 << kHasNamedInterceptor) & bit_field()) != 0;
}


void Map::set_is_access_check_needed(bool access_check_needed) {
  if (access_check_needed) {
    set_bit_field(bit_field() | (1 << kIsAccessCheckNeeded));
  } else {
    set_bit_field(bit_field() & ~(1 << kIsAccessCheckNeeded));
  }
}


bool Map::is_access_check_needed() {
  return ((1 << kIsAccessCheckNeeded) & bit_field()) != 0;
}


void Map::set_is_extensible(bool value) {
  if (value) {
    set_bit_field2(bit_field2() | (1 << kIsExtensible));
  } else {
    set_bit_field2(bit_field2() & ~(1 << kIsExtensible));
  }
}

bool Map::is_extensible() {
  return ((1 << kIsExtensible) & bit_field2()) != 0;
}


void Map::set_is_prototype_map(bool value) {
  set_bit_field2(IsPrototypeMapBits::update(bit_field2(), value));
}

bool Map::is_prototype_map() const {
  return IsPrototypeMapBits::decode(bit_field2());
}

bool Map::should_be_fast_prototype_map() const {
  if (!prototype_info()->IsPrototypeInfo()) return false;
  return PrototypeInfo::cast(prototype_info())->should_be_fast_map();
}

void Map::set_elements_kind(ElementsKind elements_kind) {
  DCHECK(static_cast<int>(elements_kind) < kElementsKindCount);
  DCHECK(kElementsKindCount <= (1 << Map::ElementsKindBits::kSize));
  set_bit_field2(Map::ElementsKindBits::update(bit_field2(), elements_kind));
  DCHECK(this->elements_kind() == elements_kind);
}


ElementsKind Map::elements_kind() {
  return Map::ElementsKindBits::decode(bit_field2());
}


bool Map::has_fast_smi_elements() {
  return IsFastSmiElementsKind(elements_kind());
}

bool Map::has_fast_object_elements() {
  return IsFastObjectElementsKind(elements_kind());
}

bool Map::has_fast_smi_or_object_elements() {
  return IsFastSmiOrObjectElementsKind(elements_kind());
}

bool Map::has_fast_double_elements() {
  return IsFastDoubleElementsKind(elements_kind());
}

bool Map::has_fast_elements() { return IsFastElementsKind(elements_kind()); }

bool Map::has_sloppy_arguments_elements() {
  return IsSloppyArgumentsElementsKind(elements_kind());
}

bool Map::has_fast_sloppy_arguments_elements() {
  return elements_kind() == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
}

bool Map::has_fast_string_wrapper_elements() {
  return elements_kind() == FAST_STRING_WRAPPER_ELEMENTS;
}

bool Map::has_fixed_typed_array_elements() {
  return IsFixedTypedArrayElementsKind(elements_kind());
}

bool Map::has_dictionary_elements() {
  return IsDictionaryElementsKind(elements_kind());
}


void Map::set_dictionary_map(bool value) {
  uint32_t new_bit_field3 = DictionaryMap::update(bit_field3(), value);
  new_bit_field3 = IsUnstable::update(new_bit_field3, value);
  set_bit_field3(new_bit_field3);
}


bool Map::is_dictionary_map() {
  return DictionaryMap::decode(bit_field3());
}


Code::Flags Code::flags() {
  return static_cast<Flags>(READ_INT_FIELD(this, kFlagsOffset));
}


void Map::set_owns_descriptors(bool owns_descriptors) {
  set_bit_field3(OwnsDescriptors::update(bit_field3(), owns_descriptors));
}


bool Map::owns_descriptors() {
  return OwnsDescriptors::decode(bit_field3());
}


void Map::set_is_callable() { set_bit_field(bit_field() | (1 << kIsCallable)); }


bool Map::is_callable() const {
  return ((1 << kIsCallable) & bit_field()) != 0;
}


void Map::deprecate() {
  set_bit_field3(Deprecated::update(bit_field3(), true));
}


bool Map::is_deprecated() {
  return Deprecated::decode(bit_field3());
}


void Map::set_migration_target(bool value) {
  set_bit_field3(IsMigrationTarget::update(bit_field3(), value));
}


bool Map::is_migration_target() {
  return IsMigrationTarget::decode(bit_field3());
}

void Map::set_immutable_proto(bool value) {
  set_bit_field3(ImmutablePrototype::update(bit_field3(), value));
}

bool Map::is_immutable_proto() {
  return ImmutablePrototype::decode(bit_field3());
}

void Map::set_new_target_is_base(bool value) {
  set_bit_field3(NewTargetIsBase::update(bit_field3(), value));
}


bool Map::new_target_is_base() { return NewTargetIsBase::decode(bit_field3()); }


void Map::set_construction_counter(int value) {
  set_bit_field3(ConstructionCounter::update(bit_field3(), value));
}


int Map::construction_counter() {
  return ConstructionCounter::decode(bit_field3());
}


void Map::mark_unstable() {
  set_bit_field3(IsUnstable::update(bit_field3(), true));
}


bool Map::is_stable() {
  return !IsUnstable::decode(bit_field3());
}


bool Map::has_code_cache() {
  // Code caches are always fixed arrays. The empty fixed array is used as a
  // sentinel for an absent code cache.
  return code_cache()->length() != 0;
}


bool Map::CanBeDeprecated() {
  int descriptor = LastAdded();
  for (int i = 0; i <= descriptor; i++) {
    PropertyDetails details = instance_descriptors()->GetDetails(i);
    if (details.representation().IsNone()) return true;
    if (details.representation().IsSmi()) return true;
    if (details.representation().IsDouble()) return true;
    if (details.representation().IsHeapObject()) return true;
    if (details.kind() == kData && details.location() == kDescriptor) {
      return true;
    }
  }
  return false;
}


void Map::NotifyLeafMapLayoutChange() {
  if (is_stable()) {
    mark_unstable();
    dependent_code()->DeoptimizeDependentCodeGroup(
        GetIsolate(),
        DependentCode::kPrototypeCheckGroup);
  }
}


bool Map::CanTransition() {
  // Only JSObject and subtypes have map transitions and back pointers.
  STATIC_ASSERT(LAST_TYPE == LAST_JS_OBJECT_TYPE);
  return instance_type() >= FIRST_JS_OBJECT_TYPE;
}


bool Map::IsBooleanMap() { return this == GetHeap()->boolean_map(); }
bool Map::IsPrimitiveMap() {
  STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
  return instance_type() <= LAST_PRIMITIVE_TYPE;
}
bool Map::IsJSReceiverMap() {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return instance_type() >= FIRST_JS_RECEIVER_TYPE;
}
bool Map::IsJSObjectMap() {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return instance_type() >= FIRST_JS_OBJECT_TYPE;
}
bool Map::IsJSArrayMap() { return instance_type() == JS_ARRAY_TYPE; }
bool Map::IsJSFunctionMap() { return instance_type() == JS_FUNCTION_TYPE; }
bool Map::IsStringMap() { return instance_type() < FIRST_NONSTRING_TYPE; }
bool Map::IsJSProxyMap() { return instance_type() == JS_PROXY_TYPE; }
bool Map::IsJSGlobalProxyMap() {
  return instance_type() == JS_GLOBAL_PROXY_TYPE;
}
bool Map::IsJSGlobalObjectMap() {
  return instance_type() == JS_GLOBAL_OBJECT_TYPE;
}
bool Map::IsJSTypedArrayMap() { return instance_type() == JS_TYPED_ARRAY_TYPE; }
bool Map::IsJSDataViewMap() { return instance_type() == JS_DATA_VIEW_TYPE; }

bool Map::IsSpecialReceiverMap() {
  bool result = IsSpecialReceiverInstanceType(instance_type());
  DCHECK_IMPLIES(!result,
                 !has_named_interceptor() && !is_access_check_needed());
  return result;
}

bool Map::CanOmitMapChecks() {
  return is_stable() && FLAG_omit_map_checks_for_leaf_maps;
}


DependentCode* DependentCode::next_link() {
  return DependentCode::cast(get(kNextLinkIndex));
}


void DependentCode::set_next_link(DependentCode* next) {
  set(kNextLinkIndex, next);
}


int DependentCode::flags() { return Smi::cast(get(kFlagsIndex))->value(); }


void DependentCode::set_flags(int flags) {
  set(kFlagsIndex, Smi::FromInt(flags));
}


int DependentCode::count() { return CountField::decode(flags()); }

void DependentCode::set_count(int value) {
  set_flags(CountField::update(flags(), value));
}


DependentCode::DependencyGroup DependentCode::group() {
  return static_cast<DependencyGroup>(GroupField::decode(flags()));
}


void DependentCode::set_group(DependentCode::DependencyGroup group) {
  set_flags(GroupField::update(flags(), static_cast<int>(group)));
}


void DependentCode::set_object_at(int i, Object* object) {
  set(kCodesStartIndex + i, object);
}


Object* DependentCode::object_at(int i) {
  return get(kCodesStartIndex + i);
}


void DependentCode::clear_at(int i) {
  set_undefined(kCodesStartIndex + i);
}


void DependentCode::copy(int from, int to) {
  set(kCodesStartIndex + to, get(kCodesStartIndex + from));
}


void Code::set_flags(Code::Flags flags) {
  STATIC_ASSERT(Code::NUMBER_OF_KINDS <= KindField::kMax + 1);
  WRITE_INT_FIELD(this, kFlagsOffset, flags);
}


Code::Kind Code::kind() {
  return ExtractKindFromFlags(flags());
}

bool Code::IsCodeStubOrIC() {
  switch (kind()) {
    case STUB:
    case HANDLER:
#define CASE_KIND(kind) case kind:
      IC_KIND_LIST(CASE_KIND)
#undef CASE_KIND
      return true;
    default:
      return false;
  }
}

ExtraICState Code::extra_ic_state() {
  DCHECK(is_binary_op_stub() || is_compare_ic_stub() ||
         is_to_boolean_ic_stub() || is_debug_stub());
  return ExtractExtraICStateFromFlags(flags());
}


// For initialization.
void Code::set_raw_kind_specific_flags1(int value) {
  WRITE_INT_FIELD(this, kKindSpecificFlags1Offset, value);
}


void Code::set_raw_kind_specific_flags2(int value) {
  WRITE_INT_FIELD(this, kKindSpecificFlags2Offset, value);
}


inline bool Code::is_crankshafted() {
  return IsCrankshaftedField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset));
}


inline bool Code::is_hydrogen_stub() {
  return is_crankshafted() && kind() != OPTIMIZED_FUNCTION;
}

inline bool Code::is_interpreter_trampoline_builtin() {
  Builtins* builtins = GetIsolate()->builtins();
  return this == *builtins->InterpreterEntryTrampoline() ||
         this == *builtins->InterpreterEnterBytecodeAdvance() ||
         this == *builtins->InterpreterEnterBytecodeDispatch();
}

inline bool Code::has_unwinding_info() const {
  return HasUnwindingInfoField::decode(READ_UINT32_FIELD(this, kFlagsOffset));
}

inline void Code::set_has_unwinding_info(bool state) {
  uint32_t previous = READ_UINT32_FIELD(this, kFlagsOffset);
  uint32_t updated_value = HasUnwindingInfoField::update(previous, state);
  WRITE_UINT32_FIELD(this, kFlagsOffset, updated_value);
}

inline void Code::set_is_crankshafted(bool value) {
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = IsCrankshaftedField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}

inline bool Code::has_tagged_params() {
  int flags = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  return HasTaggedStackField::decode(flags);
}

inline void Code::set_has_tagged_params(bool value) {
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = HasTaggedStackField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}

inline bool Code::is_turbofanned() {
  return IsTurbofannedField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


inline void Code::set_is_turbofanned(bool value) {
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = IsTurbofannedField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}


inline bool Code::can_have_weak_objects() {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  return CanHaveWeakObjectsField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


inline void Code::set_can_have_weak_objects(bool value) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = CanHaveWeakObjectsField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}

inline bool Code::is_construct_stub() {
  DCHECK(kind() == BUILTIN);
  return IsConstructStubField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}

inline void Code::set_is_construct_stub(bool value) {
  DCHECK(kind() == BUILTIN);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = IsConstructStubField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}

inline bool Code::is_promise_rejection() {
  DCHECK(kind() == BUILTIN);
  return IsPromiseRejectionField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}

inline void Code::set_is_promise_rejection(bool value) {
  DCHECK(kind() == BUILTIN);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = IsPromiseRejectionField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}

inline bool Code::is_exception_caught() {
  DCHECK(kind() == BUILTIN);
  return IsExceptionCaughtField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}

inline void Code::set_is_exception_caught(bool value) {
  DCHECK(kind() == BUILTIN);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = IsExceptionCaughtField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}

bool Code::has_deoptimization_support() {
  DCHECK_EQ(FUNCTION, kind());
  unsigned flags = READ_UINT32_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasDeoptimizationSupportField::decode(flags);
}


void Code::set_has_deoptimization_support(bool value) {
  DCHECK_EQ(FUNCTION, kind());
  unsigned flags = READ_UINT32_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasDeoptimizationSupportField::update(flags, value);
  WRITE_UINT32_FIELD(this, kFullCodeFlags, flags);
}


bool Code::has_debug_break_slots() {
  DCHECK_EQ(FUNCTION, kind());
  unsigned flags = READ_UINT32_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasDebugBreakSlotsField::decode(flags);
}


void Code::set_has_debug_break_slots(bool value) {
  DCHECK_EQ(FUNCTION, kind());
  unsigned flags = READ_UINT32_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasDebugBreakSlotsField::update(flags, value);
  WRITE_UINT32_FIELD(this, kFullCodeFlags, flags);
}


bool Code::has_reloc_info_for_serialization() {
  DCHECK_EQ(FUNCTION, kind());
  unsigned flags = READ_UINT32_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasRelocInfoForSerialization::decode(flags);
}


void Code::set_has_reloc_info_for_serialization(bool value) {
  DCHECK_EQ(FUNCTION, kind());
  unsigned flags = READ_UINT32_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasRelocInfoForSerialization::update(flags, value);
  WRITE_UINT32_FIELD(this, kFullCodeFlags, flags);
}


int Code::allow_osr_at_loop_nesting_level() {
  DCHECK_EQ(FUNCTION, kind());
  int fields = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  return AllowOSRAtLoopNestingLevelField::decode(fields);
}


void Code::set_allow_osr_at_loop_nesting_level(int level) {
  DCHECK_EQ(FUNCTION, kind());
  DCHECK(level >= 0 && level <= AbstractCode::kMaxLoopNestingMarker);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = AllowOSRAtLoopNestingLevelField::update(previous, level);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


int Code::profiler_ticks() {
  DCHECK_EQ(FUNCTION, kind());
  return ProfilerTicksField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::set_profiler_ticks(int ticks) {
  if (kind() == FUNCTION) {
    unsigned previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
    unsigned updated = ProfilerTicksField::update(previous, ticks);
    WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
  }
}

int Code::builtin_index() { return READ_INT_FIELD(this, kBuiltinIndexOffset); }

void Code::set_builtin_index(int index) {
  WRITE_INT_FIELD(this, kBuiltinIndexOffset, index);
}


unsigned Code::stack_slots() {
  DCHECK(is_crankshafted());
  return StackSlotsField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::set_stack_slots(unsigned slots) {
  CHECK(slots <= (1 << kStackSlotsBitCount));
  DCHECK(is_crankshafted());
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = StackSlotsField::update(previous, slots);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}


unsigned Code::safepoint_table_offset() {
  DCHECK(is_crankshafted());
  return SafepointTableOffsetField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset));
}


void Code::set_safepoint_table_offset(unsigned offset) {
  CHECK(offset <= (1 << kSafepointTableOffsetBitCount));
  DCHECK(is_crankshafted());
  DCHECK(IsAligned(offset, static_cast<unsigned>(kIntSize)));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = SafepointTableOffsetField::update(previous, offset);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


unsigned Code::back_edge_table_offset() {
  DCHECK_EQ(FUNCTION, kind());
  return BackEdgeTableOffsetField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset)) << kPointerSizeLog2;
}


void Code::set_back_edge_table_offset(unsigned offset) {
  DCHECK_EQ(FUNCTION, kind());
  DCHECK(IsAligned(offset, static_cast<unsigned>(kPointerSize)));
  offset = offset >> kPointerSizeLog2;
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = BackEdgeTableOffsetField::update(previous, offset);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


bool Code::back_edges_patched_for_osr() {
  DCHECK_EQ(FUNCTION, kind());
  return allow_osr_at_loop_nesting_level() > 0;
}


uint16_t Code::to_boolean_state() { return extra_ic_state(); }


bool Code::marked_for_deoptimization() {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  return MarkedForDeoptimizationField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::set_marked_for_deoptimization(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = MarkedForDeoptimizationField::update(previous, flag);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}

bool Code::deopt_already_counted() {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  return DeoptAlreadyCountedField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}

void Code::set_deopt_already_counted(bool flag) {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  DCHECK_IMPLIES(flag, AllowDeoptimization::IsAllowed(GetIsolate()));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = DeoptAlreadyCountedField::update(previous, flag);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}

bool Code::is_inline_cache_stub() {
  Kind kind = this->kind();
  switch (kind) {
#define CASE(name) case name: return true;
    IC_KIND_LIST(CASE)
#undef CASE
    default: return false;
  }
}

bool Code::is_debug_stub() {
  if (kind() != BUILTIN) return false;
  switch (builtin_index()) {
#define CASE_DEBUG_BUILTIN(name) case Builtins::k##name:
    BUILTIN_LIST_DBG(CASE_DEBUG_BUILTIN)
#undef CASE_DEBUG_BUILTIN
      return true;
    default:
      return false;
  }
  return false;
}
bool Code::is_handler() { return kind() == HANDLER; }
bool Code::is_stub() { return kind() == STUB; }
bool Code::is_binary_op_stub() { return kind() == BINARY_OP_IC; }
bool Code::is_compare_ic_stub() { return kind() == COMPARE_IC; }
bool Code::is_to_boolean_ic_stub() { return kind() == TO_BOOLEAN_IC; }
bool Code::is_optimized_code() { return kind() == OPTIMIZED_FUNCTION; }
bool Code::is_wasm_code() { return kind() == WASM_FUNCTION; }

Address Code::constant_pool() {
  Address constant_pool = NULL;
  if (FLAG_enable_embedded_constant_pool) {
    int offset = constant_pool_offset();
    if (offset < instruction_size()) {
      constant_pool = FIELD_ADDR(this, kHeaderSize + offset);
    }
  }
  return constant_pool;
}

Code::Flags Code::ComputeFlags(Kind kind, ExtraICState extra_ic_state) {
  // Compute the bit mask.
  unsigned int bits =
      KindField::encode(kind) | ExtraICStateField::encode(extra_ic_state);
  return static_cast<Flags>(bits);
}

Code::Flags Code::ComputeHandlerFlags(Kind handler_kind) {
  return ComputeFlags(Code::HANDLER, handler_kind);
}


Code::Kind Code::ExtractKindFromFlags(Flags flags) {
  return KindField::decode(flags);
}


ExtraICState Code::ExtractExtraICStateFromFlags(Flags flags) {
  return ExtraICStateField::decode(flags);
}


Code* Code::GetCodeFromTargetAddress(Address address) {
  HeapObject* code = HeapObject::FromAddress(address - Code::kHeaderSize);
  // GetCodeFromTargetAddress might be called when marking objects during mark
  // sweep. reinterpret_cast is therefore used instead of the more appropriate
  // Code::cast. Code::cast does not work when the object's map is
  // marked.
  Code* result = reinterpret_cast<Code*>(code);
  return result;
}


Object* Code::GetObjectFromEntryAddress(Address location_of_address) {
  return HeapObject::
      FromAddress(Memory::Address_at(location_of_address) - Code::kHeaderSize);
}


bool Code::CanContainWeakObjects() {
  return is_optimized_code() && can_have_weak_objects();
}


bool Code::IsWeakObject(Object* object) {
  return (CanContainWeakObjects() && IsWeakObjectInOptimizedCode(object));
}


bool Code::IsWeakObjectInOptimizedCode(Object* object) {
  if (object->IsMap()) {
    return Map::cast(object)->CanTransition();
  }
  if (object->IsCell()) {
    object = Cell::cast(object)->value();
  } else if (object->IsPropertyCell()) {
    object = PropertyCell::cast(object)->value();
  }
  if (object->IsJSReceiver() || object->IsContext()) {
    return true;
  }
  return false;
}


int AbstractCode::instruction_size() {
  if (IsCode()) {
    return GetCode()->instruction_size();
  } else {
    return GetBytecodeArray()->length();
  }
}

ByteArray* AbstractCode::source_position_table() {
  if (IsCode()) {
    return GetCode()->SourcePositionTable();
  } else {
    return GetBytecodeArray()->SourcePositionTable();
  }
}

void AbstractCode::set_source_position_table(ByteArray* source_position_table) {
  if (IsCode()) {
    GetCode()->set_source_position_table(source_position_table);
  } else {
    GetBytecodeArray()->set_source_position_table(source_position_table);
  }
}

Object* AbstractCode::stack_frame_cache() {
  Object* maybe_table;
  if (IsCode()) {
    maybe_table = GetCode()->source_position_table();
  } else {
    maybe_table = GetBytecodeArray()->source_position_table();
  }
  if (maybe_table->IsSourcePositionTableWithFrameCache()) {
    return SourcePositionTableWithFrameCache::cast(maybe_table)
        ->stack_frame_cache();
  }
  return Smi::kZero;
}

int AbstractCode::SizeIncludingMetadata() {
  if (IsCode()) {
    return GetCode()->SizeIncludingMetadata();
  } else {
    return GetBytecodeArray()->SizeIncludingMetadata();
  }
}
int AbstractCode::ExecutableSize() {
  if (IsCode()) {
    return GetCode()->ExecutableSize();
  } else {
    return GetBytecodeArray()->BytecodeArraySize();
  }
}

Address AbstractCode::instruction_start() {
  if (IsCode()) {
    return GetCode()->instruction_start();
  } else {
    return GetBytecodeArray()->GetFirstBytecodeAddress();
  }
}

Address AbstractCode::instruction_end() {
  if (IsCode()) {
    return GetCode()->instruction_end();
  } else {
    return GetBytecodeArray()->GetFirstBytecodeAddress() +
           GetBytecodeArray()->length();
  }
}

bool AbstractCode::contains(byte* inner_pointer) {
  return (address() <= inner_pointer) && (inner_pointer <= address() + Size());
}

AbstractCode::Kind AbstractCode::kind() {
  if (IsCode()) {
    STATIC_ASSERT(AbstractCode::FUNCTION ==
                  static_cast<AbstractCode::Kind>(Code::FUNCTION));
    return static_cast<AbstractCode::Kind>(GetCode()->kind());
  } else {
    return INTERPRETED_FUNCTION;
  }
}

Code* AbstractCode::GetCode() { return Code::cast(this); }

BytecodeArray* AbstractCode::GetBytecodeArray() {
  return BytecodeArray::cast(this);
}

Object* Map::prototype() const {
  return READ_FIELD(this, kPrototypeOffset);
}


void Map::set_prototype(Object* value, WriteBarrierMode mode) {
  DCHECK(value->IsNull(GetIsolate()) || value->IsJSReceiver());
  WRITE_FIELD(this, kPrototypeOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kPrototypeOffset, value, mode);
}


LayoutDescriptor* Map::layout_descriptor_gc_safe() {
  Object* layout_desc = READ_FIELD(this, kLayoutDescriptorOffset);
  return LayoutDescriptor::cast_gc_safe(layout_desc);
}


bool Map::HasFastPointerLayout() const {
  Object* layout_desc = READ_FIELD(this, kLayoutDescriptorOffset);
  return LayoutDescriptor::IsFastPointerLayout(layout_desc);
}


void Map::UpdateDescriptors(DescriptorArray* descriptors,
                            LayoutDescriptor* layout_desc) {
  set_instance_descriptors(descriptors);
  if (FLAG_unbox_double_fields) {
    if (layout_descriptor()->IsSlowLayout()) {
      set_layout_descriptor(layout_desc);
    }
#ifdef VERIFY_HEAP
    // TODO(ishell): remove these checks from VERIFY_HEAP mode.
    if (FLAG_verify_heap) {
      CHECK(layout_descriptor()->IsConsistentWithMap(this));
      CHECK(visitor_id() == Heap::GetStaticVisitorIdForMap(this));
    }
#else
    SLOW_DCHECK(layout_descriptor()->IsConsistentWithMap(this));
    DCHECK(visitor_id() == Heap::GetStaticVisitorIdForMap(this));
#endif
  }
}


void Map::InitializeDescriptors(DescriptorArray* descriptors,
                                LayoutDescriptor* layout_desc) {
  int len = descriptors->number_of_descriptors();
  set_instance_descriptors(descriptors);
  SetNumberOfOwnDescriptors(len);

  if (FLAG_unbox_double_fields) {
    set_layout_descriptor(layout_desc);
#ifdef VERIFY_HEAP
    // TODO(ishell): remove these checks from VERIFY_HEAP mode.
    if (FLAG_verify_heap) {
      CHECK(layout_descriptor()->IsConsistentWithMap(this));
    }
#else
    SLOW_DCHECK(layout_descriptor()->IsConsistentWithMap(this));
#endif
    set_visitor_id(Heap::GetStaticVisitorIdForMap(this));
  }
}


ACCESSORS(Map, instance_descriptors, DescriptorArray, kDescriptorsOffset)
ACCESSORS(Map, layout_descriptor, LayoutDescriptor, kLayoutDescriptorOffset)

void Map::set_bit_field3(uint32_t bits) {
  if (kInt32Size != kPointerSize) {
    WRITE_UINT32_FIELD(this, kBitField3Offset + kInt32Size, 0);
  }
  WRITE_UINT32_FIELD(this, kBitField3Offset, bits);
}


uint32_t Map::bit_field3() const {
  return READ_UINT32_FIELD(this, kBitField3Offset);
}


LayoutDescriptor* Map::GetLayoutDescriptor() {
  return FLAG_unbox_double_fields ? layout_descriptor()
                                  : LayoutDescriptor::FastPointerLayout();
}


void Map::AppendDescriptor(Descriptor* desc) {
  DescriptorArray* descriptors = instance_descriptors();
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DCHECK(descriptors->number_of_descriptors() == number_of_own_descriptors);
  descriptors->Append(desc);
  SetNumberOfOwnDescriptors(number_of_own_descriptors + 1);

// This function does not support appending double field descriptors and
// it should never try to (otherwise, layout descriptor must be updated too).
#ifdef DEBUG
  PropertyDetails details = desc->GetDetails();
  CHECK(details.location() != kField || !details.representation().IsDouble());
#endif
}


Object* Map::GetBackPointer() {
  Object* object = constructor_or_backpointer();
  if (object->IsMap()) {
    return object;
  }
  return GetIsolate()->heap()->undefined_value();
}


Map* Map::ElementsTransitionMap() {
  return TransitionArray::SearchSpecial(
      this, GetHeap()->elements_transition_symbol());
}


ACCESSORS(Map, raw_transitions, Object, kTransitionsOrPrototypeInfoOffset)


Object* Map::prototype_info() const {
  DCHECK(is_prototype_map());
  return READ_FIELD(this, Map::kTransitionsOrPrototypeInfoOffset);
}


void Map::set_prototype_info(Object* value, WriteBarrierMode mode) {
  DCHECK(is_prototype_map());
  WRITE_FIELD(this, Map::kTransitionsOrPrototypeInfoOffset, value);
  CONDITIONAL_WRITE_BARRIER(
      GetHeap(), this, Map::kTransitionsOrPrototypeInfoOffset, value, mode);
}


void Map::SetBackPointer(Object* value, WriteBarrierMode mode) {
  DCHECK(instance_type() >= FIRST_JS_RECEIVER_TYPE);
  DCHECK(value->IsMap());
  DCHECK(GetBackPointer()->IsUndefined(GetIsolate()));
  DCHECK(!value->IsMap() ||
         Map::cast(value)->GetConstructor() == constructor_or_backpointer());
  set_constructor_or_backpointer(value, mode);
}

ACCESSORS(JSArgumentsObject, length, Object, kLengthOffset);
ACCESSORS(JSSloppyArgumentsObject, callee, Object, kCalleeOffset);

ACCESSORS(Map, code_cache, FixedArray, kCodeCacheOffset)
ACCESSORS(Map, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS(Map, weak_cell_cache, Object, kWeakCellCacheOffset)
ACCESSORS(Map, constructor_or_backpointer, Object,
          kConstructorOrBackPointerOffset)

Object* Map::GetConstructor() const {
  Object* maybe_constructor = constructor_or_backpointer();
  // Follow any back pointers.
  while (maybe_constructor->IsMap()) {
    maybe_constructor =
        Map::cast(maybe_constructor)->constructor_or_backpointer();
  }
  return maybe_constructor;
}

FunctionTemplateInfo* Map::GetFunctionTemplateInfo() const {
  Object* constructor = GetConstructor();
  if (constructor->IsJSFunction()) {
    DCHECK(JSFunction::cast(constructor)->shared()->IsApiFunction());
    return JSFunction::cast(constructor)->shared()->get_api_func_data();
  }
  DCHECK(constructor->IsFunctionTemplateInfo());
  return FunctionTemplateInfo::cast(constructor);
}

void Map::SetConstructor(Object* constructor, WriteBarrierMode mode) {
  // Never overwrite a back pointer with a constructor.
  DCHECK(!constructor_or_backpointer()->IsMap());
  set_constructor_or_backpointer(constructor, mode);
}


Handle<Map> Map::CopyInitialMap(Handle<Map> map) {
  return CopyInitialMap(map, map->instance_size(), map->GetInObjectProperties(),
                        map->unused_property_fields());
}


ACCESSORS(JSBoundFunction, bound_target_function, JSReceiver,
          kBoundTargetFunctionOffset)
ACCESSORS(JSBoundFunction, bound_this, Object, kBoundThisOffset)
ACCESSORS(JSBoundFunction, bound_arguments, FixedArray, kBoundArgumentsOffset)

ACCESSORS(JSFunction, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
ACCESSORS(JSFunction, feedback_vector_cell, Cell, kFeedbackVectorOffset)
ACCESSORS(JSFunction, next_function_link, Object, kNextFunctionLinkOffset)

ACCESSORS(JSGlobalObject, native_context, Context, kNativeContextOffset)
ACCESSORS(JSGlobalObject, global_proxy, JSObject, kGlobalProxyOffset)

ACCESSORS(JSGlobalProxy, native_context, Object, kNativeContextOffset)
ACCESSORS(JSGlobalProxy, hash, Object, kHashOffset)

ACCESSORS(AccessorInfo, name, Object, kNameOffset)
SMI_ACCESSORS(AccessorInfo, flag, kFlagOffset)
ACCESSORS(AccessorInfo, expected_receiver_type, Object,
          kExpectedReceiverTypeOffset)

ACCESSORS(AccessorInfo, getter, Object, kGetterOffset)
ACCESSORS(AccessorInfo, setter, Object, kSetterOffset)
ACCESSORS(AccessorInfo, js_getter, Object, kJsGetterOffset)
ACCESSORS(AccessorInfo, data, Object, kDataOffset)

ACCESSORS(PromiseResolveThenableJobInfo, thenable, JSReceiver, kThenableOffset)
ACCESSORS(PromiseResolveThenableJobInfo, then, JSReceiver, kThenOffset)
ACCESSORS(PromiseResolveThenableJobInfo, resolve, JSFunction, kResolveOffset)
ACCESSORS(PromiseResolveThenableJobInfo, reject, JSFunction, kRejectOffset)
ACCESSORS(PromiseResolveThenableJobInfo, context, Context, kContextOffset);

ACCESSORS(PromiseReactionJobInfo, value, Object, kValueOffset);
ACCESSORS(PromiseReactionJobInfo, tasks, Object, kTasksOffset);
ACCESSORS(PromiseReactionJobInfo, deferred_promise, Object,
          kDeferredPromiseOffset);
ACCESSORS(PromiseReactionJobInfo, deferred_on_resolve, Object,
          kDeferredOnResolveOffset);
ACCESSORS(PromiseReactionJobInfo, deferred_on_reject, Object,
          kDeferredOnRejectOffset);
ACCESSORS(PromiseReactionJobInfo, context, Context, kContextOffset);

ACCESSORS(AsyncGeneratorRequest, next, Object, kNextOffset)
SMI_ACCESSORS(AsyncGeneratorRequest, resume_mode, kResumeModeOffset)
ACCESSORS(AsyncGeneratorRequest, value, Object, kValueOffset)
ACCESSORS(AsyncGeneratorRequest, promise, Object, kPromiseOffset)

Map* PrototypeInfo::ObjectCreateMap() {
  return Map::cast(WeakCell::cast(object_create_map())->value());
}

// static
void PrototypeInfo::SetObjectCreateMap(Handle<PrototypeInfo> info,
                                       Handle<Map> map) {
  Handle<WeakCell> cell = Map::WeakCellForMap(map);
  info->set_object_create_map(*cell);
}

bool PrototypeInfo::HasObjectCreateMap() {
  Object* cache = object_create_map();
  return cache->IsWeakCell() && !WeakCell::cast(cache)->cleared();
}

bool FunctionTemplateInfo::instantiated() {
  return shared_function_info()->IsSharedFunctionInfo();
}

FunctionTemplateInfo* FunctionTemplateInfo::GetParent(Isolate* isolate) {
  Object* parent = parent_template();
  return parent->IsUndefined(isolate) ? nullptr
                                      : FunctionTemplateInfo::cast(parent);
}

ObjectTemplateInfo* ObjectTemplateInfo::GetParent(Isolate* isolate) {
  Object* maybe_ctor = constructor();
  if (maybe_ctor->IsUndefined(isolate)) return nullptr;
  FunctionTemplateInfo* constructor = FunctionTemplateInfo::cast(maybe_ctor);
  while (true) {
    constructor = constructor->GetParent(isolate);
    if (constructor == nullptr) return nullptr;
    Object* maybe_obj = constructor->instance_template();
    if (!maybe_obj->IsUndefined(isolate)) {
      return ObjectTemplateInfo::cast(maybe_obj);
    }
  }
  return nullptr;
}

ACCESSORS(PrototypeInfo, weak_cell, Object, kWeakCellOffset)
ACCESSORS(PrototypeInfo, prototype_users, Object, kPrototypeUsersOffset)
ACCESSORS(PrototypeInfo, object_create_map, Object, kObjectCreateMap)
SMI_ACCESSORS(PrototypeInfo, registry_slot, kRegistrySlotOffset)
ACCESSORS(PrototypeInfo, validity_cell, Object, kValidityCellOffset)
SMI_ACCESSORS(PrototypeInfo, bit_field, kBitFieldOffset)
BOOL_ACCESSORS(PrototypeInfo, bit_field, should_be_fast_map, kShouldBeFastBit)

ACCESSORS(Tuple2, value1, Object, kValue1Offset)
ACCESSORS(Tuple2, value2, Object, kValue2Offset)
ACCESSORS(Tuple3, value3, Object, kValue3Offset)

ACCESSORS(ContextExtension, scope_info, ScopeInfo, kScopeInfoOffset)
ACCESSORS(ContextExtension, extension, Object, kExtensionOffset)

SMI_ACCESSORS(ConstantElementsPair, elements_kind, kElementsKindOffset)
ACCESSORS(ConstantElementsPair, constant_values, FixedArrayBase,
          kConstantValuesOffset)

ACCESSORS(JSModuleNamespace, module, Module, kModuleOffset)

ACCESSORS(Module, code, Object, kCodeOffset)
ACCESSORS(Module, exports, ObjectHashTable, kExportsOffset)
ACCESSORS(Module, regular_exports, FixedArray, kRegularExportsOffset)
ACCESSORS(Module, regular_imports, FixedArray, kRegularImportsOffset)
ACCESSORS(Module, module_namespace, HeapObject, kModuleNamespaceOffset)
ACCESSORS(Module, requested_modules, FixedArray, kRequestedModulesOffset)
SMI_ACCESSORS(Module, status, kStatusOffset)
SMI_ACCESSORS(Module, hash, kHashOffset)

bool Module::evaluated() const { return code()->IsModuleInfo(); }

void Module::set_evaluated() {
  DCHECK(instantiated());
  DCHECK(!evaluated());
  return set_code(
      JSFunction::cast(code())->shared()->scope_info()->ModuleDescriptorInfo());
}

bool Module::instantiated() const { return !code()->IsSharedFunctionInfo(); }

ModuleInfo* Module::info() const {
  if (evaluated()) return ModuleInfo::cast(code());
  ScopeInfo* scope_info = instantiated()
                              ? JSFunction::cast(code())->shared()->scope_info()
                              : SharedFunctionInfo::cast(code())->scope_info();
  return scope_info->ModuleDescriptorInfo();
}

ACCESSORS(AccessorPair, getter, Object, kGetterOffset)
ACCESSORS(AccessorPair, setter, Object, kSetterOffset)

ACCESSORS(AccessCheckInfo, callback, Object, kCallbackOffset)
ACCESSORS(AccessCheckInfo, named_interceptor, Object, kNamedInterceptorOffset)
ACCESSORS(AccessCheckInfo, indexed_interceptor, Object,
          kIndexedInterceptorOffset)
ACCESSORS(AccessCheckInfo, data, Object, kDataOffset)

ACCESSORS(InterceptorInfo, getter, Object, kGetterOffset)
ACCESSORS(InterceptorInfo, setter, Object, kSetterOffset)
ACCESSORS(InterceptorInfo, query, Object, kQueryOffset)
ACCESSORS(InterceptorInfo, descriptor, Object, kDescriptorOffset)
ACCESSORS(InterceptorInfo, deleter, Object, kDeleterOffset)
ACCESSORS(InterceptorInfo, enumerator, Object, kEnumeratorOffset)
ACCESSORS(InterceptorInfo, definer, Object, kDefinerOffset)
ACCESSORS(InterceptorInfo, data, Object, kDataOffset)
SMI_ACCESSORS(InterceptorInfo, flags, kFlagsOffset)
BOOL_ACCESSORS(InterceptorInfo, flags, can_intercept_symbols,
               kCanInterceptSymbolsBit)
BOOL_ACCESSORS(InterceptorInfo, flags, all_can_read, kAllCanReadBit)
BOOL_ACCESSORS(InterceptorInfo, flags, non_masking, kNonMasking)

ACCESSORS(CallHandlerInfo, callback, Object, kCallbackOffset)
ACCESSORS(CallHandlerInfo, data, Object, kDataOffset)

ACCESSORS(TemplateInfo, tag, Object, kTagOffset)
ACCESSORS(TemplateInfo, serial_number, Object, kSerialNumberOffset)
SMI_ACCESSORS(TemplateInfo, number_of_properties, kNumberOfProperties)
ACCESSORS(TemplateInfo, property_list, Object, kPropertyListOffset)
ACCESSORS(TemplateInfo, property_accessors, Object, kPropertyAccessorsOffset)

ACCESSORS(FunctionTemplateInfo, call_code, Object, kCallCodeOffset)
ACCESSORS(FunctionTemplateInfo, prototype_template, Object,
          kPrototypeTemplateOffset)
ACCESSORS(FunctionTemplateInfo, prototype_provider_template, Object,
          kPrototypeProviderTemplateOffset)

ACCESSORS(FunctionTemplateInfo, parent_template, Object, kParentTemplateOffset)
ACCESSORS(FunctionTemplateInfo, named_property_handler, Object,
          kNamedPropertyHandlerOffset)
ACCESSORS(FunctionTemplateInfo, indexed_property_handler, Object,
          kIndexedPropertyHandlerOffset)
ACCESSORS(FunctionTemplateInfo, instance_template, Object,
          kInstanceTemplateOffset)
ACCESSORS(FunctionTemplateInfo, class_name, Object, kClassNameOffset)
ACCESSORS(FunctionTemplateInfo, signature, Object, kSignatureOffset)
ACCESSORS(FunctionTemplateInfo, instance_call_handler, Object,
          kInstanceCallHandlerOffset)
ACCESSORS(FunctionTemplateInfo, access_check_info, Object,
          kAccessCheckInfoOffset)
ACCESSORS(FunctionTemplateInfo, shared_function_info, Object,
          kSharedFunctionInfoOffset)
ACCESSORS(FunctionTemplateInfo, cached_property_name, Object,
          kCachedPropertyNameOffset)

SMI_ACCESSORS(FunctionTemplateInfo, flag, kFlagOffset)

ACCESSORS(ObjectTemplateInfo, constructor, Object, kConstructorOffset)
ACCESSORS(ObjectTemplateInfo, data, Object, kDataOffset)

int ObjectTemplateInfo::embedder_field_count() const {
  Object* value = data();
  DCHECK(value->IsSmi());
  return EmbedderFieldCount::decode(Smi::cast(value)->value());
}

void ObjectTemplateInfo::set_embedder_field_count(int count) {
  return set_data(Smi::FromInt(
      EmbedderFieldCount::update(Smi::cast(data())->value(), count)));
}

bool ObjectTemplateInfo::immutable_proto() const {
  Object* value = data();
  DCHECK(value->IsSmi());
  return IsImmutablePrototype::decode(Smi::cast(value)->value());
}

void ObjectTemplateInfo::set_immutable_proto(bool immutable) {
  return set_data(Smi::FromInt(
      IsImmutablePrototype::update(Smi::cast(data())->value(), immutable)));
}

int TemplateList::length() const {
  return Smi::cast(FixedArray::cast(this)->get(kLengthIndex))->value();
}

Object* TemplateList::get(int index) const {
  return FixedArray::cast(this)->get(kFirstElementIndex + index);
}

void TemplateList::set(int index, Object* value) {
  FixedArray::cast(this)->set(kFirstElementIndex + index, value);
}

ACCESSORS(AllocationSite, transition_info, Object, kTransitionInfoOffset)
ACCESSORS(AllocationSite, nested_site, Object, kNestedSiteOffset)
SMI_ACCESSORS(AllocationSite, pretenure_data, kPretenureDataOffset)
SMI_ACCESSORS(AllocationSite, pretenure_create_count,
              kPretenureCreateCountOffset)
ACCESSORS(AllocationSite, dependent_code, DependentCode,
          kDependentCodeOffset)
ACCESSORS(AllocationSite, weak_next, Object, kWeakNextOffset)
ACCESSORS(AllocationMemento, allocation_site, Object, kAllocationSiteOffset)

ACCESSORS(Script, source, Object, kSourceOffset)
ACCESSORS(Script, name, Object, kNameOffset)
SMI_ACCESSORS(Script, id, kIdOffset)
SMI_ACCESSORS(Script, line_offset, kLineOffsetOffset)
SMI_ACCESSORS(Script, column_offset, kColumnOffsetOffset)
ACCESSORS(Script, context_data, Object, kContextOffset)
ACCESSORS(Script, wrapper, HeapObject, kWrapperOffset)
SMI_ACCESSORS(Script, type, kTypeOffset)
ACCESSORS(Script, line_ends, Object, kLineEndsOffset)
ACCESSORS_CHECKED(Script, eval_from_shared, Object, kEvalFromSharedOffset,
                  this->type() != TYPE_WASM)
SMI_ACCESSORS_CHECKED(Script, eval_from_position, kEvalFromPositionOffset,
                      this->type() != TYPE_WASM)
ACCESSORS(Script, shared_function_infos, FixedArray, kSharedFunctionInfosOffset)
SMI_ACCESSORS(Script, flags, kFlagsOffset)
ACCESSORS(Script, source_url, Object, kSourceUrlOffset)
ACCESSORS(Script, source_mapping_url, Object, kSourceMappingUrlOffset)
ACCESSORS_CHECKED(Script, wasm_compiled_module, Object, kEvalFromSharedOffset,
                  this->type() == TYPE_WASM)
ACCESSORS(Script, preparsed_scope_data, PodArray<uint32_t>,
          kPreParsedScopeDataOffset)

Script::CompilationType Script::compilation_type() {
  return BooleanBit::get(flags(), kCompilationTypeBit) ?
      COMPILATION_TYPE_EVAL : COMPILATION_TYPE_HOST;
}
void Script::set_compilation_type(CompilationType type) {
  set_flags(BooleanBit::set(flags(), kCompilationTypeBit,
      type == COMPILATION_TYPE_EVAL));
}
Script::CompilationState Script::compilation_state() {
  return BooleanBit::get(flags(), kCompilationStateBit) ?
      COMPILATION_STATE_COMPILED : COMPILATION_STATE_INITIAL;
}
void Script::set_compilation_state(CompilationState state) {
  set_flags(BooleanBit::set(flags(), kCompilationStateBit,
      state == COMPILATION_STATE_COMPILED));
}
ScriptOriginOptions Script::origin_options() {
  return ScriptOriginOptions((flags() & kOriginOptionsMask) >>
                             kOriginOptionsShift);
}
void Script::set_origin_options(ScriptOriginOptions origin_options) {
  DCHECK(!(origin_options.Flags() & ~((1 << kOriginOptionsSize) - 1)));
  set_flags((flags() & ~kOriginOptionsMask) |
            (origin_options.Flags() << kOriginOptionsShift));
}


ACCESSORS(DebugInfo, shared, SharedFunctionInfo, kSharedFunctionInfoIndex)
SMI_ACCESSORS(DebugInfo, debugger_hints, kDebuggerHintsIndex)
ACCESSORS(DebugInfo, debug_bytecode_array, Object, kDebugBytecodeArrayIndex)
ACCESSORS(DebugInfo, break_points, FixedArray, kBreakPointsStateIndex)

bool DebugInfo::HasDebugBytecodeArray() {
  return debug_bytecode_array()->IsBytecodeArray();
}

bool DebugInfo::HasDebugCode() {
  Code* code = shared()->code();
  bool has = code->kind() == Code::FUNCTION;
  DCHECK(!has || code->has_debug_break_slots());
  return has;
}

BytecodeArray* DebugInfo::OriginalBytecodeArray() {
  DCHECK(HasDebugBytecodeArray());
  return shared()->bytecode_array();
}

BytecodeArray* DebugInfo::DebugBytecodeArray() {
  DCHECK(HasDebugBytecodeArray());
  return BytecodeArray::cast(debug_bytecode_array());
}

Code* DebugInfo::DebugCode() {
  DCHECK(HasDebugCode());
  return shared()->code();
}

SMI_ACCESSORS(BreakPointInfo, source_position, kSourcePositionIndex)
ACCESSORS(BreakPointInfo, break_point_objects, Object, kBreakPointObjectsIndex)

SMI_ACCESSORS(StackFrameInfo, line_number, kLineNumberIndex)
SMI_ACCESSORS(StackFrameInfo, column_number, kColumnNumberIndex)
SMI_ACCESSORS(StackFrameInfo, script_id, kScriptIdIndex)
ACCESSORS(StackFrameInfo, script_name, Object, kScriptNameIndex)
ACCESSORS(StackFrameInfo, script_name_or_source_url, Object,
          kScriptNameOrSourceUrlIndex)
ACCESSORS(StackFrameInfo, function_name, Object, kFunctionNameIndex)
SMI_ACCESSORS(StackFrameInfo, flag, kFlagIndex)
BOOL_ACCESSORS(StackFrameInfo, flag, is_eval, kIsEvalBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_constructor, kIsConstructorBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_wasm, kIsWasmBit)
SMI_ACCESSORS(StackFrameInfo, id, kIdIndex)

ACCESSORS(SourcePositionTableWithFrameCache, source_position_table, ByteArray,
          kSourcePositionTableIndex)
ACCESSORS(SourcePositionTableWithFrameCache, stack_frame_cache,
          UnseededNumberDictionary, kStackFrameCacheIndex)

ACCESSORS(SharedFunctionInfo, name, Object, kNameOffset)
ACCESSORS(SharedFunctionInfo, construct_stub, Code, kConstructStubOffset)
ACCESSORS(SharedFunctionInfo, feedback_metadata, FeedbackMetadata,
          kFeedbackMetadataOffset)
SMI_ACCESSORS(SharedFunctionInfo, function_literal_id, kFunctionLiteralIdOffset)
#if V8_SFI_HAS_UNIQUE_ID
SMI_ACCESSORS(SharedFunctionInfo, unique_id, kUniqueIdOffset)
#endif
ACCESSORS(SharedFunctionInfo, instance_class_name, Object,
          kInstanceClassNameOffset)
ACCESSORS(SharedFunctionInfo, function_data, Object, kFunctionDataOffset)
ACCESSORS(SharedFunctionInfo, script, Object, kScriptOffset)
ACCESSORS(SharedFunctionInfo, debug_info, Object, kDebugInfoOffset)
ACCESSORS(SharedFunctionInfo, function_identifier, Object,
          kFunctionIdentifierOffset)

SMI_ACCESSORS(FunctionTemplateInfo, length, kLengthOffset)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, hidden_prototype,
               kHiddenPrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, undetectable, kUndetectableBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, needs_access_check,
               kNeedsAccessCheckBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, read_only_prototype,
               kReadOnlyPrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, remove_prototype,
               kRemovePrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, do_not_cache,
               kDoNotCacheBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, accept_any_receiver,
               kAcceptAnyReceiver)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_named_expression,
               kIsNamedExpressionBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_toplevel,
               kIsTopLevelBit)

#if V8_HOST_ARCH_32_BIT
SMI_ACCESSORS(SharedFunctionInfo, length, kLengthOffset)
SMI_ACCESSORS(SharedFunctionInfo, internal_formal_parameter_count,
              kFormalParameterCountOffset)
SMI_ACCESSORS(SharedFunctionInfo, expected_nof_properties,
              kExpectedNofPropertiesOffset)
SMI_ACCESSORS(SharedFunctionInfo, start_position_and_type,
              kStartPositionAndTypeOffset)
SMI_ACCESSORS(SharedFunctionInfo, end_position, kEndPositionOffset)
SMI_ACCESSORS(SharedFunctionInfo, function_token_position,
              kFunctionTokenPositionOffset)
SMI_ACCESSORS(SharedFunctionInfo, compiler_hints,
              kCompilerHintsOffset)
SMI_ACCESSORS(SharedFunctionInfo, opt_count_and_bailout_reason,
              kOptCountAndBailoutReasonOffset)
SMI_ACCESSORS(SharedFunctionInfo, counters, kCountersOffset)
SMI_ACCESSORS(SharedFunctionInfo, ast_node_count, kAstNodeCountOffset)
SMI_ACCESSORS(SharedFunctionInfo, profiler_ticks, kProfilerTicksOffset)

#else

#if V8_TARGET_LITTLE_ENDIAN
#define PSEUDO_SMI_LO_ALIGN 0
#define PSEUDO_SMI_HI_ALIGN kIntSize
#else
#define PSEUDO_SMI_LO_ALIGN kIntSize
#define PSEUDO_SMI_HI_ALIGN 0
#endif

#define PSEUDO_SMI_ACCESSORS_LO(holder, name, offset)                          \
  STATIC_ASSERT(holder::offset % kPointerSize == PSEUDO_SMI_LO_ALIGN);         \
  int holder::name() const {                                                   \
    int value = READ_INT_FIELD(this, offset);                                  \
    DCHECK(kHeapObjectTag == 1);                                               \
    DCHECK((value & kHeapObjectTag) == 0);                                     \
    return value >> 1;                                                         \
  }                                                                            \
  void holder::set_##name(int value) {                                         \
    DCHECK(kHeapObjectTag == 1);                                               \
    DCHECK((value & 0xC0000000) == 0xC0000000 || (value & 0xC0000000) == 0x0); \
    WRITE_INT_FIELD(this, offset, (value << 1) & ~kHeapObjectTag);             \
  }

#define PSEUDO_SMI_ACCESSORS_HI(holder, name, offset)                  \
  STATIC_ASSERT(holder::offset % kPointerSize == PSEUDO_SMI_HI_ALIGN); \
  INT_ACCESSORS(holder, name, offset)


PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo, length, kLengthOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo, internal_formal_parameter_count,
                        kFormalParameterCountOffset)

PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo,
                        expected_nof_properties,
                        kExpectedNofPropertiesOffset)

PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo, end_position, kEndPositionOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo,
                        start_position_and_type,
                        kStartPositionAndTypeOffset)

PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo,
                        function_token_position,
                        kFunctionTokenPositionOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo,
                        compiler_hints,
                        kCompilerHintsOffset)

PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo,
                        opt_count_and_bailout_reason,
                        kOptCountAndBailoutReasonOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo, counters, kCountersOffset)

PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo,
                        ast_node_count,
                        kAstNodeCountOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo,
                        profiler_ticks,
                        kProfilerTicksOffset)

#endif

AbstractCode* SharedFunctionInfo::abstract_code() {
  if (HasBytecodeArray()) {
    return AbstractCode::cast(bytecode_array());
  } else {
    return AbstractCode::cast(code());
  }
}

BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, allows_lazy_compilation,
               kAllowLazyCompilation)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, uses_arguments,
               kUsesArguments)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, has_duplicate_parameters,
               kHasDuplicateParameters)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, asm_function, kIsAsmFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_declaration,
               kIsDeclaration)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, marked_for_tier_up,
               kMarkedForTierUp)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints,
               has_concurrent_optimization_job, kHasConcurrentOptimizationJob)

BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, needs_home_object,
               kNeedsHomeObject)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, native, kNative)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, force_inline, kForceInline)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, must_use_ignition_turbo,
               kMustUseIgnitionTurbo)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_flush, kDontFlush)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_asm_wasm_broken,
               kIsAsmWasmBroken)

BOOL_GETTER(SharedFunctionInfo, compiler_hints, optimization_disabled,
            kOptimizationDisabled)

void SharedFunctionInfo::set_optimization_disabled(bool disable) {
  set_compiler_hints(BooleanBit::set(compiler_hints(),
                                     kOptimizationDisabled,
                                     disable));
}

LanguageMode SharedFunctionInfo::language_mode() {
  STATIC_ASSERT(LANGUAGE_END == 2);
  return construct_language_mode(
      BooleanBit::get(compiler_hints(), kStrictModeFunction));
}

void SharedFunctionInfo::set_language_mode(LanguageMode language_mode) {
  STATIC_ASSERT(LANGUAGE_END == 2);
  // We only allow language mode transitions that set the same language mode
  // again or go up in the chain:
  DCHECK(is_sloppy(this->language_mode()) || is_strict(language_mode));
  int hints = compiler_hints();
  hints = BooleanBit::set(hints, kStrictModeFunction, is_strict(language_mode));
  set_compiler_hints(hints);
}

FunctionKind SharedFunctionInfo::kind() const {
  return FunctionKindBits::decode(compiler_hints());
}

void SharedFunctionInfo::set_kind(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  int hints = compiler_hints();
  hints = FunctionKindBits::update(hints, kind);
  set_compiler_hints(hints);
}

BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints,
               name_should_print_as_anonymous, kNameShouldPrintAsAnonymous)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, is_anonymous_expression,
               kIsAnonymousExpression)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, deserialized, kDeserialized)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, has_no_side_effect,
               kHasNoSideEffect)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, computed_has_no_side_effect,
               kComputedHasNoSideEffect)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, debug_is_blackboxed,
               kDebugIsBlackboxed)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, computed_debug_is_blackboxed,
               kComputedDebugIsBlackboxed)
BOOL_ACCESSORS(SharedFunctionInfo, debugger_hints, has_reported_binary_coverage,
               kHasReportedBinaryCoverage)

bool Script::HasValidSource() {
  Object* src = this->source();
  if (!src->IsString()) return true;
  String* src_str = String::cast(src);
  if (!StringShape(src_str).IsExternal()) return true;
  if (src_str->IsOneByteRepresentation()) {
    return ExternalOneByteString::cast(src)->resource() != NULL;
  } else if (src_str->IsTwoByteRepresentation()) {
    return ExternalTwoByteString::cast(src)->resource() != NULL;
  }
  return true;
}


void SharedFunctionInfo::DontAdaptArguments() {
  DCHECK(code()->kind() == Code::BUILTIN || code()->kind() == Code::STUB);
  set_internal_formal_parameter_count(kDontAdaptArgumentsSentinel);
}


int SharedFunctionInfo::start_position() const {
  return start_position_and_type() >> kStartPositionShift;
}


void SharedFunctionInfo::set_start_position(int start_position) {
  set_start_position_and_type((start_position << kStartPositionShift)
    | (start_position_and_type() & ~kStartPositionMask));
}


Code* SharedFunctionInfo::code() const {
  return Code::cast(READ_FIELD(this, kCodeOffset));
}


void SharedFunctionInfo::set_code(Code* value, WriteBarrierMode mode) {
  DCHECK(value->kind() != Code::OPTIMIZED_FUNCTION);
  // If the SharedFunctionInfo has bytecode we should never mark it for lazy
  // compile, since the bytecode is never flushed.
  DCHECK(value != GetIsolate()->builtins()->builtin(Builtins::kCompileLazy) ||
         !HasBytecodeArray());
  WRITE_FIELD(this, kCodeOffset, value);
  CONDITIONAL_WRITE_BARRIER(value->GetHeap(), this, kCodeOffset, value, mode);
}


void SharedFunctionInfo::ReplaceCode(Code* value) {
  // If the GC metadata field is already used then the function was
  // enqueued as a code flushing candidate and we remove it now.
  if (code()->gc_metadata() != NULL) {
    CodeFlusher* flusher = GetHeap()->mark_compact_collector()->code_flusher();
    flusher->EvictCandidate(this);
  }

  DCHECK(code()->gc_metadata() == NULL && value->gc_metadata() == NULL);
#ifdef DEBUG
  Code::VerifyRecompiledCode(code(), value);
#endif  // DEBUG

  set_code(value);
}

bool SharedFunctionInfo::IsInterpreted() const {
  return code()->is_interpreter_trampoline_builtin();
}

bool SharedFunctionInfo::HasBaselineCode() const {
  return code()->kind() == Code::FUNCTION;
}

ScopeInfo* SharedFunctionInfo::scope_info() const {
  return reinterpret_cast<ScopeInfo*>(READ_FIELD(this, kScopeInfoOffset));
}


void SharedFunctionInfo::set_scope_info(ScopeInfo* value,
                                        WriteBarrierMode mode) {
  WRITE_FIELD(this, kScopeInfoOffset, reinterpret_cast<Object*>(value));
  CONDITIONAL_WRITE_BARRIER(GetHeap(),
                            this,
                            kScopeInfoOffset,
                            reinterpret_cast<Object*>(value),
                            mode);
}

ACCESSORS(SharedFunctionInfo, outer_scope_info, HeapObject,
          kOuterScopeInfoOffset)

bool SharedFunctionInfo::is_compiled() const {
  Builtins* builtins = GetIsolate()->builtins();
  DCHECK(code() != builtins->builtin(Builtins::kCompileOptimizedConcurrent));
  DCHECK(code() != builtins->builtin(Builtins::kCompileOptimized));
  return code() != builtins->builtin(Builtins::kCompileLazy);
}

int SharedFunctionInfo::GetLength() const {
  DCHECK(is_compiled());
  DCHECK(HasLength());
  return length();
}

bool SharedFunctionInfo::HasLength() const {
  DCHECK_IMPLIES(length() < 0, length() == kInvalidLength);
  return length() != kInvalidLength;
}

bool SharedFunctionInfo::has_simple_parameters() {
  return scope_info()->HasSimpleParameters();
}

bool SharedFunctionInfo::HasDebugInfo() const {
  bool has_debug_info = !debug_info()->IsSmi();
  DCHECK_EQ(debug_info()->IsStruct(), has_debug_info);
  DCHECK(!has_debug_info || HasDebugCode());
  return has_debug_info;
}

DebugInfo* SharedFunctionInfo::GetDebugInfo() const {
  DCHECK(HasDebugInfo());
  return DebugInfo::cast(debug_info());
}

bool SharedFunctionInfo::HasDebugCode() const {
  if (HasBaselineCode()) return code()->has_debug_break_slots();
  return HasBytecodeArray();
}

int SharedFunctionInfo::debugger_hints() const {
  if (HasDebugInfo()) return GetDebugInfo()->debugger_hints();
  return Smi::cast(debug_info())->value();
}

void SharedFunctionInfo::set_debugger_hints(int value) {
  if (HasDebugInfo()) {
    GetDebugInfo()->set_debugger_hints(value);
  } else {
    set_debug_info(Smi::FromInt(value));
  }
}

bool SharedFunctionInfo::IsApiFunction() {
  return function_data()->IsFunctionTemplateInfo();
}


FunctionTemplateInfo* SharedFunctionInfo::get_api_func_data() {
  DCHECK(IsApiFunction());
  return FunctionTemplateInfo::cast(function_data());
}

void SharedFunctionInfo::set_api_func_data(FunctionTemplateInfo* data) {
  DCHECK(function_data()->IsUndefined(GetIsolate()));
  set_function_data(data);
}

bool SharedFunctionInfo::HasBytecodeArray() const {
  return function_data()->IsBytecodeArray();
}

BytecodeArray* SharedFunctionInfo::bytecode_array() const {
  DCHECK(HasBytecodeArray());
  return BytecodeArray::cast(function_data());
}

void SharedFunctionInfo::set_bytecode_array(BytecodeArray* bytecode) {
  DCHECK(function_data()->IsUndefined(GetIsolate()));
  set_function_data(bytecode);
}

void SharedFunctionInfo::ClearBytecodeArray() {
  DCHECK(function_data()->IsUndefined(GetIsolate()) || HasBytecodeArray());
  set_function_data(GetHeap()->undefined_value());
}

bool SharedFunctionInfo::HasAsmWasmData() const {
  return function_data()->IsFixedArray();
}

FixedArray* SharedFunctionInfo::asm_wasm_data() const {
  DCHECK(HasAsmWasmData());
  return FixedArray::cast(function_data());
}

void SharedFunctionInfo::set_asm_wasm_data(FixedArray* data) {
  DCHECK(function_data()->IsUndefined(GetIsolate()) || HasAsmWasmData());
  set_function_data(data);
}

void SharedFunctionInfo::ClearAsmWasmData() {
  DCHECK(function_data()->IsUndefined(GetIsolate()) || HasAsmWasmData());
  set_function_data(GetHeap()->undefined_value());
}

bool SharedFunctionInfo::HasBuiltinFunctionId() {
  return function_identifier()->IsSmi();
}

BuiltinFunctionId SharedFunctionInfo::builtin_function_id() {
  DCHECK(HasBuiltinFunctionId());
  return static_cast<BuiltinFunctionId>(
      Smi::cast(function_identifier())->value());
}

void SharedFunctionInfo::set_builtin_function_id(BuiltinFunctionId id) {
  set_function_identifier(Smi::FromInt(id));
}

bool SharedFunctionInfo::HasInferredName() {
  return function_identifier()->IsString();
}

String* SharedFunctionInfo::inferred_name() {
  if (HasInferredName()) {
    return String::cast(function_identifier());
  }
  Isolate* isolate = GetIsolate();
  DCHECK(function_identifier()->IsUndefined(isolate) || HasBuiltinFunctionId());
  return isolate->heap()->empty_string();
}

void SharedFunctionInfo::set_inferred_name(String* inferred_name) {
  DCHECK(function_identifier()->IsUndefined(GetIsolate()) || HasInferredName());
  set_function_identifier(inferred_name);
}

int SharedFunctionInfo::ic_age() {
  return ICAgeBits::decode(counters());
}


void SharedFunctionInfo::set_ic_age(int ic_age) {
  set_counters(ICAgeBits::update(counters(), ic_age));
}


int SharedFunctionInfo::deopt_count() {
  return DeoptCountBits::decode(counters());
}


void SharedFunctionInfo::set_deopt_count(int deopt_count) {
  set_counters(DeoptCountBits::update(counters(), deopt_count));
}


void SharedFunctionInfo::increment_deopt_count() {
  int value = counters();
  int deopt_count = DeoptCountBits::decode(value);
  // Saturate the deopt count when incrementing, rather than overflowing.
  if (deopt_count < DeoptCountBits::kMax) {
    set_counters(DeoptCountBits::update(value, deopt_count + 1));
  }
}


int SharedFunctionInfo::opt_reenable_tries() {
  return OptReenableTriesBits::decode(counters());
}


void SharedFunctionInfo::set_opt_reenable_tries(int tries) {
  set_counters(OptReenableTriesBits::update(counters(), tries));
}


int SharedFunctionInfo::opt_count() {
  return OptCountBits::decode(opt_count_and_bailout_reason());
}


void SharedFunctionInfo::set_opt_count(int opt_count) {
  set_opt_count_and_bailout_reason(
      OptCountBits::update(opt_count_and_bailout_reason(), opt_count));
}


BailoutReason SharedFunctionInfo::disable_optimization_reason() {
  return static_cast<BailoutReason>(
      DisabledOptimizationReasonBits::decode(opt_count_and_bailout_reason()));
}


bool SharedFunctionInfo::has_deoptimization_support() {
  Code* code = this->code();
  return code->kind() == Code::FUNCTION && code->has_deoptimization_support();
}


void SharedFunctionInfo::TryReenableOptimization() {
  int tries = opt_reenable_tries();
  set_opt_reenable_tries((tries + 1) & OptReenableTriesBits::kMax);
  // We reenable optimization whenever the number of tries is a large
  // enough power of 2.
  if (tries >= 16 && (((tries - 1) & tries) == 0)) {
    set_optimization_disabled(false);
    set_deopt_count(0);
  }
}


void SharedFunctionInfo::set_disable_optimization_reason(BailoutReason reason) {
  set_opt_count_and_bailout_reason(DisabledOptimizationReasonBits::update(
      opt_count_and_bailout_reason(), reason));
}

bool SharedFunctionInfo::IsUserJavaScript() {
  Object* script_obj = script();
  if (script_obj->IsUndefined(GetIsolate())) return false;
  Script* script = Script::cast(script_obj);
  return script->IsUserJavaScript();
}

bool SharedFunctionInfo::IsSubjectToDebugging() {
  return IsUserJavaScript() && !HasAsmWasmData();
}

FeedbackVector* JSFunction::feedback_vector() const {
  DCHECK(feedback_vector_cell()->value()->IsFeedbackVector());
  return FeedbackVector::cast(feedback_vector_cell()->value());
}

bool JSFunction::IsOptimized() {
  return code()->kind() == Code::OPTIMIZED_FUNCTION;
}

bool JSFunction::IsInterpreted() {
  return code()->is_interpreter_trampoline_builtin();
}

bool JSFunction::IsMarkedForOptimization() {
  return code() == GetIsolate()->builtins()->builtin(
      Builtins::kCompileOptimized);
}


bool JSFunction::IsMarkedForConcurrentOptimization() {
  return code() == GetIsolate()->builtins()->builtin(
      Builtins::kCompileOptimizedConcurrent);
}


bool JSFunction::IsInOptimizationQueue() {
  return code() == GetIsolate()->builtins()->builtin(
      Builtins::kInOptimizationQueue);
}


void JSFunction::CompleteInobjectSlackTrackingIfActive() {
  if (has_initial_map() && initial_map()->IsInobjectSlackTrackingInProgress()) {
    initial_map()->CompleteInobjectSlackTracking();
  }
}


bool Map::IsInobjectSlackTrackingInProgress() {
  return construction_counter() != Map::kNoSlackTracking;
}


void Map::InobjectSlackTrackingStep() {
  if (!IsInobjectSlackTrackingInProgress()) return;
  int counter = construction_counter();
  set_construction_counter(counter - 1);
  if (counter == kSlackTrackingCounterEnd) {
    CompleteInobjectSlackTracking();
  }
}

AbstractCode* JSFunction::abstract_code() {
  if (IsInterpreted()) {
    return AbstractCode::cast(shared()->bytecode_array());
  } else {
    return AbstractCode::cast(code());
  }
}

Code* JSFunction::code() {
  return Code::cast(
      Code::GetObjectFromEntryAddress(FIELD_ADDR(this, kCodeEntryOffset)));
}


void JSFunction::set_code(Code* value) {
  DCHECK(!GetHeap()->InNewSpace(value));
  Address entry = value->entry();
  WRITE_INTPTR_FIELD(this, kCodeEntryOffset, reinterpret_cast<intptr_t>(entry));
  GetHeap()->incremental_marking()->RecordWriteOfCodeEntry(
      this,
      HeapObject::RawField(this, kCodeEntryOffset),
      value);
}


void JSFunction::set_code_no_write_barrier(Code* value) {
  DCHECK(!GetHeap()->InNewSpace(value));
  Address entry = value->entry();
  WRITE_INTPTR_FIELD(this, kCodeEntryOffset, reinterpret_cast<intptr_t>(entry));
}

void JSFunction::ClearOptimizedCodeSlot(const char* reason) {
  if (has_feedback_vector() && feedback_vector()->has_optimized_code()) {
    if (FLAG_trace_opt) {
      PrintF("[evicting entry from optimizing code feedback slot (%s) for ",
             reason);
      shared()->ShortPrint();
      PrintF("]\n");
    }
    feedback_vector()->ClearOptimizedCode();
  }
}

void JSFunction::ReplaceCode(Code* code) {
  bool was_optimized = IsOptimized();
  bool is_optimized = code->kind() == Code::OPTIMIZED_FUNCTION;

  if (was_optimized && is_optimized) {
    ClearOptimizedCodeSlot("Replacing with another optimized code");
  }

  set_code(code);

  // Add/remove the function from the list of optimized functions for this
  // context based on the state change.
  if (!was_optimized && is_optimized) {
    context()->native_context()->AddOptimizedFunction(this);
  }
  if (was_optimized && !is_optimized) {
    // TODO(titzer): linear in the number of optimized functions; fix!
    context()->native_context()->RemoveOptimizedFunction(this);
  }
}

bool JSFunction::has_feedback_vector() const {
  return !feedback_vector_cell()->value()->IsUndefined(GetIsolate());
}

JSFunction::FeedbackVectorState JSFunction::GetFeedbackVectorState(
    Isolate* isolate) const {
  Cell* cell = feedback_vector_cell();
  if (cell == isolate->heap()->undefined_cell()) {
    return TOP_LEVEL_SCRIPT_NEEDS_VECTOR;
  } else if (cell->value() == isolate->heap()->undefined_value() ||
             !has_feedback_vector()) {
    return NEEDS_VECTOR;
  }
  return HAS_VECTOR;
}

Context* JSFunction::context() {
  return Context::cast(READ_FIELD(this, kContextOffset));
}

bool JSFunction::has_context() const {
  return READ_FIELD(this, kContextOffset)->IsContext();
}

JSObject* JSFunction::global_proxy() {
  return context()->global_proxy();
}


Context* JSFunction::native_context() { return context()->native_context(); }


void JSFunction::set_context(Object* value) {
  DCHECK(value->IsUndefined(GetIsolate()) || value->IsContext());
  WRITE_FIELD(this, kContextOffset, value);
  WRITE_BARRIER(GetHeap(), this, kContextOffset, value);
}

ACCESSORS(JSFunction, prototype_or_initial_map, Object,
          kPrototypeOrInitialMapOffset)


Map* JSFunction::initial_map() {
  return Map::cast(prototype_or_initial_map());
}


bool JSFunction::has_initial_map() {
  return prototype_or_initial_map()->IsMap();
}


bool JSFunction::has_instance_prototype() {
  return has_initial_map() ||
         !prototype_or_initial_map()->IsTheHole(GetIsolate());
}


bool JSFunction::has_prototype() {
  return map()->has_non_instance_prototype() || has_instance_prototype();
}


Object* JSFunction::instance_prototype() {
  DCHECK(has_instance_prototype());
  if (has_initial_map()) return initial_map()->prototype();
  // When there is no initial map and the prototype is a JSObject, the
  // initial map field is used for the prototype field.
  return prototype_or_initial_map();
}


Object* JSFunction::prototype() {
  DCHECK(has_prototype());
  // If the function's prototype property has been set to a non-JSObject
  // value, that value is stored in the constructor field of the map.
  if (map()->has_non_instance_prototype()) {
    Object* prototype = map()->GetConstructor();
    // The map must have a prototype in that field, not a back pointer.
    DCHECK(!prototype->IsMap());
    DCHECK(!prototype->IsFunctionTemplateInfo());
    return prototype;
  }
  return instance_prototype();
}


bool JSFunction::is_compiled() {
  Builtins* builtins = GetIsolate()->builtins();
  return code() != builtins->builtin(Builtins::kCompileLazy) &&
         code() != builtins->builtin(Builtins::kCompileOptimized) &&
         code() != builtins->builtin(Builtins::kCompileOptimizedConcurrent);
}

ACCESSORS(JSProxy, target, JSReceiver, kTargetOffset)
ACCESSORS(JSProxy, handler, Object, kHandlerOffset)
ACCESSORS(JSProxy, hash, Object, kHashOffset)

bool JSProxy::IsRevoked() const { return !handler()->IsJSReceiver(); }

ACCESSORS(JSCollection, table, Object, kTableOffset)


#define ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(name, type, offset)    \
  template<class Derived, class TableType>                           \
  type* OrderedHashTableIterator<Derived, TableType>::name() const { \
    return type::cast(READ_FIELD(this, offset));                     \
  }                                                                  \
  template<class Derived, class TableType>                           \
  void OrderedHashTableIterator<Derived, TableType>::set_##name(     \
      type* value, WriteBarrierMode mode) {                          \
    WRITE_FIELD(this, offset, value);                                \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode); \
  }

ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(table, Object, kTableOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(index, Object, kIndexOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(kind, Object, kKindOffset)

#undef ORDERED_HASH_TABLE_ITERATOR_ACCESSORS


ACCESSORS(JSWeakCollection, table, Object, kTableOffset)
ACCESSORS(JSWeakCollection, next, Object, kNextOffset)


Address Foreign::foreign_address() {
  return AddressFrom<Address>(READ_INTPTR_FIELD(this, kForeignAddressOffset));
}


void Foreign::set_foreign_address(Address value) {
  WRITE_INTPTR_FIELD(this, kForeignAddressOffset, OffsetFrom(value));
}


ACCESSORS(JSGeneratorObject, function, JSFunction, kFunctionOffset)
ACCESSORS(JSGeneratorObject, context, Context, kContextOffset)
ACCESSORS(JSGeneratorObject, receiver, Object, kReceiverOffset)
ACCESSORS(JSGeneratorObject, input_or_debug_pos, Object, kInputOrDebugPosOffset)
SMI_ACCESSORS(JSGeneratorObject, resume_mode, kResumeModeOffset)
SMI_ACCESSORS(JSGeneratorObject, continuation, kContinuationOffset)
ACCESSORS(JSGeneratorObject, register_file, FixedArray, kRegisterFileOffset)

bool JSGeneratorObject::is_suspended() const {
  DCHECK_LT(kGeneratorExecuting, 0);
  DCHECK_LT(kGeneratorClosed, 0);
  return continuation() >= 0;
}

bool JSGeneratorObject::is_closed() const {
  return continuation() == kGeneratorClosed;
}

bool JSGeneratorObject::is_executing() const {
  return continuation() == kGeneratorExecuting;
}

ACCESSORS(JSAsyncGeneratorObject, queue, HeapObject, kQueueOffset)
ACCESSORS(JSAsyncGeneratorObject, await_input_or_debug_pos, Object,
          kAwaitInputOrDebugPosOffset)
ACCESSORS(JSAsyncGeneratorObject, awaited_promise, HeapObject,
          kAwaitedPromiseOffset)

ACCESSORS(JSValue, value, Object, kValueOffset)


HeapNumber* HeapNumber::cast(Object* object) {
  SLOW_DCHECK(object->IsHeapNumber() || object->IsMutableHeapNumber());
  return reinterpret_cast<HeapNumber*>(object);
}


const HeapNumber* HeapNumber::cast(const Object* object) {
  SLOW_DCHECK(object->IsHeapNumber() || object->IsMutableHeapNumber());
  return reinterpret_cast<const HeapNumber*>(object);
}


ACCESSORS(JSDate, value, Object, kValueOffset)
ACCESSORS(JSDate, cache_stamp, Object, kCacheStampOffset)
ACCESSORS(JSDate, year, Object, kYearOffset)
ACCESSORS(JSDate, month, Object, kMonthOffset)
ACCESSORS(JSDate, day, Object, kDayOffset)
ACCESSORS(JSDate, weekday, Object, kWeekdayOffset)
ACCESSORS(JSDate, hour, Object, kHourOffset)
ACCESSORS(JSDate, min, Object, kMinOffset)
ACCESSORS(JSDate, sec, Object, kSecOffset)


SMI_ACCESSORS(JSMessageObject, type, kTypeOffset)
ACCESSORS(JSMessageObject, argument, Object, kArgumentsOffset)
ACCESSORS(JSMessageObject, script, Object, kScriptOffset)
ACCESSORS(JSMessageObject, stack_frames, Object, kStackFramesOffset)
SMI_ACCESSORS(JSMessageObject, start_position, kStartPositionOffset)
SMI_ACCESSORS(JSMessageObject, end_position, kEndPositionOffset)
SMI_ACCESSORS(JSMessageObject, error_level, kErrorLevelOffset)

INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, prologue_offset, kPrologueOffset)
INT_ACCESSORS(Code, constant_pool_offset, kConstantPoolOffset)
#define CODE_ACCESSORS(name, type, offset)           \
  ACCESSORS_CHECKED2(Code, name, type, offset, true, \
                     !GetHeap()->InNewSpace(value))
CODE_ACCESSORS(relocation_info, ByteArray, kRelocationInfoOffset)
CODE_ACCESSORS(handler_table, FixedArray, kHandlerTableOffset)
CODE_ACCESSORS(deoptimization_data, FixedArray, kDeoptimizationDataOffset)
CODE_ACCESSORS(source_position_table, Object, kSourcePositionTableOffset)
CODE_ACCESSORS(trap_handler_index, Smi, kTrapHandlerIndex)
CODE_ACCESSORS(raw_type_feedback_info, Object, kTypeFeedbackInfoOffset)
CODE_ACCESSORS(next_code_link, Object, kNextCodeLinkOffset)
#undef CODE_ACCESSORS

void Code::WipeOutHeader() {
  WRITE_FIELD(this, kRelocationInfoOffset, NULL);
  WRITE_FIELD(this, kHandlerTableOffset, NULL);
  WRITE_FIELD(this, kDeoptimizationDataOffset, NULL);
  WRITE_FIELD(this, kSourcePositionTableOffset, NULL);
  // Do not wipe out major/minor keys on a code stub or IC
  if (!READ_FIELD(this, kTypeFeedbackInfoOffset)->IsSmi()) {
    WRITE_FIELD(this, kTypeFeedbackInfoOffset, NULL);
  }
  WRITE_FIELD(this, kNextCodeLinkOffset, NULL);
  WRITE_FIELD(this, kGCMetadataOffset, NULL);
}


Object* Code::type_feedback_info() {
  DCHECK(kind() == FUNCTION);
  return raw_type_feedback_info();
}


void Code::set_type_feedback_info(Object* value, WriteBarrierMode mode) {
  DCHECK(kind() == FUNCTION);
  set_raw_type_feedback_info(value, mode);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kTypeFeedbackInfoOffset,
                            value, mode);
}

ByteArray* Code::SourcePositionTable() {
  Object* maybe_table = source_position_table();
  if (maybe_table->IsByteArray()) return ByteArray::cast(maybe_table);
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  return SourcePositionTableWithFrameCache::cast(maybe_table)
      ->source_position_table();
}

uint32_t Code::stub_key() {
  DCHECK(IsCodeStubOrIC());
  Smi* smi_key = Smi::cast(raw_type_feedback_info());
  return static_cast<uint32_t>(smi_key->value());
}


void Code::set_stub_key(uint32_t key) {
  DCHECK(IsCodeStubOrIC());
  set_raw_type_feedback_info(Smi::FromInt(key));
}


ACCESSORS(Code, gc_metadata, Object, kGCMetadataOffset)
INT_ACCESSORS(Code, ic_age, kICAgeOffset)


byte* Code::instruction_start()  {
  return FIELD_ADDR(this, kHeaderSize);
}


byte* Code::instruction_end()  {
  return instruction_start() + instruction_size();
}

int Code::GetUnwindingInfoSizeOffset() const {
  DCHECK(has_unwinding_info());
  return RoundUp(kHeaderSize + instruction_size(), kInt64Size);
}

int Code::unwinding_info_size() const {
  DCHECK(has_unwinding_info());
  return static_cast<int>(
      READ_UINT64_FIELD(this, GetUnwindingInfoSizeOffset()));
}

void Code::set_unwinding_info_size(int value) {
  DCHECK(has_unwinding_info());
  WRITE_UINT64_FIELD(this, GetUnwindingInfoSizeOffset(), value);
}

byte* Code::unwinding_info_start() {
  DCHECK(has_unwinding_info());
  return FIELD_ADDR(this, GetUnwindingInfoSizeOffset()) + kInt64Size;
}

byte* Code::unwinding_info_end() {
  DCHECK(has_unwinding_info());
  return unwinding_info_start() + unwinding_info_size();
}

int Code::body_size() {
  int unpadded_body_size =
      has_unwinding_info()
          ? static_cast<int>(unwinding_info_end() - instruction_start())
          : instruction_size();
  return RoundUp(unpadded_body_size, kObjectAlignment);
}

int Code::SizeIncludingMetadata() {
  int size = CodeSize();
  size += relocation_info()->Size();
  size += deoptimization_data()->Size();
  size += handler_table()->Size();
  if (kind() == FUNCTION) {
    size += SourcePositionTable()->Size();
  }
  return size;
}

ByteArray* Code::unchecked_relocation_info() {
  return reinterpret_cast<ByteArray*>(READ_FIELD(this, kRelocationInfoOffset));
}


byte* Code::relocation_start() {
  return unchecked_relocation_info()->GetDataStartAddress();
}


int Code::relocation_size() {
  return unchecked_relocation_info()->length();
}


byte* Code::entry() {
  return instruction_start();
}


bool Code::contains(byte* inner_pointer) {
  return (address() <= inner_pointer) && (inner_pointer <= address() + Size());
}


int Code::ExecutableSize() {
  // Check that the assumptions about the layout of the code object holds.
  DCHECK_EQ(static_cast<int>(instruction_start() - address()),
            Code::kHeaderSize);
  return instruction_size() + Code::kHeaderSize;
}


int Code::CodeSize() { return SizeFor(body_size()); }


ACCESSORS(JSArray, length, Object, kLengthOffset)


void* JSArrayBuffer::backing_store() const {
  intptr_t ptr = READ_INTPTR_FIELD(this, kBackingStoreOffset);
  return reinterpret_cast<void*>(ptr);
}


void JSArrayBuffer::set_backing_store(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kBackingStoreOffset, ptr);
}


ACCESSORS(JSArrayBuffer, byte_length, Object, kByteLengthOffset)

void* JSArrayBuffer::allocation_base() const {
  intptr_t ptr = READ_INTPTR_FIELD(this, kAllocationBaseOffset);
  return reinterpret_cast<void*>(ptr);
}

void JSArrayBuffer::set_allocation_base(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kAllocationBaseOffset, ptr);
}

size_t JSArrayBuffer::allocation_length() const {
  return *reinterpret_cast<const size_t*>(
      FIELD_ADDR_CONST(this, kAllocationLengthOffset));
}

void JSArrayBuffer::set_allocation_length(size_t value) {
  (*reinterpret_cast<size_t*>(FIELD_ADDR(this, kAllocationLengthOffset))) =
      value;
}

ArrayBuffer::Allocator::AllocationMode JSArrayBuffer::allocation_mode() const {
  using AllocationMode = ArrayBuffer::Allocator::AllocationMode;
  return has_guard_region() ? AllocationMode::kReservation
                            : AllocationMode::kNormal;
}

void JSArrayBuffer::set_bit_field(uint32_t bits) {
  if (kInt32Size != kPointerSize) {
#if V8_TARGET_LITTLE_ENDIAN
    WRITE_UINT32_FIELD(this, kBitFieldSlot + kInt32Size, 0);
#else
    WRITE_UINT32_FIELD(this, kBitFieldSlot, 0);
#endif
  }
  WRITE_UINT32_FIELD(this, kBitFieldOffset, bits);
}


uint32_t JSArrayBuffer::bit_field() const {
  return READ_UINT32_FIELD(this, kBitFieldOffset);
}


bool JSArrayBuffer::is_external() { return IsExternal::decode(bit_field()); }


void JSArrayBuffer::set_is_external(bool value) {
  set_bit_field(IsExternal::update(bit_field(), value));
}


bool JSArrayBuffer::is_neuterable() {
  return IsNeuterable::decode(bit_field());
}


void JSArrayBuffer::set_is_neuterable(bool value) {
  set_bit_field(IsNeuterable::update(bit_field(), value));
}


bool JSArrayBuffer::was_neutered() { return WasNeutered::decode(bit_field()); }


void JSArrayBuffer::set_was_neutered(bool value) {
  set_bit_field(WasNeutered::update(bit_field(), value));
}


bool JSArrayBuffer::is_shared() { return IsShared::decode(bit_field()); }


void JSArrayBuffer::set_is_shared(bool value) {
  set_bit_field(IsShared::update(bit_field(), value));
}

bool JSArrayBuffer::has_guard_region() const {
  return HasGuardRegion::decode(bit_field());
}

void JSArrayBuffer::set_has_guard_region(bool value) {
  set_bit_field(HasGuardRegion::update(bit_field(), value));
}

bool JSArrayBuffer::is_wasm_buffer() {
  return IsWasmBuffer::decode(bit_field());
}

void JSArrayBuffer::set_is_wasm_buffer(bool value) {
  set_bit_field(IsWasmBuffer::update(bit_field(), value));
}

Object* JSArrayBufferView::byte_offset() const {
  if (WasNeutered()) return Smi::kZero;
  return Object::cast(READ_FIELD(this, kByteOffsetOffset));
}


void JSArrayBufferView::set_byte_offset(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kByteOffsetOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kByteOffsetOffset, value, mode);
}


Object* JSArrayBufferView::byte_length() const {
  if (WasNeutered()) return Smi::kZero;
  return Object::cast(READ_FIELD(this, kByteLengthOffset));
}


void JSArrayBufferView::set_byte_length(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kByteLengthOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kByteLengthOffset, value, mode);
}


ACCESSORS(JSArrayBufferView, buffer, Object, kBufferOffset)
#ifdef VERIFY_HEAP
ACCESSORS(JSArrayBufferView, raw_byte_offset, Object, kByteOffsetOffset)
ACCESSORS(JSArrayBufferView, raw_byte_length, Object, kByteLengthOffset)
#endif


bool JSArrayBufferView::WasNeutered() const {
  return JSArrayBuffer::cast(buffer())->was_neutered();
}


Object* JSTypedArray::length() const {
  if (WasNeutered()) return Smi::kZero;
  return Object::cast(READ_FIELD(this, kLengthOffset));
}


uint32_t JSTypedArray::length_value() const {
  if (WasNeutered()) return 0;
  uint32_t index = 0;
  CHECK(Object::cast(READ_FIELD(this, kLengthOffset))->ToArrayLength(&index));
  return index;
}


void JSTypedArray::set_length(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kLengthOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kLengthOffset, value, mode);
}

// static
MaybeHandle<JSTypedArray> JSTypedArray::Validate(Isolate* isolate,
                                                 Handle<Object> receiver,
                                                 const char* method_name) {
  if (V8_UNLIKELY(!receiver->IsJSTypedArray())) {
    const MessageTemplate::Template message = MessageTemplate::kNotTypedArray;
    THROW_NEW_ERROR(isolate, NewTypeError(message), JSTypedArray);
  }

  Handle<JSTypedArray> array = Handle<JSTypedArray>::cast(receiver);
  if (V8_UNLIKELY(array->WasNeutered())) {
    const MessageTemplate::Template message =
        MessageTemplate::kDetachedOperation;
    Handle<String> operation =
        isolate->factory()->NewStringFromAsciiChecked(method_name);
    THROW_NEW_ERROR(isolate, NewTypeError(message, operation), JSTypedArray);
  }

  // spec describes to return `buffer`, but it may disrupt current
  // implementations, and it's much useful to return array for now.
  return array;
}

#ifdef VERIFY_HEAP
ACCESSORS(JSTypedArray, raw_length, Object, kLengthOffset)
#endif

ACCESSORS(JSPromiseCapability, promise, Object, kPromiseOffset)
ACCESSORS(JSPromiseCapability, resolve, Object, kResolveOffset)
ACCESSORS(JSPromiseCapability, reject, Object, kRejectOffset)

SMI_ACCESSORS(JSPromise, status, kStatusOffset)
ACCESSORS(JSPromise, result, Object, kResultOffset)
ACCESSORS(JSPromise, deferred_promise, Object, kDeferredPromiseOffset)
ACCESSORS(JSPromise, deferred_on_resolve, Object, kDeferredOnResolveOffset)
ACCESSORS(JSPromise, deferred_on_reject, Object, kDeferredOnRejectOffset)
ACCESSORS(JSPromise, fulfill_reactions, Object, kFulfillReactionsOffset)
ACCESSORS(JSPromise, reject_reactions, Object, kRejectReactionsOffset)
SMI_ACCESSORS(JSPromise, flags, kFlagsOffset)
BOOL_ACCESSORS(JSPromise, flags, has_handler, kHasHandlerBit)
BOOL_ACCESSORS(JSPromise, flags, handled_hint, kHandledHintBit)

ACCESSORS(JSRegExp, data, Object, kDataOffset)
ACCESSORS(JSRegExp, flags, Object, kFlagsOffset)
ACCESSORS(JSRegExp, source, Object, kSourceOffset)


JSRegExp::Type JSRegExp::TypeTag() {
  Object* data = this->data();
  if (data->IsUndefined(GetIsolate())) return JSRegExp::NOT_COMPILED;
  Smi* smi = Smi::cast(FixedArray::cast(data)->get(kTagIndex));
  return static_cast<JSRegExp::Type>(smi->value());
}


int JSRegExp::CaptureCount() {
  switch (TypeTag()) {
    case ATOM:
      return 0;
    case IRREGEXP:
      return Smi::cast(DataAt(kIrregexpCaptureCountIndex))->value();
    default:
      UNREACHABLE();
      return -1;
  }
}


JSRegExp::Flags JSRegExp::GetFlags() {
  DCHECK(this->data()->IsFixedArray());
  Object* data = this->data();
  Smi* smi = Smi::cast(FixedArray::cast(data)->get(kFlagsIndex));
  return Flags(smi->value());
}


String* JSRegExp::Pattern() {
  DCHECK(this->data()->IsFixedArray());
  Object* data = this->data();
  String* pattern = String::cast(FixedArray::cast(data)->get(kSourceIndex));
  return pattern;
}

Object* JSRegExp::CaptureNameMap() {
  DCHECK(this->data()->IsFixedArray());
  DCHECK_EQ(TypeTag(), IRREGEXP);
  Object* value = DataAt(kIrregexpCaptureNameMapIndex);
  DCHECK_NE(value, Smi::FromInt(JSRegExp::kUninitializedValue));
  return value;
}

Object* JSRegExp::DataAt(int index) {
  DCHECK(TypeTag() != NOT_COMPILED);
  return FixedArray::cast(data())->get(index);
}


void JSRegExp::SetDataAt(int index, Object* value) {
  DCHECK(TypeTag() != NOT_COMPILED);
  DCHECK(index >= kDataIndex);  // Only implementation data can be set this way.
  FixedArray::cast(data())->set(index, value);
}

void JSRegExp::SetLastIndex(int index) {
  static const int offset =
      kSize + JSRegExp::kLastIndexFieldIndex * kPointerSize;
  Smi* value = Smi::FromInt(index);
  WRITE_FIELD(this, offset, value);
}

Object* JSRegExp::LastIndex() {
  static const int offset =
      kSize + JSRegExp::kLastIndexFieldIndex * kPointerSize;
  return READ_FIELD(this, offset);
}

ElementsKind JSObject::GetElementsKind() {
  ElementsKind kind = map()->elements_kind();
#if VERIFY_HEAP && DEBUG
  FixedArrayBase* fixed_array =
      reinterpret_cast<FixedArrayBase*>(READ_FIELD(this, kElementsOffset));

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    Map* map = fixed_array->map();
    if (IsFastSmiOrObjectElementsKind(kind)) {
      DCHECK(map == GetHeap()->fixed_array_map() ||
             map == GetHeap()->fixed_cow_array_map());
    } else if (IsFastDoubleElementsKind(kind)) {
      DCHECK(fixed_array->IsFixedDoubleArray() ||
             fixed_array == GetHeap()->empty_fixed_array());
    } else if (kind == DICTIONARY_ELEMENTS) {
      DCHECK(fixed_array->IsFixedArray());
      DCHECK(fixed_array->IsDictionary());
    } else {
      DCHECK(kind > DICTIONARY_ELEMENTS);
    }
    DCHECK(!IsSloppyArgumentsElementsKind(kind) ||
           (elements()->IsFixedArray() && elements()->length() >= 2));
  }
#endif
  return kind;
}


bool JSObject::HasFastObjectElements() {
  return IsFastObjectElementsKind(GetElementsKind());
}


bool JSObject::HasFastSmiElements() {
  return IsFastSmiElementsKind(GetElementsKind());
}


bool JSObject::HasFastSmiOrObjectElements() {
  return IsFastSmiOrObjectElementsKind(GetElementsKind());
}


bool JSObject::HasFastDoubleElements() {
  return IsFastDoubleElementsKind(GetElementsKind());
}


bool JSObject::HasFastHoleyElements() {
  return IsFastHoleyElementsKind(GetElementsKind());
}


bool JSObject::HasFastElements() {
  return IsFastElementsKind(GetElementsKind());
}


bool JSObject::HasDictionaryElements() {
  return GetElementsKind() == DICTIONARY_ELEMENTS;
}


bool JSObject::HasFastArgumentsElements() {
  return GetElementsKind() == FAST_SLOPPY_ARGUMENTS_ELEMENTS;
}


bool JSObject::HasSlowArgumentsElements() {
  return GetElementsKind() == SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
}


bool JSObject::HasSloppyArgumentsElements() {
  return IsSloppyArgumentsElementsKind(GetElementsKind());
}

bool JSObject::HasStringWrapperElements() {
  return IsStringWrapperElementsKind(GetElementsKind());
}

bool JSObject::HasFastStringWrapperElements() {
  return GetElementsKind() == FAST_STRING_WRAPPER_ELEMENTS;
}

bool JSObject::HasSlowStringWrapperElements() {
  return GetElementsKind() == SLOW_STRING_WRAPPER_ELEMENTS;
}

bool JSObject::HasFixedTypedArrayElements() {
  DCHECK_NOT_NULL(elements());
  return map()->has_fixed_typed_array_elements();
}

#define FIXED_TYPED_ELEMENTS_CHECK(Type, type, TYPE, ctype, size)      \
  bool JSObject::HasFixed##Type##Elements() {                          \
    HeapObject* array = elements();                                    \
    DCHECK(array != NULL);                                             \
    if (!array->IsHeapObject()) return false;                          \
    return array->map()->instance_type() == FIXED_##TYPE##_ARRAY_TYPE; \
  }

TYPED_ARRAYS(FIXED_TYPED_ELEMENTS_CHECK)

#undef FIXED_TYPED_ELEMENTS_CHECK


bool JSObject::HasNamedInterceptor() {
  return map()->has_named_interceptor();
}


bool JSObject::HasIndexedInterceptor() {
  return map()->has_indexed_interceptor();
}


GlobalDictionary* JSObject::global_dictionary() {
  DCHECK(!HasFastProperties());
  DCHECK(IsJSGlobalObject());
  return GlobalDictionary::cast(properties());
}


SeededNumberDictionary* JSObject::element_dictionary() {
  DCHECK(HasDictionaryElements() || HasSlowStringWrapperElements());
  return SeededNumberDictionary::cast(elements());
}


bool Name::IsHashFieldComputed(uint32_t field) {
  return (field & kHashNotComputedMask) == 0;
}


bool Name::HasHashCode() {
  return IsHashFieldComputed(hash_field());
}


uint32_t Name::Hash() {
  // Fast case: has hash code already been computed?
  uint32_t field = hash_field();
  if (IsHashFieldComputed(field)) return field >> kHashShift;
  // Slow case: compute hash code and set it. Has to be a string.
  return String::cast(this)->ComputeAndSetHash();
}


bool Name::IsPrivate() {
  return this->IsSymbol() && Symbol::cast(this)->is_private();
}

bool Name::AsArrayIndex(uint32_t* index) {
  return IsString() && String::cast(this)->AsArrayIndex(index);
}


bool String::AsArrayIndex(uint32_t* index) {
  uint32_t field = hash_field();
  if (IsHashFieldComputed(field) && (field & kIsNotArrayIndexMask)) {
    return false;
  }
  return SlowAsArrayIndex(index);
}


void String::SetForwardedInternalizedString(String* canonical) {
  DCHECK(IsInternalizedString());
  DCHECK(HasHashCode());
  if (canonical == this) return;  // No need to forward.
  DCHECK(SlowEquals(canonical));
  DCHECK(canonical->IsInternalizedString());
  DCHECK(canonical->HasHashCode());
  WRITE_FIELD(this, kHashFieldSlot, canonical);
  // Setting the hash field to a tagged value sets the LSB, causing the hash
  // code to be interpreted as uninitialized.  We use this fact to recognize
  // that we have a forwarded string.
  DCHECK(!HasHashCode());
}


String* String::GetForwardedInternalizedString() {
  DCHECK(IsInternalizedString());
  if (HasHashCode()) return this;
  String* canonical = String::cast(READ_FIELD(this, kHashFieldSlot));
  DCHECK(canonical->IsInternalizedString());
  DCHECK(SlowEquals(canonical));
  DCHECK(canonical->HasHashCode());
  return canonical;
}


// static
Maybe<bool> Object::GreaterThan(Handle<Object> x, Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kGreaterThan:
        return Just(true);
      case ComparisonResult::kLessThan:
      case ComparisonResult::kEqual:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}


// static
Maybe<bool> Object::GreaterThanOrEqual(Handle<Object> x, Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kEqual:
      case ComparisonResult::kGreaterThan:
        return Just(true);
      case ComparisonResult::kLessThan:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}


// static
Maybe<bool> Object::LessThan(Handle<Object> x, Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kLessThan:
        return Just(true);
      case ComparisonResult::kEqual:
      case ComparisonResult::kGreaterThan:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}


// static
Maybe<bool> Object::LessThanOrEqual(Handle<Object> x, Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(x, y);
  if (result.IsJust()) {
    switch (result.FromJust()) {
      case ComparisonResult::kEqual:
      case ComparisonResult::kLessThan:
        return Just(true);
      case ComparisonResult::kGreaterThan:
      case ComparisonResult::kUndefined:
        return Just(false);
    }
  }
  return Nothing<bool>();
}

MaybeHandle<Object> Object::GetPropertyOrElement(Handle<Object> object,
                                                 Handle<Name> name) {
  LookupIterator it =
      LookupIterator::PropertyOrElement(name->GetIsolate(), object, name);
  return GetProperty(&it);
}

MaybeHandle<Object> Object::SetPropertyOrElement(Handle<Object> object,
                                                 Handle<Name> name,
                                                 Handle<Object> value,
                                                 LanguageMode language_mode,
                                                 StoreFromKeyed store_mode) {
  LookupIterator it =
      LookupIterator::PropertyOrElement(name->GetIsolate(), object, name);
  MAYBE_RETURN_NULL(SetProperty(&it, value, language_mode, store_mode));
  return value;
}

MaybeHandle<Object> Object::GetPropertyOrElement(Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Handle<JSReceiver> holder) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), receiver, name, holder);
  return GetProperty(&it);
}


void JSReceiver::initialize_properties() {
  DCHECK(!GetHeap()->InNewSpace(GetHeap()->empty_fixed_array()));
  DCHECK(!GetHeap()->InNewSpace(GetHeap()->empty_properties_dictionary()));
  if (map()->is_dictionary_map()) {
    WRITE_FIELD(this, kPropertiesOffset,
                GetHeap()->empty_properties_dictionary());
  } else {
    WRITE_FIELD(this, kPropertiesOffset, GetHeap()->empty_fixed_array());
  }
}


bool JSReceiver::HasFastProperties() {
  DCHECK_EQ(properties()->IsDictionary(), map()->is_dictionary_map());
  return !properties()->IsDictionary();
}


NameDictionary* JSReceiver::property_dictionary() {
  DCHECK(!HasFastProperties());
  DCHECK(!IsJSGlobalObject());
  return NameDictionary::cast(properties());
}

Maybe<bool> JSReceiver::HasProperty(Handle<JSReceiver> object,
                                    Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(object->GetIsolate(),
                                                        object, name, object);
  return HasProperty(&it);
}


Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       Handle<Name> name) {
  if (object->IsJSObject()) {  // Shortcut
    LookupIterator it = LookupIterator::PropertyOrElement(
        object->GetIsolate(), object, name, object, LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       uint32_t index) {
  if (object->IsJSObject()) {  // Shortcut
    LookupIterator it(object->GetIsolate(), object, index, object,
                      LookupIterator::OWN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, index);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}

Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(name->GetIsolate(),
                                                        object, name, object);
  return GetPropertyAttributes(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), object, name, object, LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}

Maybe<bool> JSReceiver::HasElement(Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index, object);
  return HasProperty(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object);
  return GetPropertyAttributes(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetOwnElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return GetPropertyAttributes(&it);
}


bool JSGlobalObject::IsDetached() {
  return JSGlobalProxy::cast(global_proxy())->IsDetachedFrom(this);
}


bool JSGlobalProxy::IsDetachedFrom(JSGlobalObject* global) const {
  const PrototypeIterator iter(this->GetIsolate(),
                               const_cast<JSGlobalProxy*>(this));
  return iter.GetCurrent() != global;
}

inline int JSGlobalProxy::SizeWithEmbedderFields(int embedder_field_count) {
  DCHECK_GE(embedder_field_count, 0);
  return kSize + embedder_field_count * kPointerSize;
}

Smi* JSReceiver::GetOrCreateIdentityHash(Isolate* isolate,
                                         Handle<JSReceiver> object) {
  return object->IsJSProxy() ? JSProxy::GetOrCreateIdentityHash(
                                   isolate, Handle<JSProxy>::cast(object))
                             : JSObject::GetOrCreateIdentityHash(
                                   isolate, Handle<JSObject>::cast(object));
}

Object* JSReceiver::GetIdentityHash(Isolate* isolate,
                                    Handle<JSReceiver> receiver) {
  return receiver->IsJSProxy()
             ? JSProxy::GetIdentityHash(Handle<JSProxy>::cast(receiver))
             : JSObject::GetIdentityHash(isolate,
                                         Handle<JSObject>::cast(receiver));
}


bool AccessorInfo::all_can_read() {
  return BooleanBit::get(flag(), kAllCanReadBit);
}


void AccessorInfo::set_all_can_read(bool value) {
  set_flag(BooleanBit::set(flag(), kAllCanReadBit, value));
}


bool AccessorInfo::all_can_write() {
  return BooleanBit::get(flag(), kAllCanWriteBit);
}


void AccessorInfo::set_all_can_write(bool value) {
  set_flag(BooleanBit::set(flag(), kAllCanWriteBit, value));
}


bool AccessorInfo::is_special_data_property() {
  return BooleanBit::get(flag(), kSpecialDataProperty);
}


void AccessorInfo::set_is_special_data_property(bool value) {
  set_flag(BooleanBit::set(flag(), kSpecialDataProperty, value));
}

bool AccessorInfo::replace_on_access() {
  return BooleanBit::get(flag(), kReplaceOnAccess);
}

void AccessorInfo::set_replace_on_access(bool value) {
  set_flag(BooleanBit::set(flag(), kReplaceOnAccess, value));
}

bool AccessorInfo::is_sloppy() { return BooleanBit::get(flag(), kIsSloppy); }

void AccessorInfo::set_is_sloppy(bool value) {
  set_flag(BooleanBit::set(flag(), kIsSloppy, value));
}

PropertyAttributes AccessorInfo::property_attributes() {
  return AttributesField::decode(static_cast<uint32_t>(flag()));
}


void AccessorInfo::set_property_attributes(PropertyAttributes attributes) {
  set_flag(AttributesField::update(flag(), attributes));
}

bool FunctionTemplateInfo::IsTemplateFor(JSObject* object) {
  return IsTemplateFor(object->map());
}

bool AccessorInfo::IsCompatibleReceiver(Object* receiver) {
  if (!HasExpectedReceiverType()) return true;
  if (!receiver->IsJSObject()) return false;
  return FunctionTemplateInfo::cast(expected_receiver_type())
      ->IsTemplateFor(JSObject::cast(receiver)->map());
}


bool AccessorInfo::HasExpectedReceiverType() {
  return expected_receiver_type()->IsFunctionTemplateInfo();
}


Object* AccessorPair::get(AccessorComponent component) {
  return component == ACCESSOR_GETTER ? getter() : setter();
}


void AccessorPair::set(AccessorComponent component, Object* value) {
  if (component == ACCESSOR_GETTER) {
    set_getter(value);
  } else {
    set_setter(value);
  }
}


void AccessorPair::SetComponents(Object* getter, Object* setter) {
  Isolate* isolate = GetIsolate();
  if (!getter->IsNull(isolate)) set_getter(getter);
  if (!setter->IsNull(isolate)) set_setter(setter);
}


bool AccessorPair::Equals(AccessorPair* pair) {
  return (this == pair) || pair->Equals(getter(), setter());
}


bool AccessorPair::Equals(Object* getter_value, Object* setter_value) {
  return (getter() == getter_value) && (setter() == setter_value);
}


bool AccessorPair::ContainsAccessor() {
  return IsJSAccessor(getter()) || IsJSAccessor(setter());
}


bool AccessorPair::IsJSAccessor(Object* obj) {
  return obj->IsCallable() || obj->IsUndefined(GetIsolate());
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::SetEntry(int entry,
                                               Handle<Object> key,
                                               Handle<Object> value) {
  this->SetEntry(entry, key, value, PropertyDetails(Smi::kZero));
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::SetEntry(int entry,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               PropertyDetails details) {
  Shape::SetEntry(static_cast<Derived*>(this), entry, key, value, details);
}


template <typename Key>
template <typename Dictionary>
void BaseDictionaryShape<Key>::SetEntry(Dictionary* dict, int entry,
                                        Handle<Object> key,
                                        Handle<Object> value,
                                        PropertyDetails details) {
  STATIC_ASSERT(Dictionary::kEntrySize == 2 || Dictionary::kEntrySize == 3);
  DCHECK(!key->IsName() || details.dictionary_index() > 0);
  int index = dict->EntryToIndex(entry);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = dict->GetWriteBarrierMode(no_gc);
  dict->set(index + Dictionary::kEntryKeyIndex, *key, mode);
  dict->set(index + Dictionary::kEntryValueIndex, *value, mode);
  if (Dictionary::kEntrySize == 3) {
    dict->set(index + Dictionary::kEntryDetailsIndex, details.AsSmi());
  }
}


template <typename Dictionary>
void GlobalDictionaryShape::SetEntry(Dictionary* dict, int entry,
                                     Handle<Object> key, Handle<Object> value,
                                     PropertyDetails details) {
  STATIC_ASSERT(Dictionary::kEntrySize == 2);
  DCHECK(!key->IsName() || details.dictionary_index() > 0);
  DCHECK(value->IsPropertyCell());
  int index = dict->EntryToIndex(entry);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = dict->GetWriteBarrierMode(no_gc);
  dict->set(index + Dictionary::kEntryKeyIndex, *key, mode);
  dict->set(index + Dictionary::kEntryValueIndex, *value, mode);
  PropertyCell::cast(*value)->set_property_details(details);
}


bool NumberDictionaryShape::IsMatch(uint32_t key, Object* other) {
  DCHECK(other->IsNumber());
  return key == static_cast<uint32_t>(other->Number());
}


uint32_t UnseededNumberDictionaryShape::Hash(uint32_t key) {
  return ComputeIntegerHash(key, 0);
}


uint32_t UnseededNumberDictionaryShape::HashForObject(uint32_t key,
                                                      Object* other) {
  DCHECK(other->IsNumber());
  return ComputeIntegerHash(static_cast<uint32_t>(other->Number()), 0);
}

Map* UnseededNumberDictionaryShape::GetMap(Isolate* isolate) {
  return isolate->heap()->unseeded_number_dictionary_map();
}

uint32_t SeededNumberDictionaryShape::SeededHash(uint32_t key, uint32_t seed) {
  return ComputeIntegerHash(key, seed);
}


uint32_t SeededNumberDictionaryShape::SeededHashForObject(uint32_t key,
                                                          uint32_t seed,
                                                          Object* other) {
  DCHECK(other->IsNumber());
  return ComputeIntegerHash(static_cast<uint32_t>(other->Number()), seed);
}


Handle<Object> NumberDictionaryShape::AsHandle(Isolate* isolate, uint32_t key) {
  return isolate->factory()->NewNumberFromUint(key);
}


bool NameDictionaryShape::IsMatch(Handle<Name> key, Object* other) {
  DCHECK(Name::cast(other)->IsUniqueName());
  DCHECK(key->IsUniqueName());
  return *key == other;
}


uint32_t NameDictionaryShape::Hash(Handle<Name> key) {
  return key->Hash();
}


uint32_t NameDictionaryShape::HashForObject(Handle<Name> key, Object* other) {
  return Name::cast(other)->Hash();
}


Handle<Object> NameDictionaryShape::AsHandle(Isolate* isolate,
                                             Handle<Name> key) {
  DCHECK(key->IsUniqueName());
  return key;
}


template <typename Dictionary>
PropertyDetails GlobalDictionaryShape::DetailsAt(Dictionary* dict, int entry) {
  DCHECK(entry >= 0);  // Not found is -1, which is not caught by get().
  Object* raw_value = dict->ValueAt(entry);
  DCHECK(raw_value->IsPropertyCell());
  PropertyCell* cell = PropertyCell::cast(raw_value);
  return cell->property_details();
}


template <typename Dictionary>
void GlobalDictionaryShape::DetailsAtPut(Dictionary* dict, int entry,
                                         PropertyDetails value) {
  DCHECK(entry >= 0);  // Not found is -1, which is not caught by get().
  Object* raw_value = dict->ValueAt(entry);
  DCHECK(raw_value->IsPropertyCell());
  PropertyCell* cell = PropertyCell::cast(raw_value);
  cell->set_property_details(value);
}


template <typename Dictionary>
bool GlobalDictionaryShape::IsDeleted(Dictionary* dict, int entry) {
  DCHECK(dict->ValueAt(entry)->IsPropertyCell());
  Isolate* isolate = dict->GetIsolate();
  return PropertyCell::cast(dict->ValueAt(entry))->value()->IsTheHole(isolate);
}


bool ObjectHashTableShape::IsMatch(Handle<Object> key, Object* other) {
  return key->SameValue(other);
}


uint32_t ObjectHashTableShape::Hash(Handle<Object> key) {
  return Smi::cast(key->GetHash())->value();
}


uint32_t ObjectHashTableShape::HashForObject(Handle<Object> key,
                                             Object* other) {
  return Smi::cast(other->GetHash())->value();
}


Handle<Object> ObjectHashTableShape::AsHandle(Isolate* isolate,
                                              Handle<Object> key) {
  return key;
}


Handle<ObjectHashTable> ObjectHashTable::Shrink(
    Handle<ObjectHashTable> table, Handle<Object> key) {
  return DerivedHashTable::Shrink(table, key);
}


Object* OrderedHashMap::ValueAt(int entry) {
  return get(EntryToIndex(entry) + kValueOffset);
}


template <int entrysize>
bool WeakHashTableShape<entrysize>::IsMatch(Handle<Object> key, Object* other) {
  if (other->IsWeakCell()) other = WeakCell::cast(other)->value();
  return key->IsWeakCell() ? WeakCell::cast(*key)->value() == other
                           : *key == other;
}


template <int entrysize>
uint32_t WeakHashTableShape<entrysize>::Hash(Handle<Object> key) {
  intptr_t hash =
      key->IsWeakCell()
          ? reinterpret_cast<intptr_t>(WeakCell::cast(*key)->value())
          : reinterpret_cast<intptr_t>(*key);
  return (uint32_t)(hash & 0xFFFFFFFF);
}


template <int entrysize>
uint32_t WeakHashTableShape<entrysize>::HashForObject(Handle<Object> key,
                                                      Object* other) {
  if (other->IsWeakCell()) other = WeakCell::cast(other)->value();
  intptr_t hash = reinterpret_cast<intptr_t>(other);
  return (uint32_t)(hash & 0xFFFFFFFF);
}


template <int entrysize>
Handle<Object> WeakHashTableShape<entrysize>::AsHandle(Isolate* isolate,
                                                       Handle<Object> key) {
  return key;
}


ACCESSORS(ModuleInfoEntry, export_name, Object, kExportNameOffset)
ACCESSORS(ModuleInfoEntry, local_name, Object, kLocalNameOffset)
ACCESSORS(ModuleInfoEntry, import_name, Object, kImportNameOffset)
SMI_ACCESSORS(ModuleInfoEntry, module_request, kModuleRequestOffset)
SMI_ACCESSORS(ModuleInfoEntry, cell_index, kCellIndexOffset)
SMI_ACCESSORS(ModuleInfoEntry, beg_pos, kBegPosOffset)
SMI_ACCESSORS(ModuleInfoEntry, end_pos, kEndPosOffset)

void Map::ClearCodeCache(Heap* heap) {
  // No write barrier is needed since empty_fixed_array is not in new space.
  // Please note this function is used during marking:
  //  - MarkCompactCollector::MarkUnmarkedObject
  //  - IncrementalMarking::Step
  WRITE_FIELD(this, kCodeCacheOffset, heap->empty_fixed_array());
}


int Map::SlackForArraySize(int old_size, int size_limit) {
  const int max_slack = size_limit - old_size;
  CHECK_LE(0, max_slack);
  if (old_size < 4) {
    DCHECK_LE(1, max_slack);
    return 1;
  }
  return Min(max_slack, old_size / 4);
}


void JSArray::set_length(Smi* length) {
  // Don't need a write barrier for a Smi.
  set_length(static_cast<Object*>(length), SKIP_WRITE_BARRIER);
}


bool JSArray::SetLengthWouldNormalize(Heap* heap, uint32_t new_length) {
  // This constant is somewhat arbitrary. Any large enough value would work.
  const uint32_t kMaxFastArrayLength = 32 * 1024 * 1024;
  // If the new array won't fit in a some non-trivial fraction of the max old
  // space size, then force it to go dictionary mode.
  uint32_t heap_based_upper_bound =
      static_cast<uint32_t>((heap->MaxOldGenerationSize() / kDoubleSize) / 4);
  return new_length >= Min(kMaxFastArrayLength, heap_based_upper_bound);
}


bool JSArray::AllowsSetLength() {
  bool result = elements()->IsFixedArray() || elements()->IsFixedDoubleArray();
  DCHECK(result == !HasFixedTypedArrayElements());
  return result;
}


void JSArray::SetContent(Handle<JSArray> array,
                         Handle<FixedArrayBase> storage) {
  EnsureCanContainElements(array, storage, storage->length(),
                           ALLOW_COPIED_DOUBLE_ELEMENTS);

  DCHECK((storage->map() == array->GetHeap()->fixed_double_array_map() &&
          IsFastDoubleElementsKind(array->GetElementsKind())) ||
         ((storage->map() != array->GetHeap()->fixed_double_array_map()) &&
          (IsFastObjectElementsKind(array->GetElementsKind()) ||
           (IsFastSmiElementsKind(array->GetElementsKind()) &&
            Handle<FixedArray>::cast(storage)->ContainsOnlySmisOrHoles()))));
  array->set_elements(*storage);
  array->set_length(Smi::FromInt(storage->length()));
}


bool JSArray::HasArrayPrototype(Isolate* isolate) {
  return map()->prototype() == *isolate->initial_array_prototype();
}


int TypeFeedbackInfo::ic_total_count() {
  int current = Smi::cast(READ_FIELD(this, kStorage1Offset))->value();
  return ICTotalCountField::decode(current);
}


void TypeFeedbackInfo::set_ic_total_count(int count) {
  int value = Smi::cast(READ_FIELD(this, kStorage1Offset))->value();
  value = ICTotalCountField::update(value,
                                    ICTotalCountField::decode(count));
  WRITE_FIELD(this, kStorage1Offset, Smi::FromInt(value));
}


int TypeFeedbackInfo::ic_with_type_info_count() {
  int current = Smi::cast(READ_FIELD(this, kStorage2Offset))->value();
  return ICsWithTypeInfoCountField::decode(current);
}


void TypeFeedbackInfo::change_ic_with_type_info_count(int delta) {
  if (delta == 0) return;
  int value = Smi::cast(READ_FIELD(this, kStorage2Offset))->value();
  int new_count = ICsWithTypeInfoCountField::decode(value) + delta;
  // We can get negative count here when the type-feedback info is
  // shared between two code objects. The can only happen when
  // the debugger made a shallow copy of code object (see Heap::CopyCode).
  // Since we do not optimize when the debugger is active, we can skip
  // this counter update.
  if (new_count >= 0) {
    new_count &= ICsWithTypeInfoCountField::kMask;
    value = ICsWithTypeInfoCountField::update(value, new_count);
    WRITE_FIELD(this, kStorage2Offset, Smi::FromInt(value));
  }
}


int TypeFeedbackInfo::ic_generic_count() {
  return Smi::cast(READ_FIELD(this, kStorage3Offset))->value();
}


void TypeFeedbackInfo::change_ic_generic_count(int delta) {
  if (delta == 0) return;
  int new_count = ic_generic_count() + delta;
  if (new_count >= 0) {
    new_count &= ~Smi::kMinValue;
    WRITE_FIELD(this, kStorage3Offset, Smi::FromInt(new_count));
  }
}


void TypeFeedbackInfo::initialize_storage() {
  WRITE_FIELD(this, kStorage1Offset, Smi::kZero);
  WRITE_FIELD(this, kStorage2Offset, Smi::kZero);
  WRITE_FIELD(this, kStorage3Offset, Smi::kZero);
}


void TypeFeedbackInfo::change_own_type_change_checksum() {
  int value = Smi::cast(READ_FIELD(this, kStorage1Offset))->value();
  int checksum = OwnTypeChangeChecksum::decode(value);
  checksum = (checksum + 1) % (1 << kTypeChangeChecksumBits);
  value = OwnTypeChangeChecksum::update(value, checksum);
  // Ensure packed bit field is in Smi range.
  if (value > Smi::kMaxValue) value |= Smi::kMinValue;
  if (value < Smi::kMinValue) value &= ~Smi::kMinValue;
  WRITE_FIELD(this, kStorage1Offset, Smi::FromInt(value));
}


void TypeFeedbackInfo::set_inlined_type_change_checksum(int checksum) {
  int value = Smi::cast(READ_FIELD(this, kStorage2Offset))->value();
  int mask = (1 << kTypeChangeChecksumBits) - 1;
  value = InlinedTypeChangeChecksum::update(value, checksum & mask);
  // Ensure packed bit field is in Smi range.
  if (value > Smi::kMaxValue) value |= Smi::kMinValue;
  if (value < Smi::kMinValue) value &= ~Smi::kMinValue;
  WRITE_FIELD(this, kStorage2Offset, Smi::FromInt(value));
}


int TypeFeedbackInfo::own_type_change_checksum() {
  int value = Smi::cast(READ_FIELD(this, kStorage1Offset))->value();
  return OwnTypeChangeChecksum::decode(value);
}


bool TypeFeedbackInfo::matches_inlined_type_change_checksum(int checksum) {
  int value = Smi::cast(READ_FIELD(this, kStorage2Offset))->value();
  int mask = (1 << kTypeChangeChecksumBits) - 1;
  return InlinedTypeChangeChecksum::decode(value) == (checksum & mask);
}


SMI_ACCESSORS(AliasedArgumentsEntry, aliased_context_slot, kAliasedContextSlot)


Relocatable::Relocatable(Isolate* isolate) {
  isolate_ = isolate;
  prev_ = isolate->relocatable_top();
  isolate->set_relocatable_top(this);
}


Relocatable::~Relocatable() {
  DCHECK_EQ(isolate_->relocatable_top(), this);
  isolate_->set_relocatable_top(prev_);
}


template<class Derived, class TableType>
Object* OrderedHashTableIterator<Derived, TableType>::CurrentKey() {
  TableType* table(TableType::cast(this->table()));
  int index = Smi::cast(this->index())->value();
  Object* key = table->KeyAt(index);
  DCHECK(!key->IsTheHole(table->GetIsolate()));
  return key;
}


void JSSetIterator::PopulateValueArray(FixedArray* array) {
  array->set(0, CurrentKey());
}


void JSMapIterator::PopulateValueArray(FixedArray* array) {
  array->set(0, CurrentKey());
  array->set(1, CurrentValue());
}


Object* JSMapIterator::CurrentValue() {
  OrderedHashMap* table(OrderedHashMap::cast(this->table()));
  int index = Smi::cast(this->index())->value();
  Object* value = table->ValueAt(index);
  DCHECK(!value->IsTheHole(table->GetIsolate()));
  return value;
}


String::SubStringRange::SubStringRange(String* string, int first, int length)
    : string_(string),
      first_(first),
      length_(length == -1 ? string->length() : length) {}


class String::SubStringRange::iterator final {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef int difference_type;
  typedef uc16 value_type;
  typedef uc16* pointer;
  typedef uc16& reference;

  iterator(const iterator& other)
      : content_(other.content_), offset_(other.offset_) {}

  uc16 operator*() { return content_.Get(offset_); }
  bool operator==(const iterator& other) const {
    return content_.UsesSameString(other.content_) && offset_ == other.offset_;
  }
  bool operator!=(const iterator& other) const {
    return !content_.UsesSameString(other.content_) || offset_ != other.offset_;
  }
  iterator& operator++() {
    ++offset_;
    return *this;
  }
  iterator operator++(int);

 private:
  friend class String;
  iterator(String* from, int offset)
      : content_(from->GetFlatContent()), offset_(offset) {}
  String::FlatContent content_;
  int offset_;
};


String::SubStringRange::iterator String::SubStringRange::begin() {
  return String::SubStringRange::iterator(string_, first_);
}


String::SubStringRange::iterator String::SubStringRange::end() {
  return String::SubStringRange::iterator(string_, first_ + length_);
}


// Predictably converts HeapObject* or Address to uint32 by calculating
// offset of the address in respective MemoryChunk.
static inline uint32_t ObjectAddressForHashing(void* object) {
  uint32_t value = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(object));
  return value & MemoryChunk::kAlignmentMask;
}

static inline Handle<Object> MakeEntryPair(Isolate* isolate, uint32_t index,
                                           Handle<Object> value) {
  Handle<Object> key = isolate->factory()->Uint32ToString(index);
  Handle<FixedArray> entry_storage =
      isolate->factory()->NewUninitializedFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    FAST_ELEMENTS, 2);
}

static inline Handle<Object> MakeEntryPair(Isolate* isolate, Handle<Name> key,
                                           Handle<Object> value) {
  Handle<FixedArray> entry_storage =
      isolate->factory()->NewUninitializedFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    FAST_ELEMENTS, 2);
}

ACCESSORS(JSIteratorResult, value, Object, kValueOffset)
ACCESSORS(JSIteratorResult, done, Object, kDoneOffset)

ACCESSORS(JSArrayIterator, object, Object, kIteratedObjectOffset)
ACCESSORS(JSArrayIterator, index, Object, kNextIndexOffset)
ACCESSORS(JSArrayIterator, object_map, Object, kIteratedObjectMapOffset)

ACCESSORS(JSAsyncFromSyncIterator, sync_iterator, JSReceiver,
          kSyncIteratorOffset)

ACCESSORS(JSStringIterator, string, String, kStringOffset)
SMI_ACCESSORS(JSStringIterator, index, kNextIndexOffset)

#undef INT_ACCESSORS
#undef ACCESSORS
#undef ACCESSORS_CHECKED
#undef ACCESSORS_CHECKED2
#undef SMI_ACCESSORS
#undef SYNCHRONIZED_SMI_ACCESSORS
#undef NOBARRIER_SMI_ACCESSORS
#undef BOOL_GETTER
#undef BOOL_ACCESSORS
#undef FIELD_ADDR
#undef FIELD_ADDR_CONST
#undef READ_FIELD
#undef NOBARRIER_READ_FIELD
#undef WRITE_FIELD
#undef NOBARRIER_WRITE_FIELD
#undef WRITE_BARRIER
#undef CONDITIONAL_WRITE_BARRIER
#undef READ_DOUBLE_FIELD
#undef WRITE_DOUBLE_FIELD
#undef READ_INT_FIELD
#undef WRITE_INT_FIELD
#undef READ_INTPTR_FIELD
#undef WRITE_INTPTR_FIELD
#undef READ_UINT8_FIELD
#undef WRITE_UINT8_FIELD
#undef READ_INT8_FIELD
#undef WRITE_INT8_FIELD
#undef READ_UINT16_FIELD
#undef WRITE_UINT16_FIELD
#undef READ_INT16_FIELD
#undef WRITE_INT16_FIELD
#undef READ_UINT32_FIELD
#undef WRITE_UINT32_FIELD
#undef READ_INT32_FIELD
#undef WRITE_INT32_FIELD
#undef READ_FLOAT_FIELD
#undef WRITE_FLOAT_FIELD
#undef READ_UINT64_FIELD
#undef WRITE_UINT64_FIELD
#undef READ_INT64_FIELD
#undef WRITE_INT64_FIELD
#undef READ_BYTE_FIELD
#undef WRITE_BYTE_FIELD
#undef NOBARRIER_READ_BYTE_FIELD
#undef NOBARRIER_WRITE_BYTE_FIELD

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INL_H_
