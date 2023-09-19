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

#include "src/base/bits.h"
#include "src/base/memory.h"
#include "src/base/numbers/double.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/heap-verifier.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/read-only-heap-inl.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-proxy-inl.h"  // TODO(jkummerow): Drop.
#include "src/objects/keys.h"
#include "src/objects/literal-objects.h"
#include "src/objects/lookup-inl.h"  // TODO(jkummerow): Drop.
#include "src/objects/objects.h"
#include "src/objects/oddball-inl.h"
#include "src/objects/property-details.h"
#include "src/objects/property.h"
#include "src/objects/regexp-match-info-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/objects/tagged-impl-inl.h"
#include "src/objects/tagged-index.h"
#include "src/objects/templates.h"
#include "src/roots/roots.h"
#include "src/sandbox/bounded-size-inl.h"
#include "src/sandbox/external-pointer-inl.h"
#include "src/sandbox/sandboxed-pointer-inl.h"

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
  DCHECK_EQ(location(), PropertyLocation::kField);
  return 1;
}

DEF_GETTER(HeapObject, IsClassBoilerplate, bool) {
  return IsFixedArrayExact(cage_base);
}

bool Object::IsTaggedIndex() const {
  return IsSmi() && TaggedIndex::IsValid(TaggedIndex(ptr()).value());
}

bool Object::InSharedHeap() const {
  return IsHeapObject() && HeapObject::cast(*this).InAnySharedSpace();
}

bool Object::InWritableSharedSpace() const {
  return IsHeapObject() && HeapObject::cast(*this).InWritableSharedSpace();
}

bool Object::IsJSObjectThatCanBeTrackedAsPrototype() const {
  return IsHeapObject() &&
         HeapObject::cast(*this).IsJSObjectThatCanBeTrackedAsPrototype();
}

#define IS_TYPE_FUNCTION_DEF(type_)                                        \
  bool Object::Is##type_() const {                                         \
    return IsHeapObject() && HeapObject::cast(*this).Is##type_();          \
  }                                                                        \
  bool Object::Is##type_(PtrComprCageBase cage_base) const {               \
    return IsHeapObject() && HeapObject::cast(*this).Is##type_(cage_base); \
  }
HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DEF)
IS_TYPE_FUNCTION_DEF(HashTableBase)
IS_TYPE_FUNCTION_DEF(SmallOrderedHashTable)
#undef IS_TYPE_FUNCTION_DEF

#define IS_TYPE_FUNCTION_DEF(Type, Value, _)                     \
  bool Object::Is##Type(Isolate* isolate) const {                \
    return Is##Type(ReadOnlyRoots(isolate));                     \
  }                                                              \
  bool Object::Is##Type(LocalIsolate* isolate) const {           \
    return Is##Type(ReadOnlyRoots(isolate));                     \
  }                                                              \
  bool Object::Is##Type() const {                                \
    return IsHeapObject() && HeapObject::cast(*this).Is##Type(); \
  }                                                              \
  bool HeapObject::Is##Type(Isolate* isolate) const {            \
    return Object::Is##Type(isolate);                            \
  }                                                              \
  bool HeapObject::Is##Type(LocalIsolate* isolate) const {       \
    return Object::Is##Type(isolate);                            \
  }                                                              \
  bool HeapObject::Is##Type(ReadOnlyRoots roots) const {         \
    return Object::Is##Type(roots);                              \
  }                                                              \
  bool HeapObject::Is##Type() const { return Is##Type(GetReadOnlyRoots()); }
ODDBALL_LIST(IS_TYPE_FUNCTION_DEF)
#undef IS_TYPE_FUNCTION_DEF

#if V8_STATIC_ROOTS_BOOL
#define IS_TYPE_FUNCTION_DEF(Type, Value, CamelName)                       \
  bool Object::Is##Type(ReadOnlyRoots roots) const {                       \
    SLOW_DCHECK(CheckObjectComparisonAllowed(ptr(), roots.Value().ptr())); \
    return V8HeapCompressionScheme::CompressObject(ptr()) ==               \
           StaticReadOnlyRoot::k##CamelName;                               \
  }
#else
#define IS_TYPE_FUNCTION_DEF(Type, Value, _)         \
  bool Object::Is##Type(ReadOnlyRoots roots) const { \
    return (*this) == roots.Value();                 \
  }
#endif
ODDBALL_LIST(IS_TYPE_FUNCTION_DEF)
#undef IS_TYPE_FUNCTION_DEF

bool Object::IsNullOrUndefined(Isolate* isolate) const {
  return IsNullOrUndefined(ReadOnlyRoots(isolate));
}

bool Object::IsNullOrUndefined(LocalIsolate* local_isolate) const {
  return IsNullOrUndefined(ReadOnlyRoots(local_isolate));
}

bool Object::IsNullOrUndefined(ReadOnlyRoots roots) const {
  return IsNull(roots) || IsUndefined(roots);
}

