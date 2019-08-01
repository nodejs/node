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

#ifndef V8_OBJECTS_OBJECTS_INL_H_
#define V8_OBJECTS_OBJECTS_INL_H_

#include "src/objects/objects.h"

#include "src/base/bits.h"
#include "src/builtins/builtins.h"
#include "src/common/v8memory.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/numbers/conversions.h"
#include "src/numbers/double.h"
#include "src/objects/bigint.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-proxy-inl.h"  // TODO(jkummerow): Drop.
#include "src/objects/keys.h"
#include "src/objects/literal-objects.h"
#include "src/objects/lookup-inl.h"  // TODO(jkummerow): Drop.
#include "src/objects/oddball.h"
#include "src/objects/property-details.h"
#include "src/objects/property.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/scope-info.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/objects/templates.h"
#include "src/sanitizer/tsan.h"
#include "torque-generated/class-definitions-tq-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

PropertyDetails::PropertyDetails(Smi smi) { value_ = smi.value(); }

Smi PropertyDetails::AsSmi() const {
  // Ensure the upper 2 bits have the same value by sign extending it. This is
  // necessary to be able to use the 31st bit of the property details.
  int value = value_ << 1;
  return Smi::FromInt(value >> 1);
}

int PropertyDetails::field_width_in_words() const {
  DCHECK_EQ(location(), kField);
  if (!FLAG_unbox_double_fields) return 1;
  if (kDoubleSize == kTaggedSize) return 1;
  return representation().IsDouble() ? kDoubleSize / kTaggedSize : 1;
}

bool HeapObject::IsSloppyArgumentsElements() const {
  return IsFixedArrayExact();
}

bool HeapObject::IsJSSloppyArgumentsObject() const {
  return IsJSArgumentsObject();
}

bool HeapObject::IsJSGeneratorObject() const {
  return map().instance_type() == JS_GENERATOR_OBJECT_TYPE ||
         IsJSAsyncFunctionObject() || IsJSAsyncGeneratorObject();
}

bool HeapObject::IsDataHandler() const {
  return IsLoadHandler() || IsStoreHandler();
}

bool HeapObject::IsClassBoilerplate() const { return IsFixedArrayExact(); }

#define IS_TYPE_FUNCTION_DEF(type_)                               \
  bool Object::Is##type_() const {                                \
    return IsHeapObject() && HeapObject::cast(*this).Is##type_(); \
  }
HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DEF)
#undef IS_TYPE_FUNCTION_DEF

#define IS_TYPE_FUNCTION_DEF(Type, Value)                        \
  bool Object::Is##Type(Isolate* isolate) const {                \
    return Is##Type(ReadOnlyRoots(isolate->heap()));             \
  }                                                              \
  bool Object::Is##Type(ReadOnlyRoots roots) const {             \
    return *this == roots.Value();                               \
  }                                                              \
  bool Object::Is##Type() const {                                \
    return IsHeapObject() && HeapObject::cast(*this).Is##Type(); \
  }                                                              \
  bool HeapObject::Is##Type(Isolate* isolate) const {            \
    return Object::Is##Type(isolate);                            \
  }                                                              \
  bool HeapObject::Is##Type(ReadOnlyRoots roots) const {         \
    return Object::Is##Type(roots);                              \
  }                                                              \
  bool HeapObject::Is##Type() const { return Is##Type(GetReadOnlyRoots()); }
ODDBALL_LIST(IS_TYPE_FUNCTION_DEF)
#undef IS_TYPE_FUNCTION_DEF

bool Object::IsNullOrUndefined(Isolate* isolate) const {
  return IsNullOrUndefined(ReadOnlyRoots(isolate));
}

bool Object::IsNullOrUndefined(ReadOnlyRoots roots) const {
  return IsNull(roots) || IsUndefined(roots);
}

bool Object::IsNullOrUndefined() const {
  return IsHeapObject() && HeapObject::cast(*this).IsNullOrUndefined();
}

bool Object::IsZero() const { return *this == Smi::zero(); }

bool Object::IsNoSharedNameSentinel() const {
  return *this == SharedFunctionInfo::kNoSharedNameSentinel;
}

