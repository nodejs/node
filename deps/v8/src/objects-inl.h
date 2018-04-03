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
#include "src/base/tsan.h"
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
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects.h"
#include "src/objects/module-inl.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/scope-info.h"
#include "src/objects/template-objects.h"
#include "src/property.h"
#include "src/prototype.h"
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
  DCHECK_EQ(location(), kField);
  if (!FLAG_unbox_double_fields) return 1;
  if (kDoubleSize == kPointerSize) return 1;
  return representation().IsDouble() ? kDoubleSize / kPointerSize : 1;
}

TYPE_CHECKER(BigInt, BIGINT_TYPE)
TYPE_CHECKER(BreakPoint, TUPLE2_TYPE)
TYPE_CHECKER(BreakPointInfo, TUPLE2_TYPE)
TYPE_CHECKER(CallHandlerInfo, TUPLE3_TYPE)
TYPE_CHECKER(Cell, CELL_TYPE)
TYPE_CHECKER(ConstantElementsPair, TUPLE2_TYPE)
TYPE_CHECKER(CoverageInfo, FIXED_ARRAY_TYPE)
TYPE_CHECKER(DescriptorArray, DESCRIPTOR_ARRAY_TYPE)
TYPE_CHECKER(FeedbackVector, FEEDBACK_VECTOR_TYPE)
TYPE_CHECKER(Foreign, FOREIGN_TYPE)
TYPE_CHECKER(FreeSpace, FREE_SPACE_TYPE)
TYPE_CHECKER(HashTable, HASH_TABLE_TYPE)
TYPE_CHECKER(HeapNumber, HEAP_NUMBER_TYPE)
TYPE_CHECKER(JSAsyncFromSyncIterator, JS_ASYNC_FROM_SYNC_ITERATOR_TYPE)
TYPE_CHECKER(JSAsyncGeneratorObject, JS_ASYNC_GENERATOR_OBJECT_TYPE)
TYPE_CHECKER(JSBoundFunction, JS_BOUND_FUNCTION_TYPE)
TYPE_CHECKER(JSContextExtensionObject, JS_CONTEXT_EXTENSION_OBJECT_TYPE)
TYPE_CHECKER(JSDataView, JS_DATA_VIEW_TYPE)
TYPE_CHECKER(JSDate, JS_DATE_TYPE)
TYPE_CHECKER(JSError, JS_ERROR_TYPE)
TYPE_CHECKER(JSFunction, JS_FUNCTION_TYPE)
TYPE_CHECKER(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)
TYPE_CHECKER(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)
TYPE_CHECKER(JSPromise, JS_PROMISE_TYPE)
TYPE_CHECKER(JSStringIterator, JS_STRING_ITERATOR_TYPE)
TYPE_CHECKER(JSValue, JS_VALUE_TYPE)
TYPE_CHECKER(MutableHeapNumber, MUTABLE_HEAP_NUMBER_TYPE)
TYPE_CHECKER(Oddball, ODDBALL_TYPE)
TYPE_CHECKER(PreParsedScopeData, TUPLE2_TYPE)
TYPE_CHECKER(PropertyArray, PROPERTY_ARRAY_TYPE)
TYPE_CHECKER(PropertyCell, PROPERTY_CELL_TYPE)
TYPE_CHECKER(PropertyDescriptorObject, FIXED_ARRAY_TYPE)
TYPE_CHECKER(SmallOrderedHashMap, SMALL_ORDERED_HASH_MAP_TYPE)
TYPE_CHECKER(SmallOrderedHashSet, SMALL_ORDERED_HASH_SET_TYPE)
TYPE_CHECKER(SourcePositionTableWithFrameCache, TUPLE2_TYPE)
TYPE_CHECKER(TemplateMap, HASH_TABLE_TYPE)
TYPE_CHECKER(TemplateObjectDescription, TUPLE3_TYPE)
TYPE_CHECKER(TransitionArray, TRANSITION_ARRAY_TYPE)
TYPE_CHECKER(WasmInstanceObject, WASM_INSTANCE_TYPE)
TYPE_CHECKER(WasmMemoryObject, WASM_MEMORY_TYPE)
TYPE_CHECKER(WasmModuleObject, WASM_MODULE_TYPE)
TYPE_CHECKER(WasmTableObject, WASM_TABLE_TYPE)
TYPE_CHECKER(WeakCell, WEAK_CELL_TYPE)