bool Object::IsNullOrUndefined() const {
  return IsHeapObject() && HeapObject::cast(*this).IsNullOrUndefined();
}

bool Object::IsZero() const { return *this == Smi::zero(); }

bool Object::IsPublicSymbol() const {
  return IsSymbol() && !Symbol::cast(*this).is_private();
}
bool Object::IsPrivateSymbol() const {
  return IsSymbol() && Symbol::cast(*this).is_private();
}

bool Object::IsNoSharedNameSentinel() const {
  return *this == SharedFunctionInfo::kNoSharedNameSentinel;
}

template <class T,
          typename std::enable_if<(std::is_arithmetic<T>::value ||
                                   std::is_enum<T>::value) &&
                                      !std::is_floating_point<T>::value,
                                  int>::type>
T Object::Relaxed_ReadField(size_t offset) const {
  // Pointer compression causes types larger than kTaggedSize to be
  // unaligned. Atomic loads must be aligned.
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  return static_cast<T>(base::AsAtomicImpl<AtomicT>::Relaxed_Load(
      reinterpret_cast<AtomicT*>(field_address(offset))));
}

template <class T,
          typename std::enable_if<(std::is_arithmetic<T>::value ||
                                   std::is_enum<T>::value) &&
                                      !std::is_floating_point<T>::value,
                                  int>::type>
void Object::Relaxed_WriteField(size_t offset, T value) {
  // Pointer compression causes types larger than kTaggedSize to be
  // unaligned. Atomic stores must be aligned.
  DCHECK_IMPLIES(COMPRESS_POINTERS_BOOL, sizeof(T) <= kTaggedSize);
  using AtomicT = typename base::AtomicTypeFromByteWidth<sizeof(T)>::type;
  base::AsAtomicImpl<AtomicT>::Relaxed_Store(
      reinterpret_cast<AtomicT*>(field_address(offset)),
      static_cast<AtomicT>(value));
}

bool HeapObject::InAnySharedSpace() const {
  if (IsReadOnlyHeapObject(*this)) return V8_SHARED_RO_HEAP_BOOL;
  return InWritableSharedSpace();
}

bool HeapObject::InWritableSharedSpace() const {
  return BasicMemoryChunk::FromHeapObject(*this)->InWritableSharedSpace();
}

bool HeapObject::InReadOnlySpace() const { return IsReadOnlyHeapObject(*this); }

bool HeapObject::IsJSObjectThatCanBeTrackedAsPrototype() const {
  // Do not optimize objects in the shared heap because it is not
  // threadsafe. Objects in the shared heap have fixed layouts and their maps
  // never change.
  return IsJSObject() && !InWritableSharedSpace();
}

bool HeapObject::IsNullOrUndefined(Isolate* isolate) const {
  return IsNullOrUndefined(ReadOnlyRoots(isolate));
}

bool HeapObject::IsNullOrUndefined(ReadOnlyRoots roots) const {
  return Object::IsNullOrUndefined(roots);
}

bool HeapObject::IsNullOrUndefined() const {
  return IsNullOrUndefined(GetReadOnlyRoots());
}

DEF_GETTER(HeapObject, IsUniqueName, bool) {
  return IsInternalizedString(cage_base) || IsSymbol(cage_base);
}

DEF_GETTER(HeapObject, IsFunction, bool) {
  return IsJSFunctionOrBoundFunctionOrWrappedFunction();
}

DEF_GETTER(HeapObject, IsCallable, bool) {
  return map(cage_base).is_callable();
}

DEF_GETTER(HeapObject, IsCallableJSProxy, bool) {
  return IsCallable(cage_base) && IsJSProxy(cage_base);
}

DEF_GETTER(HeapObject, IsCallableApiObject, bool) {
  InstanceType type = map(cage_base).instance_type();
  return IsCallable(cage_base) &&
         (type == JS_API_OBJECT_TYPE || type == JS_SPECIAL_API_OBJECT_TYPE);
}

DEF_GETTER(HeapObject, IsNonNullForeign, bool) {
  return IsForeign(cage_base) &&
         Foreign::cast(*this).foreign_address() != kNullAddress;
}

DEF_GETTER(HeapObject, IsConstructor, bool) {
  return map(cage_base).is_constructor();
}

DEF_GETTER(HeapObject, IsSourceTextModuleInfo, bool) {
  return map(cage_base) == GetReadOnlyRoots(cage_base).module_info_map();
}

DEF_GETTER(HeapObject, IsConsString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsCons();
}

DEF_GETTER(HeapObject, IsThinString, bool) {
  InstanceType type = map(cage_base).instance_type();
  return type == THIN_STRING_TYPE;
}

DEF_GETTER(HeapObject, IsSlicedString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsSliced();
}

DEF_GETTER(HeapObject, IsSeqString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsSequential();
}

DEF_GETTER(HeapObject, IsSeqOneByteString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsSequentialOneByte();
}

DEF_GETTER(HeapObject, IsSeqTwoByteString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsSequentialTwoByte();
}