bool HeapObject::IsNullOrUndefined(Isolate* isolate) const {
  return Object::IsNullOrUndefined(isolate);
}

bool HeapObject::IsNullOrUndefined(ReadOnlyRoots roots) const {
  return Object::IsNullOrUndefined(roots);
}

bool HeapObject::IsNullOrUndefined() const {
  return IsNullOrUndefined(GetReadOnlyRoots());
}

bool HeapObject::IsUniqueName() const {
  return IsInternalizedString() || IsSymbol();
}

bool HeapObject::IsFunction() const {
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  return map().instance_type() >= FIRST_FUNCTION_TYPE;
}

bool HeapObject::IsCallable() const { return map().is_callable(); }

bool HeapObject::IsConstructor() const { return map().is_constructor(); }

bool HeapObject::IsModuleInfo() const {
  return map() == GetReadOnlyRoots().module_info_map();
}

bool HeapObject::IsTemplateInfo() const {
  return IsObjectTemplateInfo() || IsFunctionTemplateInfo();
}

bool HeapObject::IsConsString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsCons();
}

bool HeapObject::IsThinString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsThin();
}

bool HeapObject::IsSlicedString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsSliced();
}

bool HeapObject::IsSeqString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsSequential();
}

bool HeapObject::IsSeqOneByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsSequential() &&
         String::cast(*this).IsOneByteRepresentation();
}

bool HeapObject::IsSeqTwoByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsSequential() &&
         String::cast(*this).IsTwoByteRepresentation();
}

bool HeapObject::IsExternalString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsExternal();
}

bool HeapObject::IsExternalOneByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsExternal() &&
         String::cast(*this).IsOneByteRepresentation();
}

bool HeapObject::IsExternalTwoByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(*this)).IsExternal() &&
         String::cast(*this).IsTwoByteRepresentation();
}

bool Object::IsNumber() const { return IsSmi() || IsHeapNumber(); }

bool Object::IsNumeric() const { return IsNumber() || IsBigInt(); }

bool HeapObject::IsFiller() const {
  InstanceType instance_type = map().instance_type();
  return instance_type == FREE_SPACE_TYPE || instance_type == FILLER_TYPE;
}

bool HeapObject::IsJSWeakCollection() const {
  return IsJSWeakMap() || IsJSWeakSet();
}

bool HeapObject::IsJSCollection() const { return IsJSMap() || IsJSSet(); }

bool HeapObject::IsPromiseReactionJobTask() const {
  return IsPromiseFulfillReactionJobTask() || IsPromiseRejectReactionJobTask();
}

bool HeapObject::IsFrameArray() const { return IsFixedArrayExact(); }

bool HeapObject::IsArrayList() const {
  return map() == GetReadOnlyRoots().array_list_map() ||
         *this == GetReadOnlyRoots().empty_fixed_array();
}

bool HeapObject::IsRegExpMatchInfo() const { return IsFixedArrayExact(); }

bool Object::IsLayoutDescriptor() const { return IsSmi() || IsByteArray(); }

bool HeapObject::IsDeoptimizationData() const {
  // Must be a fixed array.
  if (!IsFixedArrayExact()) return false;

  // There's no sure way to detect the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can
  // check that the length is zero or else the fixed size plus a multiple of
  // the entry size.
  int length = FixedArray::cast(*this).length();
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
  if (FixedArray::cast(*this).length() < 1) return false;
  return true;
}

bool HeapObject::IsDependentCode() const {
  if (!IsWeakFixedArray()) return false;
  // There's actually no way to see the difference between a weak fixed array
  // and a dependent codes array.
  return true;
}

bool HeapObject::IsAbstractCode() const {
  return IsBytecodeArray() || IsCode();
}

bool HeapObject::IsStringWrapper() const {
  return IsJSValue() && JSValue::cast(*this).value().IsString();
}

bool HeapObject::IsBooleanWrapper() const {
  return IsJSValue() && JSValue::cast(*this).value().IsBoolean();
}