#define TYPED_ARRAY_TYPE_CHECKER(Type, type, TYPE, ctype, size) \
  TYPE_CHECKER(Fixed##Type##Array, FIXED_##TYPE##_ARRAY_TYPE)
TYPED_ARRAYS(TYPED_ARRAY_TYPE_CHECKER)
#undef TYPED_ARRAY_TYPE_CHECKER


bool HeapObject::IsFixedArrayBase() const {
  return IsFixedArray() || IsFixedDoubleArray() || IsFixedTypedArrayBase();
}

bool HeapObject::IsFixedArray() const {
  InstanceType instance_type = map()->instance_type();
  return instance_type >= FIRST_FIXED_ARRAY_TYPE &&
         instance_type <= LAST_FIXED_ARRAY_TYPE;
}

bool HeapObject::IsSloppyArgumentsElements() const {
  return IsFixedArrayExact();
}

bool HeapObject::IsJSSloppyArgumentsObject() const {
  return IsJSArgumentsObject();
}

bool HeapObject::IsJSGeneratorObject() const {
  return map()->instance_type() == JS_GENERATOR_OBJECT_TYPE ||
         IsJSAsyncGeneratorObject();
}

bool HeapObject::IsBoilerplateDescription() const {
  return IsFixedArrayExact();
}

bool HeapObject::IsClassBoilerplate() const { return IsFixedArrayExact(); }

bool HeapObject::IsExternal() const {
  return map()->FindRootMap() == GetHeap()->external_map();
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

bool Object::IsNumeric() const { return IsNumber() || IsBigInt(); }

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

bool HeapObject::IsJSMapIterator() const {
  InstanceType instance_type = map()->instance_type();
  STATIC_ASSERT(JS_MAP_KEY_ITERATOR_TYPE + 1 == JS_MAP_KEY_VALUE_ITERATOR_TYPE);
  STATIC_ASSERT(JS_MAP_KEY_VALUE_ITERATOR_TYPE + 1 ==
                JS_MAP_VALUE_ITERATOR_TYPE);
  return (instance_type >= JS_MAP_KEY_ITERATOR_TYPE &&
          instance_type <= JS_MAP_VALUE_ITERATOR_TYPE);
}

bool HeapObject::IsJSSetIterator() const {
  InstanceType instance_type = map()->instance_type();
  return (instance_type == JS_SET_VALUE_ITERATOR_TYPE ||
          instance_type == JS_SET_KEY_VALUE_ITERATOR_TYPE);
}

bool HeapObject::IsJSArrayIterator() const {
  InstanceType instance_type = map()->instance_type();
  return (instance_type >= FIRST_ARRAY_ITERATOR_TYPE &&
          instance_type <= LAST_ARRAY_ITERATOR_TYPE);
}

bool HeapObject::IsJSWeakCollection() const {
  return IsJSWeakMap() || IsJSWeakSet();
}

bool HeapObject::IsJSCollection() const { return IsJSMap() || IsJSSet(); }

bool HeapObject::IsPromiseCapability() const { return IsTuple3(); }

bool HeapObject::IsEnumCache() const { return IsTuple2(); }

bool HeapObject::IsFrameArray() const { return IsFixedArrayExact(); }

bool HeapObject::IsArrayList() const {
  return map() == GetHeap()->array_list_map() ||
         this == GetHeap()->empty_fixed_array();
}

bool HeapObject::IsRegExpMatchInfo() const { return IsFixedArrayExact(); }

bool Object::IsLayoutDescriptor() const { return IsSmi() || IsByteArray(); }

bool HeapObject::IsFeedbackMetadata() const { return IsFixedArrayExact(); }

bool HeapObject::IsDeoptimizationData() const {
  // Must be a fixed array.
  if (!IsFixedArrayExact()) return false;

  // There's no sure way to detect the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can
  // check that the length is zero or else the fixed size plus a multiple of
  // the entry size.
  int length = FixedArray::cast(this)->length();
  if (length == 0) return true;

  length -= DeoptimizationData::kFirstDeoptEntryIndex;
  return length >= 0 && length % DeoptimizationData::kDeoptEntrySize == 0;
}

bool HeapObject::IsHandlerTable() const {
  if (!IsFixedArrayExact()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a handler table array.
  return true;
}

bool HeapObject::IsTemplateList() const {
  if (!IsFixedArrayExact()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a template list.
  if (FixedArray::cast(this)->length() < 1) return false;
  return true;
}

bool HeapObject::IsDependentCode() const {
  if (!IsFixedArrayExact()) return false;
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

bool HeapObject::IsWeakHashTable() const {
  return map() == GetHeap()->weak_hash_table_map();
}

bool HeapObject::IsDictionary() const {
  return IsHashTable() && this != GetHeap()->string_table();
}

bool HeapObject::IsGlobalDictionary() const {
  return map() == GetHeap()->global_dictionary_map();
}

bool HeapObject::IsNameDictionary() const {
  return map() == GetHeap()->name_dictionary_map();
}

bool HeapObject::IsNumberDictionary() const {
  return map() == GetHeap()->number_dictionary_map();
}

bool HeapObject::IsStringTable() const {
  return map() == GetHeap()->string_table_map();
}

bool HeapObject::IsStringSet() const { return IsHashTable(); }

bool HeapObject::IsObjectHashSet() const { return IsHashTable(); }

bool HeapObject::IsNormalizedMapCache() const {
  return NormalizedMapCache::IsNormalizedMapCache(this);
}

bool HeapObject::IsCompilationCacheTable() const { return IsHashTable(); }

bool HeapObject::IsMapCache() const { return IsHashTable(); }

bool HeapObject::IsObjectHashTable() const { return IsHashTable(); }

bool HeapObject::IsOrderedHashSet() const {
  return map() == GetHeap()->ordered_hash_set_map();
}

bool HeapObject::IsOrderedHashMap() const {
  return map() == GetHeap()->ordered_hash_map_map();
}

bool Object::IsSmallOrderedHashTable() const {
  return IsSmallOrderedHashSet() || IsSmallOrderedHashMap();
}

bool Object::IsPrimitive() const {
  return IsSmi() || HeapObject::cast(this)->map()->IsPrimitiveMap();
}

// static
Maybe<bool> Object::IsArray(Handle<Object> object) {
  if (object->IsSmi()) return Just(false);
  Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
  if (heap_object->IsJSArray()) return Just(true);
  if (!heap_object->IsJSProxy()) return Just(false);
  return JSProxy::IsArray(Handle<JSProxy>::cast(object));
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

CAST_ACCESSOR(AccessCheckInfo)
CAST_ACCESSOR(AccessorInfo)
CAST_ACCESSOR(AccessorPair)
CAST_ACCESSOR(AllocationMemento)
CAST_ACCESSOR(AllocationSite)
CAST_ACCESSOR(AsyncGeneratorRequest)
CAST_ACCESSOR(BigInt)
CAST_ACCESSOR(BoilerplateDescription)
CAST_ACCESSOR(CallHandlerInfo)
CAST_ACCESSOR(Cell)
CAST_ACCESSOR(ConstantElementsPair)
CAST_ACCESSOR(ContextExtension)
CAST_ACCESSOR(DescriptorArray)
CAST_ACCESSOR(EnumCache)
CAST_ACCESSOR(Foreign)
CAST_ACCESSOR(FunctionTemplateInfo)
CAST_ACCESSOR(GlobalDictionary)
CAST_ACCESSOR(HeapObject)
CAST_ACCESSOR(InterceptorInfo)
CAST_ACCESSOR(JSAsyncFromSyncIterator)
CAST_ACCESSOR(JSAsyncGeneratorObject)
CAST_ACCESSOR(JSBoundFunction)
CAST_ACCESSOR(JSDataView)
CAST_ACCESSOR(JSDate)
CAST_ACCESSOR(JSFunction)
CAST_ACCESSOR(JSGeneratorObject)
CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSGlobalProxy)
CAST_ACCESSOR(JSMessageObject)
CAST_ACCESSOR(JSObject)
CAST_ACCESSOR(JSPromise)
CAST_ACCESSOR(JSProxy)
CAST_ACCESSOR(JSReceiver)
CAST_ACCESSOR(JSStringIterator)
CAST_ACCESSOR(JSValue)
CAST_ACCESSOR(LayoutDescriptor)
CAST_ACCESSOR(NameDictionary)
CAST_ACCESSOR(NormalizedMapCache)
CAST_ACCESSOR(Object)
CAST_ACCESSOR(ObjectHashSet)
CAST_ACCESSOR(ObjectHashTable)
CAST_ACCESSOR(ObjectTemplateInfo)
CAST_ACCESSOR(Oddball)
CAST_ACCESSOR(OrderedHashMap)
CAST_ACCESSOR(OrderedHashSet)
CAST_ACCESSOR(PromiseCapability)
CAST_ACCESSOR(PromiseReactionJobInfo)
CAST_ACCESSOR(PromiseResolveThenableJobInfo)
CAST_ACCESSOR(PropertyArray)
CAST_ACCESSOR(PropertyCell)
CAST_ACCESSOR(PrototypeInfo)
CAST_ACCESSOR(RegExpMatchInfo)
CAST_ACCESSOR(ScopeInfo)
CAST_ACCESSOR(NumberDictionary)
CAST_ACCESSOR(SmallOrderedHashMap)
CAST_ACCESSOR(SmallOrderedHashSet)
CAST_ACCESSOR(Smi)
CAST_ACCESSOR(SourcePositionTableWithFrameCache)
CAST_ACCESSOR(StackFrameInfo)
CAST_ACCESSOR(StringSet)
CAST_ACCESSOR(StringTable)
CAST_ACCESSOR(Struct)
CAST_ACCESSOR(TemplateInfo)
CAST_ACCESSOR(TemplateMap)
CAST_ACCESSOR(TemplateObjectDescription)
CAST_ACCESSOR(Tuple2)
CAST_ACCESSOR(Tuple3)
CAST_ACCESSOR(WeakCell)
CAST_ACCESSOR(WeakHashTable)

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
  DCHECK(!IsPropertyCell());
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
  if (IsSmi()) return PACKED_SMI_ELEMENTS;
  if (IsNumber()) return PACKED_DOUBLE_ELEMENTS;
  return PACKED_ELEMENTS;
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

bool Object::ToUint32(uint32_t* value) const {
  if (IsSmi()) {
    int num = Smi::ToInt(this);
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
  if (input->IsNumber()) return input;  // Shortcut.
  return ConvertToNumberOrNumeric(HeapObject::cast(*input)->GetIsolate(), input,
                                  Conversion::kToNumber);
}

// static
MaybeHandle<Object> Object::ToNumeric(Handle<Object> input) {
  if (input->IsNumber() || input->IsBigInt()) return input;  // Shortcut.
  return ConvertToNumberOrNumeric(HeapObject::cast(*input)->GetIsolate(), input,
                                  Conversion::kToNumeric);
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
    int value = std::max(Smi::ToInt(*input), 0);
    return handle(Smi::FromInt(value), isolate);
  }
  return ConvertToLength(isolate, input);
}

// static
MaybeHandle<Object> Object::ToIndex(Isolate* isolate, Handle<Object> input,
                                    MessageTemplate::Template error_index) {
  if (input->IsSmi() && Smi::ToInt(*input) >= 0) return input;
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
  HeapObject* empty_fixed_array = isolate->heap()->empty_fixed_array();
  HeapObject* empty_slow_element_dictionary =
      isolate->heap()->empty_slow_element_dictionary();
  while (prototype != null) {
    Map* map = prototype->map();
    if (map->instance_type() <= LAST_CUSTOM_ELEMENTS_RECEIVER) return false;
    HeapObject* elements = JSObject::cast(prototype)->elements();
    if (elements != empty_fixed_array &&
        elements != empty_slow_element_dictionary) {
      return false;
    }
    prototype = HeapObject::cast(map->prototype());
  }
  return true;
}

Object** HeapObject::RawField(HeapObject* obj, int byte_offset) {
  return reinterpret_cast<Object**>(FIELD_ADDR(obj, byte_offset));
}

int Smi::ToInt(const Object* object) { return Smi::cast(object)->value(); }

MapWord MapWord::FromMap(const Map* map) {
  return MapWord(reinterpret_cast<uintptr_t>(map));
}

Map* MapWord::ToMap() const { return reinterpret_cast<Map*>(value_); }

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
  SLOW_DCHECK(heap != nullptr);
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
    // TODO(1600) We are passing nullptr as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, nullptr, value);
  }
}

Map* HeapObject::synchronized_map() const {
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
    // TODO(1600) We are passing nullptr as a slot because maps can never be on
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
    DCHECK_NOT_NULL(value);
    // TODO(1600) We are passing nullptr as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, nullptr, value);
  }
}

HeapObject** HeapObject::map_slot() {
  return reinterpret_cast<HeapObject**>(FIELD_ADDR(this, kMapOffset));
}

MapWord HeapObject::map_word() const {
  return MapWord(
      reinterpret_cast<uintptr_t>(RELAXED_READ_FIELD(this, kMapOffset)));
}


void HeapObject::set_map_word(MapWord map_word) {
  RELAXED_WRITE_FIELD(this, kMapOffset,
                      reinterpret_cast<Object*>(map_word.value_));
}


MapWord HeapObject::synchronized_map_word() const {
  return MapWord(
      reinterpret_cast<uintptr_t>(ACQUIRE_READ_FIELD(this, kMapOffset)));
}


void HeapObject::synchronized_set_map_word(MapWord map_word) {
  RELEASE_WRITE_FIELD(
      this, kMapOffset, reinterpret_cast<Object*>(map_word.value_));
}

int HeapObject::Size() const { return SizeFromMap(map()); }

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

ACCESSORS(JSReceiver, raw_properties_or_hash, Object, kPropertiesOrHashOffset)

FixedArrayBase* JSObject::elements() const {
  Object* array = READ_FIELD(this, kElementsOffset);
  return static_cast<FixedArrayBase*>(array);
}

void AllocationSite::Initialize() {
  set_transition_info_or_boilerplate(Smi::kZero);
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::kZero);
  set_pretenure_data(0);
  set_pretenure_create_count(0);
  set_dependent_code(DependentCode::cast(GetHeap()->empty_fixed_array()),
                     SKIP_WRITE_BARRIER);
}

bool AllocationSite::IsZombie() const {
  return pretenure_decision() == kZombie;
}

bool AllocationSite::IsMaybeTenure() const {
  return pretenure_decision() == kMaybeTenure;
}

bool AllocationSite::PretenuringDecisionMade() const {
  return pretenure_decision() != kUndecided;
}


void AllocationSite::MarkZombie() {
  DCHECK(!IsZombie());
  Initialize();
  set_pretenure_decision(kZombie);
}

ElementsKind AllocationSite::GetElementsKind() const {
  return ElementsKindBits::decode(transition_info());
}


void AllocationSite::SetElementsKind(ElementsKind kind) {
  set_transition_info(ElementsKindBits::update(transition_info(), kind));
}

bool AllocationSite::CanInlineCall() const {
  return DoNotInlineBit::decode(transition_info()) == 0;
}


void AllocationSite::SetDoNotInlineCall() {
  set_transition_info(DoNotInlineBit::update(transition_info(), true));
}

bool AllocationSite::PointsToLiteral() const {
  Object* raw_value = transition_info_or_boilerplate();
  DCHECK_EQ(!raw_value->IsSmi(),
            raw_value->IsJSArray() || raw_value->IsJSObject());
  return !raw_value->IsSmi();
}


// Heuristic: We only need to create allocation site info if the boilerplate
// elements kind is the initial elements kind.
bool AllocationSite::ShouldTrack(ElementsKind boilerplate_elements_kind) {
  return IsSmiElementsKind(boilerplate_elements_kind);
}

inline bool AllocationSite::CanTrack(InstanceType type) {
  if (FLAG_allocation_site_pretenuring) {
    // TurboFan doesn't care at all about String pretenuring feedback,
    // so don't bother even trying to track that.
    return type == JS_ARRAY_TYPE || type == JS_OBJECT_TYPE;
  }
  return type == JS_ARRAY_TYPE;
}

AllocationSite::PretenureDecision AllocationSite::pretenure_decision() const {
  return PretenureDecisionBits::decode(pretenure_data());
}

void AllocationSite::set_pretenure_decision(PretenureDecision decision) {
  int value = pretenure_data();
  set_pretenure_data(PretenureDecisionBits::update(value, decision));
}

bool AllocationSite::deopt_dependent_code() const {
  return DeoptDependentCodeBit::decode(pretenure_data());
}

void AllocationSite::set_deopt_dependent_code(bool deopt) {
  int value = pretenure_data();
  set_pretenure_data(DeoptDependentCodeBit::update(value, deopt));
}

int AllocationSite::memento_found_count() const {
  return MementoFoundCountBits::decode(pretenure_data());
}

inline void AllocationSite::set_memento_found_count(int count) {
  int value = pretenure_data();
  // Verify that we can count more mementos than we can possibly find in one
  // new space collection.
  DCHECK((GetHeap()->MaxSemiSpaceSize() /
          (Heap::kMinObjectSizeInWords * kPointerSize +
           AllocationMemento::kSize)) < MementoFoundCountBits::kMax);
  DCHECK_LT(count, MementoFoundCountBits::kMax);
  set_pretenure_data(MementoFoundCountBits::update(value, count));
}

int AllocationSite::memento_create_count() const {
  return pretenure_create_count();
}

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

bool AllocationMemento::IsValid() const {
  return allocation_site()->IsAllocationSite() &&
         !AllocationSite::cast(allocation_site())->IsZombie();
}

AllocationSite* AllocationMemento::GetAllocationSite() const {
  DCHECK(IsValid());
  return AllocationSite::cast(allocation_site());
}

Address AllocationMemento::GetAllocationSiteUnchecked() const {
  return reinterpret_cast<Address>(allocation_site());
}

void JSObject::EnsureCanContainHeapObjectElements(Handle<JSObject> object) {
  JSObject::ValidateElements(*object);
  ElementsKind elements_kind = object->map()->elements_kind();
  if (!IsObjectElementsKind(elements_kind)) {
    if (IsHoleyElementsKind(elements_kind)) {
      TransitionElementsKind(object, HOLEY_ELEMENTS);
    } else {
      TransitionElementsKind(object, PACKED_ELEMENTS);
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
    bool is_holey = IsHoleyElementsKind(current_kind);
    if (current_kind == HOLEY_ELEMENTS) return;
    Object* the_hole = object->GetHeap()->the_hole_value();
    for (uint32_t i = 0; i < count; ++i) {
      Object* current = *objects++;
      if (current == the_hole) {
        is_holey = true;
        target_kind = GetHoleyElementsKind(target_kind);
      } else if (!current->IsSmi()) {
        if (mode == ALLOW_CONVERTED_DOUBLE_ELEMENTS && current->IsNumber()) {
          if (IsSmiElementsKind(target_kind)) {
            if (is_holey) {
              target_kind = HOLEY_DOUBLE_ELEMENTS;
            } else {
              target_kind = PACKED_DOUBLE_ELEMENTS;
            }
          }
        } else if (is_holey) {
          target_kind = HOLEY_ELEMENTS;
          break;
        } else {
          target_kind = PACKED_ELEMENTS;
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
  if (object->GetElementsKind() == HOLEY_SMI_ELEMENTS) {
    TransitionElementsKind(object, HOLEY_DOUBLE_ELEMENTS);
  } else if (object->GetElementsKind() == PACKED_SMI_ELEMENTS) {
    Handle<FixedDoubleArray> double_array =
        Handle<FixedDoubleArray>::cast(elements);
    for (uint32_t i = 0; i < length; ++i) {
      if (double_array->is_the_hole(i)) {
        TransitionElementsKind(object, HOLEY_DOUBLE_ELEMENTS);
        return;
      }
    }
    TransitionElementsKind(object, PACKED_DOUBLE_ELEMENTS);
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

byte Oddball::kind() const { return Smi::ToInt(READ_FIELD(this, kKindOffset)); }

void Oddball::set_kind(byte value) {
  WRITE_FIELD(this, kKindOffset, Smi::FromInt(value));
}


// static
Handle<Object> Oddball::ToNumber(Handle<Oddball> input) {
  return handle(input->to_number(), input->GetIsolate());
}


ACCESSORS(Cell, value, Object, kValueOffset)
ACCESSORS(PropertyCell, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS(PropertyCell, name, Name, kNameOffset)
ACCESSORS(PropertyCell, value, Object, kValueOffset)
ACCESSORS(PropertyCell, property_details_raw, Object, kDetailsOffset)

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
  Heap* heap = val->GetHeap();
  WriteBarrierMode mode =
      heap->incremental_marking()->marking_state()->IsBlack(this)
          ? UPDATE_WRITE_BARRIER
          : UPDATE_WEAK_WRITE_BARRIER;
  CONDITIONAL_WRITE_BARRIER(heap, this, kValueOffset, val, mode);
}

bool WeakCell::cleared() const { return value() == Smi::kZero; }

int JSObject::GetHeaderSize() const { return GetHeaderSize(map()); }

int JSObject::GetHeaderSize(const Map* map) {
  // Check for the most common kind of JavaScript object before
  // falling into the generic switch. This speeds up the internal
  // field operations considerably on average.
  InstanceType instance_type = map->instance_type();
  return instance_type == JS_OBJECT_TYPE
             ? JSObject::kHeaderSize
             : GetHeaderSize(instance_type, map->has_prototype_slot());
}

inline bool IsSpecialReceiverInstanceType(InstanceType instance_type) {
  return instance_type <= LAST_SPECIAL_RECEIVER_TYPE;
}

// This should be in objects/map-inl.h, but can't, because of a cyclic
// dependency.
bool Map::IsSpecialReceiverMap() const {
  bool result = IsSpecialReceiverInstanceType(instance_type());
  DCHECK_IMPLIES(!result,
                 !has_named_interceptor() && !is_access_check_needed());
  return result;
}

// static
int JSObject::GetEmbedderFieldCount(const Map* map) {
  int instance_size = map->instance_size();
  if (instance_size == kVariableSizeSentinel) return 0;
  return ((instance_size - GetHeaderSize(map)) >> kPointerSizeLog2) -
         map->GetInObjectProperties();
}

int JSObject::GetEmbedderFieldCount() const {
  return GetEmbedderFieldCount(map());
}

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

// Access fast-case object properties at index. The use of these routines
// is needed to correctly distinguish between properties stored in-object and
// properties stored in the properties array.
Object* JSObject::RawFastPropertyAt(FieldIndex index) {
  DCHECK(!IsUnboxedDoubleField(index));
  if (index.is_inobject()) {
    return READ_FIELD(this, index.offset());
  } else {
    return property_array()->get(index.outobject_array_index());
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
    property_array()->set(index.outobject_array_index(), value);
  }
}

void JSObject::RawFastDoublePropertyAsBitsAtPut(FieldIndex index,
                                                uint64_t bits) {
  // Double unboxing is enabled only on 64-bit platforms.
  DCHECK_EQ(kDoubleSize, kPointerSize);
  Address field_addr = FIELD_ADDR(this, index.offset());
  base::Relaxed_Store(reinterpret_cast<base::AtomicWord*>(field_addr),
                      static_cast<base::AtomicWord>(bits));
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
      bits = bit_cast<uint64_t>(static_cast<double>(Smi::ToInt(value)));
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
        size - (map->UnusedPropertyFields() * kPointerSize);
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

void Struct::InitializeBody(int object_size) {
  Object* value = GetHeap()->undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}

bool Object::ToArrayLength(uint32_t* index) const {
  return Object::ToUint32(index);
}

bool Object::ToArrayIndex(uint32_t* index) const {
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

Object* PropertyArray::get(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LE(index, this->length());
  return RELAXED_READ_FIELD(this, kHeaderSize + index * kPointerSize);
}

void PropertyArray::set(int index, Object* value) {
  DCHECK(IsPropertyArray());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kPointerSize;
  RELAXED_WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(GetHeap(), this, offset, value);
}

int RegExpMatchInfo::NumberOfCaptureRegisters() {
  DCHECK_GE(length(), kLastMatchOverhead);
  Object* obj = get(kNumberOfCapturesIndex);
  return Smi::ToInt(obj);
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
  return Smi::ToInt(obj);
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

AllocationAlignment HeapObject::RequiredAlignment() const {
#ifdef V8_HOST_ARCH_32_BIT
  if ((IsFixedFloat64Array() || IsFixedDoubleArray()) &&
      FixedArrayBase::cast(this)->length() != 0) {
    return kDoubleAligned;
  }
  if (IsHeapNumber()) return kDoubleUnaligned;
#endif  // V8_HOST_ARCH_32_BIT
  return kWordAligned;
}

bool HeapObject::NeedsRehashing() const {
  switch (map()->instance_type()) {
    case DESCRIPTOR_ARRAY_TYPE:
      return DescriptorArray::cast(this)->number_of_descriptors() > 1;
    case TRANSITION_ARRAY_TYPE:
      return TransitionArray::cast(this)->number_of_entries() > 1;
    case HASH_TABLE_TYPE:
      if (IsOrderedHashMap()) {
        return OrderedHashMap::cast(this)->NumberOfElements() > 0;
      } else if (IsOrderedHashSet()) {
        return OrderedHashSet::cast(this)->NumberOfElements() > 0;
      }
      return true;
    case SMALL_ORDERED_HASH_MAP_TYPE:
    case SMALL_ORDERED_HASH_SET_TYPE:
      return true;
    default:
      return false;
  }
}

void PropertyArray::set(int index, Object* value, WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kPointerSize;
  RELAXED_WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);
}

Object** PropertyArray::data_start() {
  return HeapObject::RawField(this, kHeaderSize);
}

ACCESSORS(EnumCache, keys, FixedArray, kKeysOffset)
ACCESSORS(EnumCache, indices, FixedArray, kIndicesOffset)

int DescriptorArray::number_of_descriptors() const {
  return Smi::ToInt(get(kDescriptorLengthIndex));
}

int DescriptorArray::number_of_descriptors_storage() const {
  return (length() - kFirstIndex) / kEntrySize;
}

int DescriptorArray::NumberOfSlackDescriptors() const {
  return number_of_descriptors_storage() - number_of_descriptors();
}


void DescriptorArray::SetNumberOfDescriptors(int number_of_descriptors) {
  set(kDescriptorLengthIndex, Smi::FromInt(number_of_descriptors));
}

inline int DescriptorArray::number_of_entries() const {
  return number_of_descriptors();
}

void DescriptorArray::CopyEnumCacheFrom(DescriptorArray* array) {
  set(kEnumCacheIndex, array->get(kEnumCacheIndex));
}

EnumCache* DescriptorArray::GetEnumCache() {
  return EnumCache::cast(get(kEnumCacheIndex));
}

// Perform a binary search in a fixed array.
template <SearchMode search_mode, typename T>
int BinarySearch(T* array, Name* name, int valid_entries,
                 int* out_insertion_index) {
  DCHECK(search_mode == ALL_ENTRIES || out_insertion_index == nullptr);
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
  return internal::Search<VALID_ENTRIES>(this, name, valid_descriptors,
                                         nullptr);
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
  DCHECK_EQ(GetDetails(descriptor_number).location(), kField);
  return GetDetails(descriptor_number).field_index();
}

FieldType* DescriptorArray::GetFieldType(int descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), kField);
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

bool StringSetShape::IsMatch(String* key, Object* value) {
  DCHECK(value->IsString());
  return key->Equals(String::cast(value));
}

uint32_t StringSetShape::Hash(Isolate* isolate, String* key) {
  return key->Hash();
}

uint32_t StringSetShape::HashForObject(Isolate* isolate, Object* object) {
  return String::cast(object)->Hash();
}

StringTableKey::StringTableKey(uint32_t hash_field)
    : HashTableKey(hash_field >> Name::kHashShift), hash_field_(hash_field) {}

void StringTableKey::set_hash_field(uint32_t hash_field) {
  hash_field_ = hash_field;
  set_hash(hash_field >> Name::kHashShift);
}

Handle<Object> StringTableShape::AsHandle(Isolate* isolate,
                                          StringTableKey* key) {
  return key->AsHandle(isolate);
}

uint32_t StringTableShape::HashForObject(Isolate* isolate, Object* object) {
  return String::cast(object)->Hash();
}

int StringTableShape::GetMapRootIndex() {
  return Heap::kStringTableMapRootIndex;
}

bool NumberDictionary::requires_slow_elements() {
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return false;
  return 0 != (Smi::ToInt(max_index_object) & kRequiresSlowElementsMask);
}

uint32_t NumberDictionary::max_number_key() {
  DCHECK(!requires_slow_elements());
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return 0;
  uint32_t value = static_cast<uint32_t>(Smi::ToInt(max_index_object));
  return value >> kRequiresSlowElementsTagSize;
}

void NumberDictionary::set_requires_slow_elements() {
  set(kMaxNumberKeyIndex, Smi::FromInt(kRequiresSlowElementsMask));
}

DEFINE_DEOPT_ELEMENT_ACCESSORS(TranslationByteArray, ByteArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(OptimizationId, Smi)
DEFINE_DEOPT_ELEMENT_ACCESSORS(WeakCellCache, Object)
DEFINE_DEOPT_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)

DEFINE_DEOPT_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(TranslationIndex, Smi)
DEFINE_DEOPT_ENTRY_ACCESSORS(Pc, Smi)

int PropertyArray::length() const {
  Object* value_obj = READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return LengthField::decode(value);
}

void PropertyArray::initialize_length(int len) {
  SLOW_DCHECK(len >= 0);
  SLOW_DCHECK(len < LengthField::kMax);
  WRITE_FIELD(this, kLengthAndHashOffset, Smi::FromInt(len));
}

int PropertyArray::synchronized_length() const {
  Object* value_obj = ACQUIRE_READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return LengthField::decode(value);
}

int PropertyArray::Hash() const {
  Object* value_obj = READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  return HashField::decode(value);
}

void PropertyArray::SetHash(int hash) {
  Object* value_obj = READ_FIELD(this, kLengthAndHashOffset);
  int value = Smi::ToInt(value_obj);
  value = HashField::update(value, hash);
  WRITE_FIELD(this, kLengthAndHashOffset, Smi::FromInt(value));
}

SMI_ACCESSORS(FreeSpace, size, kSizeOffset)
RELAXED_SMI_ACCESSORS(FreeSpace, size, kSizeOffset)


int FreeSpace::Size() { return size(); }


FreeSpace* FreeSpace::next() {
  DCHECK(map() == GetHeap()->root(Heap::kFreeSpaceMapRootIndex) ||
         (!GetHeap()->deserialization_complete() && map() == nullptr));
  DCHECK_LE(kNextOffset + kPointerSize, relaxed_read_size());
  return reinterpret_cast<FreeSpace*>(
      Memory::Address_at(address() + kNextOffset));
}


void FreeSpace::set_next(FreeSpace* next) {
  DCHECK(map() == GetHeap()->root(Heap::kFreeSpaceMapRootIndex) ||
         (!GetHeap()->deserialization_complete() && map() == nullptr));
  DCHECK_LE(kNextOffset + kPointerSize, relaxed_read_size());
  base::Relaxed_Store(
      reinterpret_cast<base::AtomicWord*>(address() + kNextOffset),
      reinterpret_cast<base::AtomicWord>(next));
}


FreeSpace* FreeSpace::cast(HeapObject* o) {
  SLOW_DCHECK(!o->GetHeap()->deserialization_complete() || o->IsFreeSpace());
  return reinterpret_cast<FreeSpace*>(o);
}

int HeapObject::SizeFromMap(Map* map) const {
  int instance_size = map->instance_size();
  if (instance_size != kVariableSizeSentinel) return instance_size;
  // Only inline the most frequent cases.
  InstanceType instance_type = map->instance_type();
  if (instance_type >= FIRST_FIXED_ARRAY_TYPE &&
      instance_type <= LAST_FIXED_ARRAY_TYPE) {
    return FixedArray::SizeFor(
        reinterpret_cast<const FixedArray*>(this)->synchronized_length());
  }
  if (instance_type == ONE_BYTE_STRING_TYPE ||
      instance_type == ONE_BYTE_INTERNALIZED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqOneByteString::SizeFor(
        reinterpret_cast<const SeqOneByteString*>(this)->synchronized_length());
  }
  if (instance_type == BYTE_ARRAY_TYPE) {
    return ByteArray::SizeFor(
        reinterpret_cast<const ByteArray*>(this)->synchronized_length());
  }
  if (instance_type == BYTECODE_ARRAY_TYPE) {
    return BytecodeArray::SizeFor(
        reinterpret_cast<const BytecodeArray*>(this)->synchronized_length());
  }
  if (instance_type == FREE_SPACE_TYPE) {
    return reinterpret_cast<const FreeSpace*>(this)->relaxed_read_size();
  }
  if (instance_type == STRING_TYPE ||
      instance_type == INTERNALIZED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqTwoByteString::SizeFor(
        reinterpret_cast<const SeqTwoByteString*>(this)->synchronized_length());
  }
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
    return FixedDoubleArray::SizeFor(
        reinterpret_cast<const FixedDoubleArray*>(this)->synchronized_length());
  }
  if (instance_type >= FIRST_FIXED_TYPED_ARRAY_TYPE &&
      instance_type <= LAST_FIXED_TYPED_ARRAY_TYPE) {
    return reinterpret_cast<const FixedTypedArrayBase*>(this)->TypedArraySize(
        instance_type);
  }
  if (instance_type == SMALL_ORDERED_HASH_SET_TYPE) {
    return reinterpret_cast<const SmallOrderedHashSet*>(this)->Size();
  }
  if (instance_type == PROPERTY_ARRAY_TYPE) {
    return PropertyArray::SizeFor(
        reinterpret_cast<const PropertyArray*>(this)->synchronized_length());
  }
  if (instance_type == SMALL_ORDERED_HASH_MAP_TYPE) {
    return reinterpret_cast<const SmallOrderedHashMap*>(this)->Size();
  }
  if (instance_type == FEEDBACK_VECTOR_TYPE) {
    return FeedbackVector::SizeFor(
        reinterpret_cast<const FeedbackVector*>(this)->length());
  }
  if (instance_type == BIGINT_TYPE) {
    return BigInt::SizeFor(reinterpret_cast<const BigInt*>(this)->length());
  }
  DCHECK(instance_type == CODE_TYPE);
  return reinterpret_cast<const Code*>(this)->CodeSize();
}

Object* JSBoundFunction::raw_bound_target_function() const {
  return READ_FIELD(this, kBoundTargetFunctionOffset);
}

ACCESSORS(JSBoundFunction, bound_target_function, JSReceiver,
          kBoundTargetFunctionOffset)
ACCESSORS(JSBoundFunction, bound_this, Object, kBoundThisOffset)
ACCESSORS(JSBoundFunction, bound_arguments, FixedArray, kBoundArgumentsOffset)

ACCESSORS(JSFunction, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
ACCESSORS(JSFunction, feedback_vector_cell, Cell, kFeedbackVectorOffset)

ACCESSORS(JSGlobalObject, native_context, Context, kNativeContextOffset)
ACCESSORS(JSGlobalObject, global_proxy, JSObject, kGlobalProxyOffset)

ACCESSORS(JSGlobalProxy, native_context, Object, kNativeContextOffset)

ACCESSORS(AccessorInfo, name, Name, kNameOffset)
SMI_ACCESSORS(AccessorInfo, flags, kFlagsOffset)
ACCESSORS(AccessorInfo, expected_receiver_type, Object,
          kExpectedReceiverTypeOffset)

ACCESSORS_CHECKED2(AccessorInfo, getter, Object, kGetterOffset, true,
                   Foreign::IsNormalized(value))
ACCESSORS_CHECKED2(AccessorInfo, setter, Object, kSetterOffset, true,
                   Foreign::IsNormalized(value));
ACCESSORS(AccessorInfo, js_getter, Object, kJsGetterOffset)
ACCESSORS(AccessorInfo, data, Object, kDataOffset)

bool AccessorInfo::has_getter() {
  bool result = getter() != Smi::kZero;
  DCHECK_EQ(result, getter() != Smi::kZero &&
                        Foreign::cast(getter())->foreign_address() != nullptr);
  return result;
}

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
bool ConstantElementsPair::is_empty() const {
  return constant_values()->length() == 0;
}

SMI_ACCESSORS(TemplateObjectDescription, hash, kHashOffset)
ACCESSORS(TemplateObjectDescription, raw_strings, FixedArray, kRawStringsOffset)
ACCESSORS(TemplateObjectDescription, cooked_strings, FixedArray,
          kCookedStringsOffset)

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
BOOL_ACCESSORS(InterceptorInfo, flags, is_named, kNamed)

ACCESSORS(CallHandlerInfo, callback, Object, kCallbackOffset)
ACCESSORS(CallHandlerInfo, js_callback, Object, kJsCallbackOffset)
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
  return EmbedderFieldCount::decode(Smi::ToInt(value));
}

void ObjectTemplateInfo::set_embedder_field_count(int count) {
  DCHECK_LE(count, JSObject::kMaxEmbedderFields);
  return set_data(
      Smi::FromInt(EmbedderFieldCount::update(Smi::ToInt(data()), count)));
}

bool ObjectTemplateInfo::immutable_proto() const {
  Object* value = data();
  DCHECK(value->IsSmi());
  return IsImmutablePrototype::decode(Smi::ToInt(value));
}

void ObjectTemplateInfo::set_immutable_proto(bool immutable) {
  return set_data(Smi::FromInt(
      IsImmutablePrototype::update(Smi::ToInt(data()), immutable)));
}

ACCESSORS(AllocationSite, transition_info_or_boilerplate, Object,
          kTransitionInfoOrBoilerplateOffset)

JSObject* AllocationSite::boilerplate() const {
  DCHECK(PointsToLiteral());
  return JSObject::cast(transition_info_or_boilerplate());
}

void AllocationSite::set_boilerplate(JSObject* object, WriteBarrierMode mode) {
  set_transition_info_or_boilerplate(object, mode);
}

int AllocationSite::transition_info() const {
  DCHECK(!PointsToLiteral());
  return Smi::cast(transition_info_or_boilerplate())->value();
}

void AllocationSite::set_transition_info(int value) {
  DCHECK(!PointsToLiteral());
  set_transition_info_or_boilerplate(Smi::FromInt(value), SKIP_WRITE_BARRIER);
}

ACCESSORS(AllocationSite, nested_site, Object, kNestedSiteOffset)
SMI_ACCESSORS(AllocationSite, pretenure_data, kPretenureDataOffset)
SMI_ACCESSORS(AllocationSite, pretenure_create_count,
              kPretenureCreateCountOffset)
ACCESSORS(AllocationSite, dependent_code, DependentCode,
          kDependentCodeOffset)
ACCESSORS(AllocationSite, weak_next, Object, kWeakNextOffset)
ACCESSORS(AllocationMemento, allocation_site, Object, kAllocationSiteOffset)

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
          NumberDictionary, kStackFrameCacheIndex)

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

FeedbackVector* JSFunction::feedback_vector() const {
  DCHECK(feedback_vector_cell()->value()->IsFeedbackVector());
  return FeedbackVector::cast(feedback_vector_cell()->value());
}

// Code objects that are marked for deoptimization are not considered to be
// optimized. This is because the JSFunction might have been already
// deoptimized but its code() still needs to be unlinked, which will happen on
// its next activation.
// TODO(jupvfranco): rename this function. Maybe RunOptimizedCode,
// or IsValidOptimizedCode.
bool JSFunction::IsOptimized() {
  return code()->kind() == Code::OPTIMIZED_FUNCTION &&
         !code()->marked_for_deoptimization();
}

bool JSFunction::HasOptimizedCode() {
  return IsOptimized() ||
         (has_feedback_vector() && feedback_vector()->has_optimized_code() &&
          !feedback_vector()->optimized_code()->marked_for_deoptimization());
}

bool JSFunction::HasOptimizationMarker() {
  return has_feedback_vector() && feedback_vector()->has_optimization_marker();
}

void JSFunction::ClearOptimizationMarker() {
  DCHECK(has_feedback_vector());
  feedback_vector()->ClearOptimizationMarker();
}

bool JSFunction::IsInterpreted() {
  return code()->is_interpreter_trampoline_builtin();
}

bool JSFunction::ChecksOptimizationMarker() {
  return code()->checks_optimization_marker();
}

bool JSFunction::IsMarkedForOptimization() {
  return has_feedback_vector() && feedback_vector()->optimization_marker() ==
                                      OptimizationMarker::kCompileOptimized;
}


bool JSFunction::IsMarkedForConcurrentOptimization() {
  return has_feedback_vector() &&
         feedback_vector()->optimization_marker() ==
             OptimizationMarker::kCompileOptimizedConcurrent;
}


bool JSFunction::IsInOptimizationQueue() {
  return has_feedback_vector() && feedback_vector()->optimization_marker() ==
                                      OptimizationMarker::kInOptimizationQueue;
}


void JSFunction::CompleteInobjectSlackTrackingIfActive() {
  if (!has_prototype_slot()) return;
  if (has_initial_map() && initial_map()->IsInobjectSlackTrackingInProgress()) {
    initial_map()->CompleteInobjectSlackTracking();
  }
}

AbstractCode* JSFunction::abstract_code() {
  if (IsInterpreted()) {
    return AbstractCode::cast(shared()->bytecode_array());
  } else {
    return AbstractCode::cast(code());
  }
}

Code* JSFunction::code() { return Code::cast(READ_FIELD(this, kCodeOffset)); }

void JSFunction::set_code(Code* value) {
  DCHECK(!GetHeap()->InNewSpace(value));
  WRITE_FIELD(this, kCodeOffset, value);
  GetHeap()->incremental_marking()->RecordWrite(
      this, HeapObject::RawField(this, kCodeOffset), value);
}


void JSFunction::set_code_no_write_barrier(Code* value) {
  DCHECK(!GetHeap()->InNewSpace(value));
  WRITE_FIELD(this, kCodeOffset, value);
}

void JSFunction::ClearOptimizedCodeSlot(const char* reason) {
  if (has_feedback_vector() && feedback_vector()->has_optimized_code()) {
    if (FLAG_trace_opt) {
      PrintF("[evicting entry from optimizing code feedback slot (%s) for ",
             reason);
      ShortPrint();
      PrintF("]\n");
    }
    feedback_vector()->ClearOptimizedCode();
  }
}

void JSFunction::SetOptimizationMarker(OptimizationMarker marker) {
  DCHECK(has_feedback_vector());
  DCHECK(ChecksOptimizationMarker());
  DCHECK(!HasOptimizedCode());

  feedback_vector()->SetOptimizationMarker(marker);
}

bool JSFunction::has_feedback_vector() const {
  return !feedback_vector_cell()->value()->IsUndefined(GetIsolate());
}

JSFunction::FeedbackVectorState JSFunction::GetFeedbackVectorState(
    Isolate* isolate) const {
  Cell* cell = feedback_vector_cell();
  if (shared()->HasAsmWasmData()) {
    return NO_VECTOR_NEEDED;
  } else if (cell == isolate->heap()->undefined_cell()) {
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

ACCESSORS_CHECKED(JSFunction, prototype_or_initial_map, Object,
                  kPrototypeOrInitialMapOffset, map()->has_prototype_slot())

bool JSFunction::has_prototype_slot() const {
  return map()->has_prototype_slot();
}

Map* JSFunction::initial_map() {
  return Map::cast(prototype_or_initial_map());
}


bool JSFunction::has_initial_map() {
  DCHECK(has_prototype_slot());
  return prototype_or_initial_map()->IsMap();
}


bool JSFunction::has_instance_prototype() {
  DCHECK(has_prototype_slot());
  return has_initial_map() ||
         !prototype_or_initial_map()->IsTheHole(GetIsolate());
}


bool JSFunction::has_prototype() {
  DCHECK(has_prototype_slot());
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
  return code() != builtins->builtin(Builtins::kCompileLazy);
}

ACCESSORS(JSProxy, target, Object, kTargetOffset)
ACCESSORS(JSProxy, handler, Object, kHandlerOffset)

bool JSProxy::IsRevoked() const { return !handler()->IsJSReceiver(); }

// static
bool Foreign::IsNormalized(Object* value) {
  if (value == Smi::kZero) return true;
  return Foreign::cast(value)->foreign_address() != nullptr;
}

Address Foreign::foreign_address() {
  return AddressFrom<Address>(READ_INTPTR_FIELD(this, kForeignAddressOffset));
}

void Foreign::set_foreign_address(Address value) {
  WRITE_INTPTR_FIELD(this, kForeignAddressOffset, OffsetFrom(value));
}

template <class Derived>
void SmallOrderedHashTable<Derived>::SetDataEntry(int entry, int relative_index,
                                                  Object* value) {
  int entry_offset = GetDataEntryOffset(entry, relative_index);
  RELAXED_WRITE_FIELD(this, entry_offset, value);
  WRITE_BARRIER(GetHeap(), this, entry_offset, value);
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



ACCESSORS(PromiseCapability, promise, Object, kPromiseOffset)
ACCESSORS(PromiseCapability, resolve, Object, kResolveOffset)
ACCESSORS(PromiseCapability, reject, Object, kRejectOffset)

ACCESSORS(JSPromise, result, Object, kResultOffset)
ACCESSORS(JSPromise, deferred_promise, Object, kDeferredPromiseOffset)
ACCESSORS(JSPromise, deferred_on_resolve, Object, kDeferredOnResolveOffset)
ACCESSORS(JSPromise, deferred_on_reject, Object, kDeferredOnRejectOffset)
ACCESSORS(JSPromise, fulfill_reactions, Object, kFulfillReactionsOffset)
ACCESSORS(JSPromise, reject_reactions, Object, kRejectReactionsOffset)
SMI_ACCESSORS(JSPromise, flags, kFlagsOffset)
BOOL_ACCESSORS(JSPromise, flags, has_handler, kHasHandlerBit)
BOOL_ACCESSORS(JSPromise, flags, handled_hint, kHandledHintBit)


ElementsKind JSObject::GetElementsKind() {
  ElementsKind kind = map()->elements_kind();
#if VERIFY_HEAP && DEBUG
  FixedArrayBase* fixed_array =
      reinterpret_cast<FixedArrayBase*>(READ_FIELD(this, kElementsOffset));

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    Map* map = fixed_array->map();
    if (IsSmiOrObjectElementsKind(kind)) {
      DCHECK(map == GetHeap()->fixed_array_map() ||
             map == GetHeap()->fixed_cow_array_map());
    } else if (IsDoubleElementsKind(kind)) {
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

bool JSObject::HasObjectElements() {
  return IsObjectElementsKind(GetElementsKind());
}

bool JSObject::HasSmiElements() { return IsSmiElementsKind(GetElementsKind()); }

bool JSObject::HasSmiOrObjectElements() {
  return IsSmiOrObjectElementsKind(GetElementsKind());
}

bool JSObject::HasDoubleElements() {
  return IsDoubleElementsKind(GetElementsKind());
}

bool JSObject::HasHoleyElements() {
  return IsHoleyElementsKind(GetElementsKind());
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
    DCHECK_NOT_NULL(array);                                            \
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

void JSGlobalObject::set_global_dictionary(GlobalDictionary* dictionary) {
  DCHECK(IsJSGlobalObject());
  set_raw_properties_or_hash(dictionary);
}

GlobalDictionary* JSGlobalObject::global_dictionary() {
  DCHECK(!HasFastProperties());
  DCHECK(IsJSGlobalObject());
  return GlobalDictionary::cast(raw_properties_or_hash());
}

NumberDictionary* JSObject::element_dictionary() {
  DCHECK(HasDictionaryElements() || HasSlowStringWrapperElements());
  return NumberDictionary::cast(elements());
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
  DCHECK(!GetHeap()->InNewSpace(GetHeap()->empty_property_dictionary()));
  if (map()->is_dictionary_map()) {
    WRITE_FIELD(this, kPropertiesOrHashOffset,
                GetHeap()->empty_property_dictionary());
  } else {
    WRITE_FIELD(this, kPropertiesOrHashOffset, GetHeap()->empty_fixed_array());
  }
}

bool JSReceiver::HasFastProperties() const {
  DCHECK(
      raw_properties_or_hash()->IsSmi() ||
      (raw_properties_or_hash()->IsDictionary() == map()->is_dictionary_map()));
  return !map()->is_dictionary_map();
}

NameDictionary* JSReceiver::property_dictionary() const {
  DCHECK(!IsJSGlobalObject());
  DCHECK(!HasFastProperties());

  Object* prop = raw_properties_or_hash();
  if (prop->IsSmi()) {
    return GetHeap()->empty_property_dictionary();
  }

  return NameDictionary::cast(prop);
}

// TODO(gsathya): Pass isolate directly to this function and access
// the heap from this.
PropertyArray* JSReceiver::property_array() const {
  DCHECK(HasFastProperties());

  Object* prop = raw_properties_or_hash();
  if (prop->IsSmi() || prop == GetHeap()->empty_fixed_array()) {
    return GetHeap()->empty_property_array();
  }

  return PropertyArray::cast(prop);
}

Maybe<bool> JSReceiver::HasProperty(Handle<JSReceiver> object,
                                    Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(object->GetIsolate(),
                                                        object, name, object);
  return HasProperty(&it);
}


Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       uint32_t index) {
  if (object->IsJSModuleNamespace()) return Just(false);

  if (object->IsJSObject()) {  // Shortcut.
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

BIT_FIELD_ACCESSORS(AccessorInfo, flags, all_can_read,
                    AccessorInfo::AllCanReadBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, all_can_write,
                    AccessorInfo::AllCanWriteBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, is_special_data_property,
                    AccessorInfo::IsSpecialDataPropertyBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, replace_on_access,
                    AccessorInfo::ReplaceOnAccessBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, is_sloppy, AccessorInfo::IsSloppyBit)
BIT_FIELD_ACCESSORS(AccessorInfo, flags, initial_property_attributes,
                    AccessorInfo::InitialAttributesBits)

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

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::ClearEntry(int entry) {
  Object* the_hole = this->GetHeap()->the_hole_value();
  PropertyDetails details = PropertyDetails::Empty();
  Derived::cast(this)->SetEntry(entry, the_hole, the_hole, details);
}

template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::SetEntry(int entry, Object* key, Object* value,
                                          PropertyDetails details) {
  DCHECK(Dictionary::kEntrySize == 2 || Dictionary::kEntrySize == 3);
  DCHECK(!key->IsName() || details.dictionary_index() > 0);
  int index = DerivedHashTable::EntryToIndex(entry);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = this->GetWriteBarrierMode(no_gc);
  this->set(index + Derived::kEntryKeyIndex, key, mode);
  this->set(index + Derived::kEntryValueIndex, value, mode);
  if (Shape::kHasDetails) DetailsAtPut(entry, details);
}

Object* GlobalDictionaryShape::Unwrap(Object* object) {
  return PropertyCell::cast(object)->name();
}

int GlobalDictionaryShape::GetMapRootIndex() {
  return Heap::kGlobalDictionaryMapRootIndex;
}

Name* NameDictionary::NameAt(int entry) { return Name::cast(KeyAt(entry)); }

int NameDictionaryShape::GetMapRootIndex() {
  return Heap::kNameDictionaryMapRootIndex;
}

PropertyCell* GlobalDictionary::CellAt(int entry) {
  DCHECK(KeyAt(entry)->IsPropertyCell());
  return PropertyCell::cast(KeyAt(entry));
}

bool GlobalDictionaryShape::IsLive(Isolate* isolate, Object* k) {
  Heap* heap = isolate->heap();
  DCHECK_NE(heap->the_hole_value(), k);
  return k != heap->undefined_value();
}

bool GlobalDictionaryShape::IsKey(Isolate* isolate, Object* k) {
  return IsLive(isolate, k) &&
         !PropertyCell::cast(k)->value()->IsTheHole(isolate);
}

Name* GlobalDictionary::NameAt(int entry) { return CellAt(entry)->name(); }
Object* GlobalDictionary::ValueAt(int entry) { return CellAt(entry)->value(); }

void GlobalDictionary::SetEntry(int entry, Object* key, Object* value,
                                PropertyDetails details) {
  DCHECK_EQ(key, PropertyCell::cast(value)->name());
  set(EntryToIndex(entry) + kEntryKeyIndex, value);
  DetailsAtPut(entry, details);
}

void GlobalDictionary::ValueAtPut(int entry, Object* value) {
  set(EntryToIndex(entry), value);
}

bool NumberDictionaryShape::IsMatch(uint32_t key, Object* other) {
  DCHECK(other->IsNumber());
  return key == static_cast<uint32_t>(other->Number());
}

uint32_t NumberDictionaryShape::Hash(Isolate* isolate, uint32_t key) {
  return ComputeIntegerHash(key, isolate->heap()->HashSeed());
}

uint32_t NumberDictionaryShape::HashForObject(Isolate* isolate, Object* other) {
  DCHECK(other->IsNumber());
  return ComputeIntegerHash(static_cast<uint32_t>(other->Number()),
                            isolate->heap()->HashSeed());
}

int NumberDictionaryShape::GetMapRootIndex() {
  return Heap::kNumberDictionaryMapRootIndex;
}

Handle<Object> NumberDictionaryShape::AsHandle(Isolate* isolate, uint32_t key) {
  return isolate->factory()->NewNumberFromUint(key);
}


bool NameDictionaryShape::IsMatch(Handle<Name> key, Object* other) {
  DCHECK(other->IsTheHole(key->GetIsolate()) ||
         Name::cast(other)->IsUniqueName());
  DCHECK(key->IsUniqueName());
  return *key == other;
}

uint32_t NameDictionaryShape::Hash(Isolate* isolate, Handle<Name> key) {
  return key->Hash();
}

uint32_t NameDictionaryShape::HashForObject(Isolate* isolate, Object* other) {
  return Name::cast(other)->Hash();
}

bool GlobalDictionaryShape::IsMatch(Handle<Name> key, Object* other) {
  DCHECK(PropertyCell::cast(other)->name()->IsUniqueName());
  return *key == PropertyCell::cast(other)->name();
}

uint32_t GlobalDictionaryShape::HashForObject(Isolate* isolate, Object* other) {
  return PropertyCell::cast(other)->name()->Hash();
}

Handle<Object> NameDictionaryShape::AsHandle(Isolate* isolate,
                                             Handle<Name> key) {
  DCHECK(key->IsUniqueName());
  return key;
}


template <typename Dictionary>
PropertyDetails GlobalDictionaryShape::DetailsAt(Dictionary* dict, int entry) {
  DCHECK_LE(0, entry);  // Not found is -1, which is not caught by get().
  return dict->CellAt(entry)->property_details();
}


template <typename Dictionary>
void GlobalDictionaryShape::DetailsAtPut(Dictionary* dict, int entry,
                                         PropertyDetails value) {
  DCHECK_LE(0, entry);  // Not found is -1, which is not caught by get().
  PropertyCell* cell = dict->CellAt(entry);
  if (cell->property_details().IsReadOnly() != value.IsReadOnly()) {
    cell->dependent_code()->DeoptimizeDependentCodeGroup(
        cell->GetIsolate(), DependentCode::kPropertyCellChangedGroup);
  }
  cell->set_property_details(value);
}

bool ObjectHashTableShape::IsMatch(Handle<Object> key, Object* other) {
  return key->SameValue(other);
}

uint32_t ObjectHashTableShape::Hash(Isolate* isolate, Handle<Object> key) {
  return Smi::ToInt(key->GetHash());
}

uint32_t ObjectHashTableShape::HashForObject(Isolate* isolate, Object* other) {
  return Smi::ToInt(other->GetHash());
}


Handle<Object> ObjectHashTableShape::AsHandle(Isolate* isolate,
                                              Handle<Object> key) {
  return key;
}

Handle<ObjectHashTable> ObjectHashTable::Shrink(Handle<ObjectHashTable> table) {
  return DerivedHashTable::Shrink(table);
}

bool WeakHashTableShape::IsMatch(Handle<Object> key, Object* other) {
  if (other->IsWeakCell()) other = WeakCell::cast(other)->value();
  return key->IsWeakCell() ? WeakCell::cast(*key)->value() == other
                           : *key == other;
}

uint32_t WeakHashTableShape::Hash(Isolate* isolate, Handle<Object> key) {
  intptr_t hash =
      key->IsWeakCell()
          ? reinterpret_cast<intptr_t>(WeakCell::cast(*key)->value())
          : reinterpret_cast<intptr_t>(*key);
  return (uint32_t)(hash & 0xFFFFFFFF);
}

uint32_t WeakHashTableShape::HashForObject(Isolate* isolate, Object* other) {
  if (other->IsWeakCell()) other = WeakCell::cast(other)->value();
  intptr_t hash = reinterpret_cast<intptr_t>(other);
  return (uint32_t)(hash & 0xFFFFFFFF);
}

Handle<Object> WeakHashTableShape::AsHandle(Isolate* isolate,
                                            Handle<Object> key) {
  return key;
}

int WeakHashTableShape::GetMapRootIndex() {
  return Heap::kWeakHashTableMapRootIndex;
}

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
  int index = Smi::ToInt(this->index());
  Object* key = table->KeyAt(index);
  DCHECK(!key->IsTheHole(table->GetIsolate()));
  return key;
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
                                                    PACKED_ELEMENTS, 2);
}

static inline Handle<Object> MakeEntryPair(Isolate* isolate, Handle<Object> key,
                                           Handle<Object> value) {
  Handle<FixedArray> entry_storage =
      isolate->factory()->NewUninitializedFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    PACKED_ELEMENTS, 2);
}

ACCESSORS(JSIteratorResult, value, Object, kValueOffset)
ACCESSORS(JSIteratorResult, done, Object, kDoneOffset)

ACCESSORS(JSAsyncFromSyncIterator, sync_iterator, JSReceiver,
          kSyncIteratorOffset)

ACCESSORS(JSStringIterator, string, String, kStringOffset)
SMI_ACCESSORS(JSStringIterator, index, kNextIndexOffset)

bool ScopeInfo::IsAsmModule() { return AsmModuleField::decode(Flags()); }

bool ScopeInfo::HasSimpleParameters() {
  return HasSimpleParametersField::decode(Flags());
}

#define FIELD_ACCESSORS(name)                                                 \
  void ScopeInfo::Set##name(int value) { set(k##name, Smi::FromInt(value)); } \
  int ScopeInfo::name() {                                                     \
    if (length() > 0) {                                                       \
      return Smi::ToInt(get(k##name));                                        \
    } else {                                                                  \
      return 0;                                                               \
    }                                                                         \
  }
FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(FIELD_ACCESSORS)
#undef FIELD_ACCESSORS

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_INL_H_