DEF_GETTER(HeapObject, IsExternalOneByteString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsExternalOneByte();
}

DEF_GETTER(HeapObject, IsExternalTwoByteString, bool) {
  if (!IsString(cage_base)) return false;
  return StringShape(String::cast(*this).map(cage_base)).IsExternalTwoByte();
}

bool Object::IsNumber() const {
  if (IsSmi()) return true;
  HeapObject this_heap_object = HeapObject::cast(*this);
  PtrComprCageBase cage_base = GetPtrComprCageBase(this_heap_object);
  return this_heap_object.IsHeapNumber(cage_base);
}

bool Object::IsNumber(PtrComprCageBase cage_base) const {
  return IsSmi() || IsHeapNumber(cage_base);
}

bool Object::IsNumeric() const {
  if (IsSmi()) return true;
  HeapObject this_heap_object = HeapObject::cast(*this);
  PtrComprCageBase cage_base = GetPtrComprCageBase(this_heap_object);
  return this_heap_object.IsHeapNumber(cage_base) ||
         this_heap_object.IsBigInt(cage_base);
}

bool Object::IsNumeric(PtrComprCageBase cage_base) const {
  return IsNumber(cage_base) || IsBigInt(cage_base);
}

DEF_GETTER(HeapObject, IsTemplateLiteralObject, bool) {
  return IsJSArray(cage_base);
}

DEF_GETTER(HeapObject, IsArrayList, bool) {
  return map(cage_base) ==
         GetReadOnlyRoots(cage_base).unchecked_array_list_map();
}

DEF_GETTER(HeapObject, IsRegExpMatchInfo, bool) {
  return IsFixedArrayExact(cage_base);
}

DEF_GETTER(HeapObject, IsDeoptimizationData, bool) {
  // Must be a fixed array.
  if (!IsFixedArrayExact(cage_base)) return false;

  // There's no sure way to detect the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can
  // check that the length is zero or else the fixed size plus a multiple of
  // the entry size.
  int length = FixedArray::cast(*this).length();
  if (length == 0) return true;

  length -= DeoptimizationData::kFirstDeoptEntryIndex;
  return length >= 0 && length % DeoptimizationData::kDeoptEntrySize == 0;
}

DEF_GETTER(HeapObject, IsHandlerTable, bool) {
  return IsFixedArrayExact(cage_base);
}

DEF_GETTER(HeapObject, IsTemplateList, bool) {
  if (!IsFixedArrayExact(cage_base)) return false;
  if (FixedArray::cast(*this).length() < 1) return false;
  return true;
}

DEF_GETTER(HeapObject, IsDependentCode, bool) {
  return IsWeakArrayList(cage_base);
}

DEF_GETTER(HeapObject, IsOSROptimizedCodeCache, bool) {
  return IsWeakFixedArray(cage_base);
}

DEF_GETTER(HeapObject, IsStringWrapper, bool) {
  return IsJSPrimitiveWrapper(cage_base) &&
         JSPrimitiveWrapper::cast(*this).value().IsString(cage_base);
}

DEF_GETTER(HeapObject, IsBooleanWrapper, bool) {
  return IsJSPrimitiveWrapper(cage_base) &&
         JSPrimitiveWrapper::cast(*this).value().IsBoolean(cage_base);
}

DEF_GETTER(HeapObject, IsScriptWrapper, bool) {
  return IsJSPrimitiveWrapper(cage_base) &&
         JSPrimitiveWrapper::cast(*this).value().IsScript(cage_base);
}

DEF_GETTER(HeapObject, IsNumberWrapper, bool) {
  return IsJSPrimitiveWrapper(cage_base) &&
         JSPrimitiveWrapper::cast(*this).value().IsNumber(cage_base);
}

DEF_GETTER(HeapObject, IsBigIntWrapper, bool) {
  return IsJSPrimitiveWrapper(cage_base) &&
         JSPrimitiveWrapper::cast(*this).value().IsBigInt(cage_base);
}

DEF_GETTER(HeapObject, IsSymbolWrapper, bool) {
  return IsJSPrimitiveWrapper(cage_base) &&
         JSPrimitiveWrapper::cast(*this).value().IsSymbol(cage_base);
}

DEF_GETTER(HeapObject, IsStringSet, bool) { return IsHashTable(cage_base); }

DEF_GETTER(HeapObject, IsObjectHashSet, bool) { return IsHashTable(cage_base); }

DEF_GETTER(HeapObject, IsCompilationCacheTable, bool) {
  return IsHashTable(cage_base);
}

DEF_GETTER(HeapObject, IsMapCache, bool) { return IsHashTable(cage_base); }

DEF_GETTER(HeapObject, IsObjectHashTable, bool) {
  return IsHashTable(cage_base);
}

DEF_GETTER(HeapObject, IsObjectTwoHashTable, bool) {
  return IsHashTable(cage_base);
}

DEF_GETTER(HeapObject, IsHashTableBase, bool) { return IsHashTable(cage_base); }