bool HeapObject::IsScriptWrapper() const {
  return IsJSValue() && JSValue::cast(*this).value().IsScript();
}

bool HeapObject::IsNumberWrapper() const {
  return IsJSValue() && JSValue::cast(*this).value().IsNumber();
}

bool HeapObject::IsBigIntWrapper() const {
  return IsJSValue() && JSValue::cast(*this).value().IsBigInt();
}

bool HeapObject::IsSymbolWrapper() const {
  return IsJSValue() && JSValue::cast(*this).value().IsSymbol();
}

bool HeapObject::IsJSArrayBufferView() const {
  return IsJSDataView() || IsJSTypedArray();
}

bool HeapObject::IsStringSet() const { return IsHashTable(); }

bool HeapObject::IsObjectHashSet() const { return IsHashTable(); }

bool HeapObject::IsCompilationCacheTable() const { return IsHashTable(); }

bool HeapObject::IsMapCache() const { return IsHashTable(); }

bool HeapObject::IsObjectHashTable() const { return IsHashTable(); }

bool Object::IsHashTableBase() const { return IsHashTable(); }

bool Object::IsSmallOrderedHashTable() const {
  return IsSmallOrderedHashSet() || IsSmallOrderedHashMap() ||
         IsSmallOrderedNameDictionary();
}

bool Object::IsPrimitive() const {
  return IsSmi() || HeapObject::cast(*this).map().IsPrimitiveMap();
}

// static
Maybe<bool> Object::IsArray(Handle<Object> object) {
  if (object->IsSmi()) return Just(false);
  Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
  if (heap_object->IsJSArray()) return Just(true);
  if (!heap_object->IsJSProxy()) return Just(false);
  return JSProxy::IsArray(Handle<JSProxy>::cast(object));
}

bool HeapObject::IsUndetectable() const { return map().is_undetectable(); }

bool HeapObject::IsAccessCheckNeeded() const {
  if (IsJSGlobalProxy()) {
    const JSGlobalProxy proxy = JSGlobalProxy::cast(*this);
    JSGlobalObject global = proxy.GetIsolate()->context().global_object();
    return proxy.IsDetachedFrom(global);
  }
  return map().is_access_check_needed();
}

bool HeapObject::IsStruct() const {
  switch (map().instance_type()) {
#define MAKE_STRUCT_CASE(TYPE, Name, name) \
  case TYPE:                               \
    return true;
    STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    // It is hard to include ALLOCATION_SITE_TYPE in STRUCT_LIST because
    // that macro is used for many things and AllocationSite needs a few
    // special cases.
    case ALLOCATION_SITE_TYPE:
      return true;
    case LOAD_HANDLER_TYPE:
    case STORE_HANDLER_TYPE:
      return true;
    case FEEDBACK_CELL_TYPE:
      return true;
    case CALL_HANDLER_INFO_TYPE:
      return true;
    default:
      return false;
  }
}

#define MAKE_STRUCT_PREDICATE(NAME, Name, name)                  \
  bool Object::Is##Name() const {                                \
    return IsHeapObject() && HeapObject::cast(*this).Is##Name(); \
  }                                                              \
  TYPE_CHECKER(Name)
STRUCT_LIST(MAKE_STRUCT_PREDICATE)
#undef MAKE_STRUCT_PREDICATE

double Object::Number() const {
  DCHECK(IsNumber());
  return IsSmi() ? static_cast<double>(Smi(this->ptr()).value())
                 : HeapNumber::unchecked_cast(*this).value();
}

// static
bool Object::SameNumberValue(double value1, double value2) {
  // SameNumberValue(NaN, NaN) is true.
  if (value1 != value2) {
    return std::isnan(value1) && std::isnan(value2);
  }
  // SameNumberValue(0.0, -0.0) is false.
  return (std::signbit(value1) == std::signbit(value2));
}

bool Object::IsNaN() const {
  return this->IsHeapNumber() && std::isnan(HeapNumber::cast(*this).value());
}

bool Object::IsMinusZero() const {
  return this->IsHeapNumber() &&
         i::IsMinusZero(HeapNumber::cast(*this).value());
}