bool Object::IsPrimitive() const {
  if (IsSmi()) return true;
  HeapObject this_heap_object = HeapObject::cast(*this);
  PtrComprCageBase cage_base = GetPtrComprCageBase(this_heap_object);
  return this_heap_object.map(cage_base).IsPrimitiveMap();
}

bool Object::IsPrimitive(PtrComprCageBase cage_base) const {
  return IsSmi() || HeapObject::cast(*this).map(cage_base).IsPrimitiveMap();
}

// static
Maybe<bool> Object::IsArray(Handle<Object> object) {
  if (object->IsSmi()) return Just(false);
  Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
  if (heap_object->IsJSArray()) return Just(true);
  if (!heap_object->IsJSProxy()) return Just(false);
  return JSProxy::IsArray(Handle<JSProxy>::cast(object));
}

DEF_GETTER(HeapObject, IsUndetectable, bool) {
  return map(cage_base).is_undetectable();
}

DEF_GETTER(HeapObject, IsAccessCheckNeeded, bool) {
  if (IsJSGlobalProxy(cage_base)) {
    const JSGlobalProxy proxy = JSGlobalProxy::cast(*this);
    JSGlobalObject global = proxy.GetIsolate()->context().global_object();
    return proxy.IsDetachedFrom(global);
  }
  return map(cage_base).is_access_check_needed();
}

#define MAKE_STRUCT_PREDICATE(NAME, Name, name)                           \
  bool Object::Is##Name() const {                                         \
    return IsHeapObject() && HeapObject::cast(*this).Is##Name();          \
  }                                                                       \
  bool Object::Is##Name(PtrComprCageBase cage_base) const {               \
    return IsHeapObject() && HeapObject::cast(*this).Is##Name(cage_base); \
  }
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

OBJECT_CONSTRUCTORS_IMPL(BigIntBase, PrimitiveHeapObject)
OBJECT_CONSTRUCTORS_IMPL(BigInt, BigIntBase)
OBJECT_CONSTRUCTORS_IMPL(FreshlyAllocatedBigInt, BigIntBase)

// ------------------------------------
// Cast operations

CAST_ACCESSOR(BigIntBase)
CAST_ACCESSOR(BigInt)

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

Representation Object::OptimalRepresentation(PtrComprCageBase cage_base) const {
  if (IsSmi()) {
    return Representation::Smi();
  }
  HeapObject heap_object = HeapObject::cast(*this);
  if (heap_object.IsHeapNumber(cage_base)) {
    return Representation::Double();
  } else if (heap_object.IsUninitialized(
                 heap_object.GetReadOnlyRoots(cage_base))) {
    return Representation::None();
  }
  return Representation::HeapObject();
}

ElementsKind Object::OptimalElementsKind(PtrComprCageBase cage_base) const {
  if (IsSmi()) return PACKED_SMI_ELEMENTS;
  if (IsNumber(cage_base)) return PACKED_DOUBLE_ELEMENTS;
  return PACKED_ELEMENTS;
}

bool Object::FitsRepresentation(Representation representation,
                                bool allow_coercion) const {
  if (representation.IsSmi()) {
    return IsSmi();
  } else if (representation.IsDouble()) {
    return allow_coercion ? IsNumber() : IsHeapNumber();
  } else if (representation.IsHeapObject()) {
    return IsHeapObject();
  } else if (representation.IsNone()) {
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
MaybeHandle<Object> Object::ToPrimitive(Isolate* isolate, Handle<Object> input,
                                        ToPrimitiveHint hint) {
  if (input->IsPrimitive()) return input;
  return JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(input),
                                 hint);
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

Address Object::ReadSandboxedPointerField(size_t offset,
                                          PtrComprCageBase cage_base) const {
  return i::ReadSandboxedPointerField(field_address(offset), cage_base);
}

void Object::WriteSandboxedPointerField(size_t offset,
                                        PtrComprCageBase cage_base,
                                        Address value) {
  i::WriteSandboxedPointerField(field_address(offset), cage_base, value);
}

void Object::WriteSandboxedPointerField(size_t offset, Isolate* isolate,
                                        Address value) {
  i::WriteSandboxedPointerField(field_address(offset),
                                PtrComprCageBase(isolate), value);
}

size_t Object::ReadBoundedSizeField(size_t offset) const {
  return i::ReadBoundedSizeField(field_address(offset));
}

void Object::WriteBoundedSizeField(size_t offset, size_t value) {
  i::WriteBoundedSizeField(field_address(offset), value);
}

template <ExternalPointerTag tag>
void Object::InitExternalPointerField(size_t offset, Isolate* isolate,
                                      Address value) {
  i::InitExternalPointerField<tag>(field_address(offset), isolate, value);
}

template <ExternalPointerTag tag>
Address Object::ReadExternalPointerField(size_t offset,
                                         Isolate* isolate) const {
  return i::ReadExternalPointerField<tag>(field_address(offset), isolate);
}

template <ExternalPointerTag tag>
void Object::WriteExternalPointerField(size_t offset, Isolate* isolate,
                                       Address value) {
  i::WriteExternalPointerField<tag>(field_address(offset), isolate, value);
}

ObjectSlot HeapObject::RawField(int byte_offset) const {
  return ObjectSlot(field_address(byte_offset));
}

MaybeObjectSlot HeapObject::RawMaybeWeakField(int byte_offset) const {
  return MaybeObjectSlot(field_address(byte_offset));
}

CodeObjectSlot HeapObject::RawCodeField(int byte_offset) const {
  return CodeObjectSlot(field_address(byte_offset));
}

ExternalPointerSlot HeapObject::RawExternalPointerField(int byte_offset) const {
  return ExternalPointerSlot(field_address(byte_offset));
}

MapWord MapWord::FromMap(const Map map) {
  DCHECK(map.is_null() || !MapWord::IsPacked(map.ptr()));
#ifdef V8_MAP_PACKING
  return MapWord(Pack(map.ptr()));
#else
  return MapWord(map.ptr());
#endif
}

Map MapWord::ToMap() const {
#ifdef V8_MAP_PACKING
  return Map::unchecked_cast(Object(Unpack(value_)));
#else
  return Map::unchecked_cast(Object(value_));
#endif
}

bool MapWord::IsForwardingAddress() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  // When external code space is enabled forwarding pointers are encoded as
  // Smi representing a diff from the source object address in kObjectAlignment
  // chunks.
  return HAS_SMI_TAG(value_);
#else
  return (value_ & kForwardingTagMask) == kForwardingTag;
#endif  // V8_EXTERNAL_CODE_SPACE
}

MapWord MapWord::FromForwardingAddress(HeapObject map_word_host,
                                       HeapObject object) {
#ifdef V8_EXTERNAL_CODE_SPACE
  // When external code space is enabled forwarding pointers are encoded as
  // Smi representing a diff from the source object address in kObjectAlignment
  // chunks.
  intptr_t diff = static_cast<intptr_t>(object.ptr() - map_word_host.ptr());
  DCHECK(IsAligned(diff, kObjectAlignment));
  MapWord map_word(Smi::FromIntptr(diff / kObjectAlignment).ptr());
  DCHECK(map_word.IsForwardingAddress());
  return map_word;
#else
  return MapWord(object.ptr() - kHeapObjectTag);
#endif  // V8_EXTERNAL_CODE_SPACE
}

HeapObject MapWord::ToForwardingAddress(HeapObject map_word_host) {
  DCHECK(IsForwardingAddress());
#ifdef V8_EXTERNAL_CODE_SPACE
  // When external code space is enabled forwarding pointers are encoded as
  // Smi representing a diff from the source object address in kObjectAlignment
  // chunks.
  intptr_t diff = static_cast<intptr_t>(Smi(value_).value()) * kObjectAlignment;
  Address address = map_word_host.address() + diff;
  return HeapObject::FromAddress(address);
#else
  return HeapObject::FromAddress(value_);
#endif  // V8_EXTERNAL_CODE_SPACE
}

#ifdef VERIFY_HEAP
void HeapObject::VerifyObjectField(Isolate* isolate, int offset) {
  VerifyPointer(isolate, TaggedField<Object>::load(isolate, *this, offset));
  static_assert(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObject::VerifyMaybeObjectField(Isolate* isolate, int offset) {
  MaybeObject::VerifyMaybeObjectPointer(
      isolate, TaggedField<MaybeObject>::load(isolate, *this, offset));
  static_assert(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

void HeapObject::VerifySmiField(int offset) {
  CHECK(TaggedField<Object>::load(*this, offset).IsSmi());
  static_assert(!COMPRESS_POINTERS_BOOL || kTaggedSize == kInt32Size);
}

#endif

ReadOnlyRoots HeapObject::EarlyGetReadOnlyRoots() const {
  return ReadOnlyHeap::EarlyGetReadOnlyRoots(*this);
}

ReadOnlyRoots HeapObject::GetReadOnlyRoots() const {
  return ReadOnlyHeap::GetReadOnlyRoots(*this);
}

// TODO(v8:13788): Remove this cage-ful accessor.
ReadOnlyRoots HeapObject::GetReadOnlyRoots(PtrComprCageBase cage_base) const {
  return GetReadOnlyRoots();
}

Map HeapObject::map() const {
  // This method is never used for objects located in code space
  // (InstructionStream and free space fillers) and thus it is fine to use
  // auto-computed cage base value.
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL, !IsCodeSpaceObject(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::map(cage_base);
}
Map HeapObject::map(PtrComprCageBase cage_base) const {
  return map_word(cage_base, kRelaxedLoad).ToMap();
}

void HeapObject::set_map(Map value) {
  set_map<EmitWriteBarrier::kYes>(value, kRelaxedStore,
                                  VerificationMode::kPotentialLayoutChange);
}

void HeapObject::set_map(Map value, ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kYes>(value, kReleaseStore,
                                  VerificationMode::kPotentialLayoutChange);
}

void HeapObject::set_map_safe_transition(Map value) {
  set_map<EmitWriteBarrier::kYes>(value, kRelaxedStore,
                                  VerificationMode::kSafeMapTransition);
}

void HeapObject::set_map_safe_transition(Map value, ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kYes>(value, kReleaseStore,
                                  VerificationMode::kSafeMapTransition);
}

void HeapObject::set_map_safe_transition_no_write_barrier(Map value,
                                                          RelaxedStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(value, kRelaxedStore,
                                 VerificationMode::kSafeMapTransition);
}

void HeapObject::set_map_safe_transition_no_write_barrier(Map value,
                                                          ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(value, kReleaseStore,
                                 VerificationMode::kSafeMapTransition);
}

// Unsafe accessor omitting write barrier.
void HeapObject::set_map_no_write_barrier(Map value, RelaxedStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(value, kRelaxedStore,
                                 VerificationMode::kPotentialLayoutChange);
}

void HeapObject::set_map_no_write_barrier(Map value, ReleaseStoreTag tag) {
  set_map<EmitWriteBarrier::kNo>(value, kReleaseStore,
                                 VerificationMode::kPotentialLayoutChange);
}

template <HeapObject::EmitWriteBarrier emit_write_barrier, typename MemoryOrder>
void HeapObject::set_map(Map value, MemoryOrder order, VerificationMode mode) {
#if V8_ENABLE_WEBASSEMBLY
  // In {WasmGraphBuilder::SetMap} and {WasmGraphBuilder::LoadMap}, we treat
  // maps as immutable. Therefore we are not allowed to mutate them here.
  DCHECK(!value.IsWasmStructMap() && !value.IsWasmArrayMap());
#endif
  // Object layout changes are currently not supported on background threads.
  // This method might change object layout and therefore can't be used on
  // background threads.
  DCHECK_IMPLIES(mode != VerificationMode::kSafeMapTransition,
                 !LocalHeap::Current());
  if (v8_flags.verify_heap && !value.is_null()) {
    Heap* heap = GetHeapFromWritableObject(*this);
    if (mode == VerificationMode::kSafeMapTransition) {
      HeapVerifier::VerifySafeMapTransition(heap, *this, value);
    } else {
      DCHECK_EQ(mode, VerificationMode::kPotentialLayoutChange);
      HeapVerifier::VerifyObjectLayoutChange(heap, *this, value);
    }
  }
  set_map_word(value, order);
  Heap::NotifyObjectLayoutChangeDone(*this);
#ifndef V8_DISABLE_WRITE_BARRIERS
  if (!value.is_null()) {
    if (emit_write_barrier == EmitWriteBarrier::kYes) {
      CombinedWriteBarrier(*this, map_slot(), value, UPDATE_WRITE_BARRIER);
    } else {
      DCHECK_EQ(emit_write_barrier, EmitWriteBarrier::kNo);
      SLOW_DCHECK(!WriteBarrier::IsRequired(*this, value));
    }
  }
#endif
}

void HeapObject::set_map_after_allocation(Map value, WriteBarrierMode mode) {
  set_map_word(value, kRelaxedStore);
#ifndef V8_DISABLE_WRITE_BARRIERS
  if (mode != SKIP_WRITE_BARRIER) {
    DCHECK(!value.is_null());
    CombinedWriteBarrier(*this, map_slot(), value, mode);
  } else {
    SLOW_DCHECK(!WriteBarrier::IsRequired(*this, value));
  }
#endif
}

DEF_ACQUIRE_GETTER(HeapObject, map, Map) {
  return map_word(cage_base, kAcquireLoad).ToMap();
}

ObjectSlot HeapObject::map_slot() const {
  return ObjectSlot(MapField::address(*this));
}

MapWord HeapObject::map_word(RelaxedLoadTag tag) const {
  // This method is never used for objects located in code space
  // (InstructionStream and free space fillers) and thus it is fine to use
  // auto-computed cage base value.
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL, !IsCodeSpaceObject(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::map_word(cage_base, tag);
}
MapWord HeapObject::map_word(PtrComprCageBase cage_base,
                             RelaxedLoadTag tag) const {
  return MapField::Relaxed_Load_Map_Word(cage_base, *this);
}

void HeapObject::set_map_word(Map map, RelaxedStoreTag) {
  MapField::Relaxed_Store_Map_Word(*this, MapWord::FromMap(map));
}

void HeapObject::set_map_word_forwarded(HeapObject target_object,
                                        RelaxedStoreTag) {
  MapField::Relaxed_Store_Map_Word(
      *this, MapWord::FromForwardingAddress(*this, target_object));
}

MapWord HeapObject::map_word(AcquireLoadTag tag) const {
  // This method is never used for objects located in code space
  // (InstructionStream and free space fillers) and thus it is fine to use
  // auto-computed cage base value.
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL, !IsCodeSpaceObject(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::map_word(cage_base, tag);
}
MapWord HeapObject::map_word(PtrComprCageBase cage_base,
                             AcquireLoadTag tag) const {
  return MapField::Acquire_Load_No_Unpack(cage_base, *this);
}

void HeapObject::set_map_word(Map map, ReleaseStoreTag) {
  MapField::Release_Store_Map_Word(*this, MapWord::FromMap(map));
}

void HeapObject::set_map_word_forwarded(HeapObject target_object,
                                        ReleaseStoreTag) {
  MapField::Release_Store_Map_Word(
      *this, MapWord::FromForwardingAddress(*this, target_object));
}

bool HeapObject::release_compare_and_swap_map_word_forwarded(
    MapWord old_map_word, HeapObject new_target_object) {
  Tagged_t result = MapField::Release_CompareAndSwap(
      *this, old_map_word,
      MapWord::FromForwardingAddress(*this, new_target_object));
  return result == static_cast<Tagged_t>(old_map_word.ptr());
}

// TODO(v8:11880): consider dropping parameterless version.
int HeapObject::Size() const {
  DCHECK_IMPLIES(V8_EXTERNAL_CODE_SPACE_BOOL, !IsCodeSpaceObject(*this));
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return HeapObject::Size(cage_base);
}
int HeapObject::Size(PtrComprCageBase cage_base) const {
  return SizeFromMap(map(cage_base));
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

bool Object::ToIntegerIndex(size_t* index) const {
  if (IsSmi()) {
    int num = Smi::ToInt(*this);
    if (num < 0) return false;
    *index = static_cast<size_t>(num);
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(*this).value();
    if (!(num >= 0)) return false;  // Negation to catch NaNs.
    constexpr double max =
        std::min(kMaxSafeInteger,
                 // The maximum size_t is reserved as "invalid" sentinel.
                 static_cast<double>(std::numeric_limits<size_t>::max() - 1));
    if (num > max) return false;
    size_t result = static_cast<size_t>(num);
    if (num != result) return false;  // Conversion lost fractional precision.
    *index = result;
    return true;
  }
  return false;
}

WriteBarrierMode HeapObject::GetWriteBarrierMode(
    const DisallowGarbageCollection& promise) {
  return GetWriteBarrierModeForObject(*this, &promise);
}

// static
AllocationAlignment HeapObject::RequiredAlignment(Map map) {
  // TODO(v8:4153): We should think about requiring double alignment
  // in general for ByteArray, since they are used as backing store for typed
  // arrays now.
  // TODO(ishell, v8:8875): Consider using aligned allocations for BigInt.
  if (USE_ALLOCATION_ALIGNMENT_BOOL) {
    int instance_type = map.instance_type();
    if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) return kDoubleAligned;
    if (instance_type == HEAP_NUMBER_TYPE) return kDoubleUnaligned;
  }
  return kTaggedAligned;
}

bool HeapObject::CheckRequiredAlignment(PtrComprCageBase cage_base) const {
  AllocationAlignment alignment = HeapObject::RequiredAlignment(map(cage_base));
  CHECK_EQ(0, Heap::GetFillToAlign(address(), alignment));
  return true;
}

Address HeapObject::GetFieldAddress(int field_offset) const {
  return field_address(field_offset);
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
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key);
  return GetProperty(&it);
}

MaybeHandle<Object> Object::SetPropertyOrElement(
    Isolate* isolate, Handle<Object> object, Handle<Name> name,
    Handle<Object> value, Maybe<ShouldThrow> should_throw,
    StoreOrigin store_origin) {
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, object, key);
  MAYBE_RETURN_NULL(SetProperty(&it, value, store_origin, should_throw));
  return value;
}

MaybeHandle<Object> Object::GetPropertyOrElement(Handle<Object> receiver,
                                                 Handle<Name> name,
                                                 Handle<JSReceiver> holder) {
  Isolate* isolate = holder->GetIsolate();
  PropertyKey key(isolate, name);
  LookupIterator it(isolate, receiver, key, holder);
  return GetProperty(&it);
}

// static
Object Object::GetSimpleHash(Object object) {
  DisallowGarbageCollection no_gc;
  if (object.IsSmi()) {
    uint32_t hash = ComputeUnseededHash(Smi::ToInt(object));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  auto instance_type = HeapObject::cast(object).map().instance_type();
  if (InstanceTypeChecker::IsHeapNumber(instance_type)) {
    double num = HeapNumber::cast(object).value();
    if (std::isnan(num)) return Smi::FromInt(Smi::kMaxValue);
    // Use ComputeUnseededHash for all values in Signed32 range, including -0,
    // which is considered equal to 0 because collections use SameValueZero.
    uint32_t hash;
    // Check range before conversion to avoid undefined behavior.
    if (num >= kMinInt && num <= kMaxInt && FastI2D(FastD2I(num)) == num) {
      hash = ComputeUnseededHash(FastD2I(num));
    } else {
      hash = ComputeLongHash(base::double_to_uint64(num));
    }
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsName(instance_type)) {
    uint32_t hash = Name::cast(object).EnsureHash();
    return Smi::FromInt(hash);
  } else if (InstanceTypeChecker::IsOddball(instance_type)) {
    uint32_t hash = Oddball::cast(object).to_string().EnsureHash();
    return Smi::FromInt(hash);
  } else if (InstanceTypeChecker::IsBigInt(instance_type)) {
    uint32_t hash = BigInt::cast(object).Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsSharedFunctionInfo(instance_type)) {
    uint32_t hash = SharedFunctionInfo::cast(object).Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsScopeInfo(instance_type)) {
    uint32_t hash = ScopeInfo::cast(object).Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  } else if (InstanceTypeChecker::IsScript(instance_type)) {
    int id = Script::cast(object).id();
    return Smi::FromInt(ComputeUnseededHash(id) & Smi::kMaxValue);
  }
  DCHECK(object.IsJSReceiver());
  return object;
}

Object Object::GetHash() {
  DisallowGarbageCollection no_gc;
  Object hash = GetSimpleHash(*this);
  if (hash.IsSmi()) return hash;

  DCHECK(IsJSReceiver());
  JSReceiver receiver = JSReceiver::cast(*this);
  return receiver.GetIdentityHash();
}

bool Object::IsShared() const {
  // This logic should be kept in sync with fast paths in
  // CodeStubAssembler::SharedValueBarrier.

  // Smis are trivially shared.
  if (IsSmi()) return true;

  HeapObject object = HeapObject::cast(*this);

  // RO objects are shared when the RO space is shared.
  if (IsReadOnlyHeapObject(object)) {
    return ReadOnlyHeap::IsReadOnlySpaceShared();
  }

  // Check if this object is already shared.
  InstanceType instance_type = object.map().instance_type();
  if (InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(instance_type)) {
    DCHECK(object.InAnySharedSpace());
    return true;
  }
  switch (instance_type) {
    case SHARED_STRING_TYPE:
    case SHARED_ONE_BYTE_STRING_TYPE:
    case SHARED_EXTERNAL_STRING_TYPE:
    case SHARED_EXTERNAL_ONE_BYTE_STRING_TYPE:
    case SHARED_UNCACHED_EXTERNAL_STRING_TYPE:
    case SHARED_UNCACHED_EXTERNAL_ONE_BYTE_STRING_TYPE:
      DCHECK(object.InAnySharedSpace());
      return true;
    case INTERNALIZED_STRING_TYPE:
    case ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case UNCACHED_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case UNCACHED_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
      if (v8_flags.shared_string_table) {
        DCHECK(object.InAnySharedSpace());
        return true;
      }
      return false;
    case HEAP_NUMBER_TYPE:
      return object.InWritableSharedSpace();
    default:
      return false;
  }
}

// static
MaybeHandle<Object> Object::Share(Isolate* isolate, Handle<Object> value,
                                  ShouldThrow throw_if_cannot_be_shared) {
  // Sharing values requires the RO space be shared.
  DCHECK(ReadOnlyHeap::IsReadOnlySpaceShared());
  if (value->IsShared()) return value;
  return ShareSlow(isolate, Handle<HeapObject>::cast(value),
                   throw_if_cannot_be_shared);
}

// https://tc39.es/proposal-symbols-as-weakmap-keys/#sec-canbeheldweakly-abstract-operation
bool Object::CanBeHeldWeakly() const {
  if (IsJSReceiver()) {
    // TODO(v8:12547) Shared structs and arrays should only be able to point
    // to shared values in weak collections. For now, disallow them as weak
    // collection keys.
    if (v8_flags.harmony_struct) {
      return !IsJSSharedStruct() && !IsJSSharedArray();
    }
    return true;
  }
  if (v8_flags.harmony_symbol_as_weakmap_key) {
    return IsSymbol() && !Symbol::cast(*this).is_in_public_symbol_table();
  }
  return false;
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

static inline Handle<Object> MakeEntryPair(Isolate* isolate, size_t index,
                                           Handle<Object> value) {
  Handle<Object> key = isolate->factory()->SizeToString(index);
  Handle<FixedArray> entry_storage = isolate->factory()->NewFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    PACKED_ELEMENTS, 2);
}

static inline Handle<Object> MakeEntryPair(Isolate* isolate, Handle<Object> key,
                                           Handle<Object> value) {
  Handle<FixedArray> entry_storage = isolate->factory()->NewFixedArray(2);
  {
    entry_storage->set(0, *key, SKIP_WRITE_BARRIER);
    entry_storage->set(1, *value, SKIP_WRITE_BARRIER);
  }
  return isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                    PACKED_ELEMENTS, 2);
}

FreshlyAllocatedBigInt FreshlyAllocatedBigInt::cast(Object object) {
  SLOW_DCHECK(object.IsBigInt());
  return FreshlyAllocatedBigInt(object.ptr());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_OBJECTS_INL_H_