OBJECT_CONSTRUCTORS_IMPL(RegExpMatchInfo, FixedArray)
OBJECT_CONSTRUCTORS_IMPL(ScopeInfo, FixedArray)
OBJECT_CONSTRUCTORS_IMPL(BigIntBase, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(BigInt, BigIntBase)
OBJECT_CONSTRUCTORS_IMPL(FreshlyAllocatedBigInt, BigIntBase)

// ------------------------------------
// Cast operations

CAST_ACCESSOR(BigInt)
CAST_ACCESSOR(RegExpMatchInfo)
CAST_ACCESSOR(ScopeInfo)

bool Object::HasValidElements() {
  // Dictionary is covered under FixedArray. ByteArray is used
  // for the JSTypedArray backing stores.
  return IsFixedArray() || IsFixedDoubleArray() || IsByteArray();
}

bool Object::FilterKey(PropertyFilter filter) {
  DCHECK(!IsPropertyCell());
  if (filter == PRIVATE_NAMES_ONLY) {
    if (!IsSymbol()) return true;
    return !Symbol::cast(*this).is_private_name();
  } else if (IsSymbol()) {
    if (filter & SKIP_SYMBOLS) return true;

    if (Symbol::cast(*this).is_private()) return true;
  } else {
    if (filter & SKIP_STRINGS) return true;
  }
  return false;
}

Representation Object::OptimalRepresentation() {
  if (!FLAG_track_fields) return Representation::Tagged();
  if (IsSmi()) {
    return Representation::Smi();
  } else if (FLAG_track_double_fields && IsHeapNumber()) {
    return Representation::Double();
  } else if (FLAG_track_computed_fields && IsUninitialized()) {
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
    int num = Smi::ToInt(*this);
    if (num < 0) return false;
    *value = static_cast<uint32_t>(num);
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(*this).value();
    return DoubleToUint32IfEqualToSelf(num, value);
  }
  return false;
}

// static
MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object,
                                         const char* method_name) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  return ToObjectImpl(isolate, object, method_name);
}

// static
MaybeHandle<Name> Object::ToName(Isolate* isolate, Handle<Object> input) {
  if (input->IsName()) return Handle<Name>::cast(input);
  return ConvertToName(isolate, input);
}

// static
MaybeHandle<Object> Object::ToPropertyKey(Isolate* isolate,
                                          Handle<Object> value) {
  if (value->IsSmi() || HeapObject::cast(*value).IsName()) return value;
  return ConvertToPropertyKey(isolate, value);
}

// static
MaybeHandle<Object> Object::ToPrimitive(Handle<Object> input,
                                        ToPrimitiveHint hint) {
  if (input->IsPrimitive()) return input;
  return JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input), hint);
}

// static
MaybeHandle<Object> Object::ToNumber(Isolate* isolate, Handle<Object> input) {
  if (input->IsNumber()) return input;  // Shortcut.
  return ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber);
}

// static
MaybeHandle<Object> Object::ToNumeric(Isolate* isolate, Handle<Object> input) {
  if (input->IsNumber() || input->IsBigInt()) return input;  // Shortcut.
  return ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumeric);
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
  if (input->IsSmi()) return handle(Smi::cast(*input).ToUint32Smi(), isolate);
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
                                    MessageTemplate error_index) {
  if (input->IsSmi() && Smi::ToInt(*input) >= 0) return input;
  return ConvertToIndex(isolate, input, error_index);
}

MaybeHandle<Object> Object::GetProperty(Isolate* isolate, Handle<Object> object,
                                        Handle<Name> name) {
  LookupIterator it(isolate, object, name);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetProperty(&it);
}

MaybeHandle<Object> Object::GetElement(Isolate* isolate, Handle<Object> object,
                                       uint32_t index) {
  LookupIterator it(isolate, object, index);
  if (!it.IsFound()) return it.factory()->undefined_value();
  return GetProperty(&it);
}

MaybeHandle<Object> Object::SetElement(Isolate* isolate, Handle<Object> object,
                                       uint32_t index, Handle<Object> value,
                                       ShouldThrow should_throw) {
  LookupIterator it(isolate, object, index);
  MAYBE_RETURN_NULL(
      SetProperty(&it, value, StoreOrigin::kMaybeKeyed, Just(should_throw)));
  return value;
}

ObjectSlot HeapObject::RawField(int byte_offset) const {
  return ObjectSlot(FIELD_ADDR(*this, byte_offset));
}

MaybeObjectSlot HeapObject::RawMaybeWeakField(int byte_offset) const {
  return MaybeObjectSlot(FIELD_ADDR(*this, byte_offset));
}

MapWord MapWord::FromMap(const Map map) { return MapWord(map.ptr()); }

Map MapWord::ToMap() const { return Map::unchecked_cast(Object(value_)); }

bool MapWord::IsForwardingAddress() const { return HAS_SMI_TAG(value_); }

MapWord MapWord::FromForwardingAddress(HeapObject object) {
  return MapWord(object.ptr() - kHeapObjectTag);
}

HeapObject MapWord::ToForwardingAddress() {
  DCHECK(IsForwardingAddress());
  return HeapObject::FromAddress(value_);
}

#ifdef VERIFY_HEAP
void HeapObject::VerifyObjectField(Isolate* isolate, int offset) {
  VerifyPointer(isolate, READ_FIELD(*this, offset));
  STATIC_ASSERT(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObject::VerifyMaybeObjectField(Isolate* isolate, int offset) {
  MaybeObject::VerifyMaybeObjectPointer(isolate,
                                        READ_WEAK_FIELD(*this, offset));
  STATIC_ASSERT(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObject::VerifySmiField(int offset) {
  CHECK(READ_FIELD(*this, offset).IsSmi());
  STATIC_ASSERT(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

#endif

ReadOnlyRoots HeapObject::GetReadOnlyRoots() const {
  return ReadOnlyHeap::GetReadOnlyRoots(*this);
}

Map HeapObject::map() const { return map_word().ToMap(); }

void HeapObject::set_map(Map value) {
  if (!value.is_null()) {
#ifdef VERIFY_HEAP
    GetHeapFromWritableObject(*this)->VerifyObjectLayoutChange(*this, value);
#endif
  }
  set_map_word(MapWord::FromMap(value));
  if (!value.is_null()) {
    // TODO(1600) We are passing kNullAddress as a slot because maps can never
    // be on an evacuation candidate.
    MarkingBarrier(*this, ObjectSlot(kNullAddress), value);
  }
}

Map HeapObject::synchronized_map() const {
  return synchronized_map_word().ToMap();
}

void HeapObject::synchronized_set_map(Map value) {
  if (!value.is_null()) {
#ifdef VERIFY_HEAP
    GetHeapFromWritableObject(*this)->VerifyObjectLayoutChange(*this, value);
#endif
  }
  synchronized_set_map_word(MapWord::FromMap(value));
  if (!value.is_null()) {
    // TODO(1600) We are passing kNullAddress as a slot because maps can never
    // be on an evacuation candidate.
    MarkingBarrier(*this, ObjectSlot(kNullAddress), value);
  }
}

// Unsafe accessor omitting write barrier.
void HeapObject::set_map_no_write_barrier(Map value) {
  if (!value.is_null()) {
#ifdef VERIFY_HEAP
    GetHeapFromWritableObject(*this)->VerifyObjectLayoutChange(*this, value);
#endif
  }
  set_map_word(MapWord::FromMap(value));
}

void HeapObject::set_map_after_allocation(Map value, WriteBarrierMode mode) {
  set_map_word(MapWord::FromMap(value));
  if (mode != SKIP_WRITE_BARRIER) {
    DCHECK(!value.is_null());
    // TODO(1600) We are passing kNullAddress as a slot because maps can never
    // be on an evacuation candidate.
    MarkingBarrier(*this, ObjectSlot(kNullAddress), value);
  }
}

MapWordSlot HeapObject::map_slot() const {
  return MapWordSlot(FIELD_ADDR(*this, kMapOffset));
}

MapWord HeapObject::map_word() const {
  return MapWord(map_slot().Relaxed_Load().ptr());
}

void HeapObject::set_map_word(MapWord map_word) {
  map_slot().Relaxed_Store(Object(map_word.value_));
}

MapWord HeapObject::synchronized_map_word() const {
  return MapWord(map_slot().Acquire_Load().ptr());
}

void HeapObject::synchronized_set_map_word(MapWord map_word) {
  map_slot().Release_Store(Object(map_word.value_));
}

int HeapObject::Size() const { return SizeFromMap(map()); }

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

inline bool IsCustomElementsReceiverInstanceType(InstanceType instance_type) {
  return instance_type <= LAST_CUSTOM_ELEMENTS_RECEIVER;
}

// This should be in objects/map-inl.h, but can't, because of a cyclic
// dependency.
bool Map::IsCustomElementsReceiverMap() const {
  return IsCustomElementsReceiverInstanceType(instance_type());
}

bool Object::ToArrayLength(uint32_t* index) const {
  return Object::ToUint32(index);
}

bool Object::ToArrayIndex(uint32_t* index) const {
  return Object::ToUint32(index) && *index != kMaxUInt32;
}

int RegExpMatchInfo::NumberOfCaptureRegisters() {
  DCHECK_GE(length(), kLastMatchOverhead);
  Object obj = get(kNumberOfCapturesIndex);
  return Smi::ToInt(obj);
}

void RegExpMatchInfo::SetNumberOfCaptureRegisters(int value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kNumberOfCapturesIndex, Smi::FromInt(value));
}

String RegExpMatchInfo::LastSubject() {
  DCHECK_GE(length(), kLastMatchOverhead);
  return String::cast(get(kLastSubjectIndex));
}

void RegExpMatchInfo::SetLastSubject(String value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kLastSubjectIndex, value);
}

Object RegExpMatchInfo::LastInput() {
  DCHECK_GE(length(), kLastMatchOverhead);
  return get(kLastInputIndex);
}

void RegExpMatchInfo::SetLastInput(Object value) {
  DCHECK_GE(length(), kLastMatchOverhead);
  set(kLastInputIndex, value);
}

int RegExpMatchInfo::Capture(int i) {
  DCHECK_LT(i, NumberOfCaptureRegisters());
  Object obj = get(kFirstCaptureIndex + i);
  return Smi::ToInt(obj);
}

void RegExpMatchInfo::SetCapture(int i, int value) {
  DCHECK_LT(i, NumberOfCaptureRegisters());
  set(kFirstCaptureIndex + i, Smi::FromInt(value));
}

WriteBarrierMode HeapObject::GetWriteBarrierMode(
    const DisallowHeapAllocation& promise) {
  return GetWriteBarrierModeForObject(*this, &promise);
}

// static
AllocationAlignment HeapObject::RequiredAlignment(Map map) {
  // TODO(bmeurer, v8:4153): We should think about requiring double alignment
  // in general for ByteArray, since they are used as backing store for typed
  // arrays now.
#ifdef V8_COMPRESS_POINTERS
  // TODO(ishell, v8:8875): Consider using aligned allocations once the
  // allocation alignment inconsistency is fixed. For now we keep using
  // unaligned access since both x64 and arm64 architectures (where pointer
  // compression is supported) allow unaligned access to doubles and full words.
#endif  // V8_COMPRESS_POINTERS
#ifdef V8_HOST_ARCH_32_BIT
  int instance_type = map.instance_type();
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) return kDoubleAligned;
  if (instance_type == HEAP_NUMBER_TYPE) return kDoubleUnaligned;
#endif  // V8_HOST_ARCH_32_BIT
  return kWordAligned;
}

Address HeapObject::GetFieldAddress(int field_offset) const {
  return FIELD_ADDR(*this, field_offset);
}

// static
Maybe<bool> Object::GreaterThan(Isolate* isolate, Handle<Object> x,
                                Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
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
Maybe<bool> Object::GreaterThanOrEqual(Isolate* isolate, Handle<Object> x,
                                       Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
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
Maybe<bool> Object::LessThan(Isolate* isolate, Handle<Object> x,
                             Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
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
Maybe<bool> Object::LessThanOrEqual(Isolate* isolate, Handle<Object> x,
                                    Handle<Object> y) {
  Maybe<ComparisonResult> result = Compare(isolate, x, y);
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

MaybeHandle<Object> Object::GetPropertyOrElement(Isolate* isolate,
                                                 Handle<Object> object,
                                                 Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, object, name);
  return GetProperty(&it);
}

MaybeHandle<Object> Object::SetPropertyOrElement(
    Isolate* isolate, Handle<Object> object, Handle<Name> name,
    Handle<Object> value, Maybe<ShouldThrow> should_throw,
    StoreOrigin store_origin) {
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, object, name);
  MAYBE_RETURN_NULL(SetProperty(&it, value, store_origin, should_throw));
  return value;
}

MaybeHandle<Object> Object::GetPropertyOrElement(Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Handle<JSReceiver> holder) {
  LookupIterator it = LookupIterator::PropertyOrElement(holder->GetIsolate(),
                                                        receiver, name, holder);
  return GetProperty(&it);
}

// static
Object Object::GetSimpleHash(Object object) {
  DisallowHeapAllocation no_gc;
  if (object.IsSmi()) {
    uint32_t hash = ComputeUnseededHash(Smi::ToInt(object));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (object.IsHeapNumber()) {
    double num = HeapNumber::cast(object).value();
    if (std::isnan(num)) return Smi::FromInt(Smi::kMaxValue);
    // Use ComputeUnseededHash for all values in Signed32 range, including -0,
    // which is considered equal to 0 because collections use SameValueZero.
    uint32_t hash;
    // Check range before conversion to avoid undefined behavior.
    if (num >= kMinInt && num <= kMaxInt && FastI2D(FastD2I(num)) == num) {
      hash = ComputeUnseededHash(FastD2I(num));
    } else {
      hash = ComputeLongHash(double_to_uint64(num));
    }
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (object.IsName()) {
    uint32_t hash = Name::cast(object).Hash();
    return Smi::FromInt(hash);
  }
  if (object.IsOddball()) {
    uint32_t hash = Oddball::cast(object).to_string().Hash();
    return Smi::FromInt(hash);
  }
  if (object.IsBigInt()) {
    uint32_t hash = BigInt::cast(object).Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (object.IsSharedFunctionInfo()) {
    uint32_t hash = SharedFunctionInfo::cast(object).Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  DCHECK(object.IsJSReceiver());
  return object;
}

Object Object::GetHash() {
  DisallowHeapAllocation no_gc;
  Object hash = GetSimpleHash(*this);
  if (hash.IsSmi()) return hash;

  DCHECK(IsJSReceiver());
  JSReceiver receiver = JSReceiver::cast(*this);
  return receiver.GetIdentityHash();
}

Handle<Object> ObjectHashTableShape::AsHandle(Handle<Object> key) {
  return key;
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

// Predictably converts HeapObject or Address to uint32 by calculating
// offset of the address in respective MemoryChunk.
static inline uint32_t ObjectAddressForHashing(Address object) {
  uint32_t value = static_cast<uint32_t>(object);
  return value & kPageAlignmentMask;
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

bool ScopeInfo::IsAsmModule() const {
  return IsAsmModuleField::decode(Flags());
}

bool ScopeInfo::HasSimpleParameters() const {
  return HasSimpleParametersField::decode(Flags());
}

#define FIELD_ACCESSORS(name)                                                 \
  void ScopeInfo::Set##name(int value) { set(k##name, Smi::FromInt(value)); } \
  int ScopeInfo::name() const {                                               \
    if (length() > 0) {                                                       \
      return Smi::ToInt(get(k##name));                                        \
    } else {                                                                  \
      return 0;                                                               \
    }                                                                         \
  }
FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(FIELD_ACCESSORS)
#undef FIELD_ACCESSORS

FreshlyAllocatedBigInt FreshlyAllocatedBigInt::cast(Object object) {
  SLOW_DCHECK(object.IsBigInt());
  return FreshlyAllocatedBigInt(object.ptr());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_OBJECTS_INL_H_
