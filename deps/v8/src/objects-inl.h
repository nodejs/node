// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Review notes:
//
// - The use of macros in these inline functions may seem superfluous
// but it is absolutely needed to make sure gcc generates optimal
// code. gcc is not happy when attempting to inline too deep.
//

#ifndef V8_OBJECTS_INL_H_
#define V8_OBJECTS_INL_H_

#include "elements.h"
#include "objects.h"
#include "contexts.h"
#include "conversions-inl.h"
#include "heap.h"
#include "isolate.h"
#include "property.h"
#include "spaces.h"
#include "v8memory.h"

namespace v8 {
namespace internal {

PropertyDetails::PropertyDetails(Smi* smi) {
  value_ = smi->value();
}


Smi* PropertyDetails::AsSmi() {
  return Smi::FromInt(value_);
}


PropertyDetails PropertyDetails::AsDeleted() {
  Smi* smi = Smi::FromInt(value_ | DeletedField::encode(1));
  return PropertyDetails(smi);
}


#define CAST_ACCESSOR(type)                     \
  type* type::cast(Object* object) {            \
    ASSERT(object->Is##type());                 \
    return reinterpret_cast<type*>(object);     \
  }


#define INT_ACCESSORS(holder, name, offset)                             \
  int holder::name() { return READ_INT_FIELD(this, offset); }           \
  void holder::set_##name(int value) { WRITE_INT_FIELD(this, offset, value); }


#define ACCESSORS(holder, name, type, offset)                           \
  type* holder::name() { return type::cast(READ_FIELD(this, offset)); } \
  void holder::set_##name(type* value, WriteBarrierMode mode) {         \
    WRITE_FIELD(this, offset, value);                                   \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, mode);           \
  }


// GC-safe accessors do not use HeapObject::GetHeap(), but access TLS instead.
#define ACCESSORS_GCSAFE(holder, name, type, offset)                    \
  type* holder::name() { return type::cast(READ_FIELD(this, offset)); } \
  void holder::set_##name(type* value, WriteBarrierMode mode) {         \
    WRITE_FIELD(this, offset, value);                                   \
    CONDITIONAL_WRITE_BARRIER(HEAP, this, offset, mode);                \
  }


#define SMI_ACCESSORS(holder, name, offset)             \
  int holder::name() {                                  \
    Object* value = READ_FIELD(this, offset);           \
    return Smi::cast(value)->value();                   \
  }                                                     \
  void holder::set_##name(int value) {                  \
    WRITE_FIELD(this, offset, Smi::FromInt(value));     \
  }


#define BOOL_GETTER(holder, field, name, offset)           \
  bool holder::name() {                                    \
    return BooleanBit::get(field(), offset);               \
  }                                                        \


#define BOOL_ACCESSORS(holder, field, name, offset)        \
  bool holder::name() {                                    \
    return BooleanBit::get(field(), offset);               \
  }                                                        \
  void holder::set_##name(bool value) {                    \
    set_##field(BooleanBit::set(field(), offset, value));  \
  }


bool Object::IsInstanceOf(FunctionTemplateInfo* expected) {
  // There is a constraint on the object; check.
  if (!this->IsJSObject()) return false;
  // Fetch the constructor function of the object.
  Object* cons_obj = JSObject::cast(this)->map()->constructor();
  if (!cons_obj->IsJSFunction()) return false;
  JSFunction* fun = JSFunction::cast(cons_obj);
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  for (Object* type = fun->shared()->function_data();
       type->IsFunctionTemplateInfo();
       type = FunctionTemplateInfo::cast(type)->parent_template()) {
    if (type == expected) return true;
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}


bool Object::IsSmi() {
  return HAS_SMI_TAG(this);
}


bool Object::IsHeapObject() {
  return Internals::HasHeapObjectTag(this);
}


bool Object::IsHeapNumber() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() == HEAP_NUMBER_TYPE;
}


bool Object::IsString() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() < FIRST_NONSTRING_TYPE;
}


bool Object::IsSpecObject() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() >= FIRST_SPEC_OBJECT_TYPE;
}


bool Object::IsSymbol() {
  if (!this->IsHeapObject()) return false;
  uint32_t type = HeapObject::cast(this)->map()->instance_type();
  // Because the symbol tag is non-zero and no non-string types have the
  // symbol bit set we can test for symbols with a very simple test
  // operation.
  STATIC_ASSERT(kSymbolTag != 0);
  ASSERT(kNotStringTag + kIsSymbolMask > LAST_TYPE);
  return (type & kIsSymbolMask) != 0;
}


bool Object::IsConsString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsCons();
}


bool Object::IsSlicedString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSliced();
}


bool Object::IsSeqString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential();
}


bool Object::IsSeqAsciiString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsAsciiRepresentation();
}


bool Object::IsSeqTwoByteString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsTwoByteRepresentation();
}


bool Object::IsExternalString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal();
}


bool Object::IsExternalAsciiString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsAsciiRepresentation();
}


bool Object::IsExternalTwoByteString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsTwoByteRepresentation();
}

bool Object::HasValidElements() {
  // Dictionary is covered under FixedArray.
  return IsFixedArray() || IsFixedDoubleArray() || IsExternalArray();
}

StringShape::StringShape(String* str)
  : type_(str->map()->instance_type()) {
  set_valid();
  ASSERT((type_ & kIsNotStringMask) == kStringTag);
}


StringShape::StringShape(Map* map)
  : type_(map->instance_type()) {
  set_valid();
  ASSERT((type_ & kIsNotStringMask) == kStringTag);
}


StringShape::StringShape(InstanceType t)
  : type_(static_cast<uint32_t>(t)) {
  set_valid();
  ASSERT((type_ & kIsNotStringMask) == kStringTag);
}


bool StringShape::IsSymbol() {
  ASSERT(valid());
  STATIC_ASSERT(kSymbolTag != 0);
  return (type_ & kIsSymbolMask) != 0;
}


bool String::IsAsciiRepresentation() {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kAsciiStringTag;
}


bool String::IsTwoByteRepresentation() {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}


bool String::IsAsciiRepresentationUnderneath() {
  uint32_t type = map()->instance_type();
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
  ASSERT(IsFlat());
  switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
    case kAsciiStringTag:
      return true;
    case kTwoByteStringTag:
      return false;
    default:  // Cons or sliced string.  Need to go deeper.
      return GetUnderlying()->IsAsciiRepresentation();
  }
}


bool String::IsTwoByteRepresentationUnderneath() {
  uint32_t type = map()->instance_type();
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
  ASSERT(IsFlat());
  switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
    case kAsciiStringTag:
      return false;
    case kTwoByteStringTag:
      return true;
    default:  // Cons or sliced string.  Need to go deeper.
      return GetUnderlying()->IsTwoByteRepresentation();
  }
}


bool String::HasOnlyAsciiChars() {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kAsciiStringTag ||
         (type & kAsciiDataHintMask) == kAsciiDataHintTag;
}


bool StringShape::IsCons() {
  return (type_ & kStringRepresentationMask) == kConsStringTag;
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


uint32_t StringShape::encoding_tag() {
  return type_ & kStringEncodingMask;
}


uint32_t StringShape::full_representation_tag() {
  return (type_ & (kStringRepresentationMask | kStringEncodingMask));
}


STATIC_CHECK((kStringRepresentationMask | kStringEncodingMask) ==
             Internals::kFullStringRepresentationMask);


bool StringShape::IsSequentialAscii() {
  return full_representation_tag() == (kSeqStringTag | kAsciiStringTag);
}


bool StringShape::IsSequentialTwoByte() {
  return full_representation_tag() == (kSeqStringTag | kTwoByteStringTag);
}


bool StringShape::IsExternalAscii() {
  return full_representation_tag() == (kExternalStringTag | kAsciiStringTag);
}


bool StringShape::IsExternalTwoByte() {
  return full_representation_tag() == (kExternalStringTag | kTwoByteStringTag);
}


STATIC_CHECK((kExternalStringTag | kTwoByteStringTag) ==
             Internals::kExternalTwoByteRepresentationTag);


uc32 FlatStringReader::Get(int index) {
  ASSERT(0 <= index && index <= length_);
  if (is_ascii_) {
    return static_cast<const byte*>(start_)[index];
  } else {
    return static_cast<const uc16*>(start_)[index];
  }
}


bool Object::IsNumber() {
  return IsSmi() || IsHeapNumber();
}


bool Object::IsByteArray() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() == BYTE_ARRAY_TYPE;
}


bool Object::IsExternalPixelArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
          EXTERNAL_PIXEL_ARRAY_TYPE;
}


bool Object::IsExternalArray() {
  if (!Object::IsHeapObject())
    return false;
  InstanceType instance_type =
      HeapObject::cast(this)->map()->instance_type();
  return (instance_type >= FIRST_EXTERNAL_ARRAY_TYPE &&
          instance_type <= LAST_EXTERNAL_ARRAY_TYPE);
}


bool Object::IsExternalByteArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_BYTE_ARRAY_TYPE;
}


bool Object::IsExternalUnsignedByteArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE;
}


bool Object::IsExternalShortArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_SHORT_ARRAY_TYPE;
}


bool Object::IsExternalUnsignedShortArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE;
}


bool Object::IsExternalIntArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_INT_ARRAY_TYPE;
}


bool Object::IsExternalUnsignedIntArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_UNSIGNED_INT_ARRAY_TYPE;
}


bool Object::IsExternalFloatArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_FLOAT_ARRAY_TYPE;
}


bool Object::IsExternalDoubleArray() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() ==
      EXTERNAL_DOUBLE_ARRAY_TYPE;
}


bool MaybeObject::IsFailure() {
  return HAS_FAILURE_TAG(this);
}


bool MaybeObject::IsRetryAfterGC() {
  return HAS_FAILURE_TAG(this)
    && Failure::cast(this)->type() == Failure::RETRY_AFTER_GC;
}


bool MaybeObject::IsOutOfMemory() {
  return HAS_FAILURE_TAG(this)
      && Failure::cast(this)->IsOutOfMemoryException();
}


bool MaybeObject::IsException() {
  return this == Failure::Exception();
}


bool MaybeObject::IsTheHole() {
  return !IsFailure() && ToObjectUnchecked()->IsTheHole();
}


Failure* Failure::cast(MaybeObject* obj) {
  ASSERT(HAS_FAILURE_TAG(obj));
  return reinterpret_cast<Failure*>(obj);
}


bool Object::IsJSReceiver() {
  return IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() >= FIRST_JS_RECEIVER_TYPE;
}


bool Object::IsJSObject() {
  return IsJSReceiver() && !IsJSProxy();
}


bool Object::IsJSProxy() {
  return Object::IsHeapObject() &&
     (HeapObject::cast(this)->map()->instance_type() == JS_PROXY_TYPE ||
      HeapObject::cast(this)->map()->instance_type() == JS_FUNCTION_PROXY_TYPE);
}


bool Object::IsJSFunctionProxy() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() == JS_FUNCTION_PROXY_TYPE;
}


bool Object::IsJSWeakMap() {
  return Object::IsJSObject() &&
      HeapObject::cast(this)->map()->instance_type() == JS_WEAK_MAP_TYPE;
}


bool Object::IsJSContextExtensionObject() {
  return IsHeapObject()
      && (HeapObject::cast(this)->map()->instance_type() ==
          JS_CONTEXT_EXTENSION_OBJECT_TYPE);
}


bool Object::IsMap() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == MAP_TYPE;
}


bool Object::IsFixedArray() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == FIXED_ARRAY_TYPE;
}


bool Object::IsFixedDoubleArray() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() ==
          FIXED_DOUBLE_ARRAY_TYPE;
}


bool Object::IsDescriptorArray() {
  return IsFixedArray();
}


bool Object::IsDeoptimizationInputData() {
  // Must be a fixed array.
  if (!IsFixedArray()) return false;

  // There's no sure way to detect the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can
  // check that the length is zero or else the fixed size plus a multiple of
  // the entry size.
  int length = FixedArray::cast(this)->length();
  if (length == 0) return true;

  length -= DeoptimizationInputData::kFirstDeoptEntryIndex;
  return length >= 0 &&
      length % DeoptimizationInputData::kDeoptEntrySize == 0;
}


bool Object::IsDeoptimizationOutputData() {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can check
  // that the length is plausible though.
  if (FixedArray::cast(this)->length() % 2 != 0) return false;
  return true;
}


bool Object::IsContext() {
  if (Object::IsHeapObject()) {
    Map* map = HeapObject::cast(this)->map();
    Heap* heap = map->GetHeap();
    return (map == heap->function_context_map() ||
            map == heap->catch_context_map() ||
            map == heap->with_context_map() ||
            map == heap->global_context_map() ||
            map == heap->block_context_map());
  }
  return false;
}


bool Object::IsGlobalContext() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->global_context_map();
}


bool Object::IsSerializedScopeInfo() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->serialized_scope_info_map();
}


bool Object::IsJSFunction() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == JS_FUNCTION_TYPE;
}


template <> inline bool Is<JSFunction>(Object* obj) {
  return obj->IsJSFunction();
}


bool Object::IsCode() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == CODE_TYPE;
}


bool Object::IsOddball() {
  ASSERT(HEAP->is_safe_to_read_maps());
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() == ODDBALL_TYPE;
}


bool Object::IsJSGlobalPropertyCell() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type()
      == JS_GLOBAL_PROPERTY_CELL_TYPE;
}


bool Object::IsSharedFunctionInfo() {
  return Object::IsHeapObject() &&
      (HeapObject::cast(this)->map()->instance_type() ==
       SHARED_FUNCTION_INFO_TYPE);
}


bool Object::IsJSValue() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == JS_VALUE_TYPE;
}


bool Object::IsJSMessageObject() {
  return Object::IsHeapObject()
      && (HeapObject::cast(this)->map()->instance_type() ==
          JS_MESSAGE_OBJECT_TYPE);
}


bool Object::IsStringWrapper() {
  return IsJSValue() && JSValue::cast(this)->value()->IsString();
}


bool Object::IsForeign() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == FOREIGN_TYPE;
}


bool Object::IsBoolean() {
  return IsOddball() &&
      ((Oddball::cast(this)->kind() & Oddball::kNotBooleanMask) == 0);
}


bool Object::IsJSArray() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == JS_ARRAY_TYPE;
}


bool Object::IsJSRegExp() {
  return Object::IsHeapObject()
      && HeapObject::cast(this)->map()->instance_type() == JS_REGEXP_TYPE;
}


template <> inline bool Is<JSArray>(Object* obj) {
  return obj->IsJSArray();
}


bool Object::IsHashTable() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->hash_table_map();
}


bool Object::IsDictionary() {
  return IsHashTable() &&
      this != HeapObject::cast(this)->GetHeap()->symbol_table();
}


bool Object::IsSymbolTable() {
  return IsHashTable() && this ==
         HeapObject::cast(this)->GetHeap()->raw_unchecked_symbol_table();
}


bool Object::IsJSFunctionResultCache() {
  if (!IsFixedArray()) return false;
  FixedArray* self = FixedArray::cast(this);
  int length = self->length();
  if (length < JSFunctionResultCache::kEntriesIndex) return false;
  if ((length - JSFunctionResultCache::kEntriesIndex)
      % JSFunctionResultCache::kEntrySize != 0) {
    return false;
  }
#ifdef DEBUG
  reinterpret_cast<JSFunctionResultCache*>(this)->JSFunctionResultCacheVerify();
#endif
  return true;
}


bool Object::IsNormalizedMapCache() {
  if (!IsFixedArray()) return false;
  if (FixedArray::cast(this)->length() != NormalizedMapCache::kEntries) {
    return false;
  }
#ifdef DEBUG
  reinterpret_cast<NormalizedMapCache*>(this)->NormalizedMapCacheVerify();
#endif
  return true;
}


bool Object::IsCompilationCacheTable() {
  return IsHashTable();
}


bool Object::IsCodeCacheHashTable() {
  return IsHashTable();
}


bool Object::IsPolymorphicCodeCacheHashTable() {
  return IsHashTable();
}


bool Object::IsMapCache() {
  return IsHashTable();
}


bool Object::IsPrimitive() {
  return IsOddball() || IsNumber() || IsString();
}


bool Object::IsJSGlobalProxy() {
  bool result = IsHeapObject() &&
                (HeapObject::cast(this)->map()->instance_type() ==
                 JS_GLOBAL_PROXY_TYPE);
  ASSERT(!result || IsAccessCheckNeeded());
  return result;
}


bool Object::IsGlobalObject() {
  if (!IsHeapObject()) return false;

  InstanceType type = HeapObject::cast(this)->map()->instance_type();
  return type == JS_GLOBAL_OBJECT_TYPE ||
         type == JS_BUILTINS_OBJECT_TYPE;
}


bool Object::IsJSGlobalObject() {
  return IsHeapObject() &&
      (HeapObject::cast(this)->map()->instance_type() ==
       JS_GLOBAL_OBJECT_TYPE);
}


bool Object::IsJSBuiltinsObject() {
  return IsHeapObject() &&
      (HeapObject::cast(this)->map()->instance_type() ==
       JS_BUILTINS_OBJECT_TYPE);
}


bool Object::IsUndetectableObject() {
  return IsHeapObject()
    && HeapObject::cast(this)->map()->is_undetectable();
}


bool Object::IsAccessCheckNeeded() {
  return IsHeapObject()
    && HeapObject::cast(this)->map()->is_access_check_needed();
}


bool Object::IsStruct() {
  if (!IsHeapObject()) return false;
  switch (HeapObject::cast(this)->map()->instance_type()) {
#define MAKE_STRUCT_CASE(NAME, Name, name) case NAME##_TYPE: return true;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    default: return false;
  }
}


#define MAKE_STRUCT_PREDICATE(NAME, Name, name)                  \
  bool Object::Is##Name() {                                      \
    return Object::IsHeapObject()                                \
      && HeapObject::cast(this)->map()->instance_type() == NAME##_TYPE; \
  }
  STRUCT_LIST(MAKE_STRUCT_PREDICATE)
#undef MAKE_STRUCT_PREDICATE


bool Object::IsUndefined() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kUndefined;
}


bool Object::IsNull() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kNull;
}


bool Object::IsTheHole() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kTheHole;
}


bool Object::IsTrue() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kTrue;
}


bool Object::IsFalse() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kFalse;
}


bool Object::IsArgumentsMarker() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kArgumentMarker;
}


double Object::Number() {
  ASSERT(IsNumber());
  return IsSmi()
    ? static_cast<double>(reinterpret_cast<Smi*>(this)->value())
    : reinterpret_cast<HeapNumber*>(this)->value();
}


MaybeObject* Object::ToSmi() {
  if (IsSmi()) return this;
  if (IsHeapNumber()) {
    double value = HeapNumber::cast(this)->value();
    int int_value = FastD2I(value);
    if (value == FastI2D(int_value) && Smi::IsValid(int_value)) {
      return Smi::FromInt(int_value);
    }
  }
  return Failure::Exception();
}


bool Object::HasSpecificClassOf(String* name) {
  return this->IsJSObject() && (JSObject::cast(this)->class_name() == name);
}


MaybeObject* Object::GetElement(uint32_t index) {
  // GetElement can trigger a getter which can cause allocation.
  // This was not always the case. This ASSERT is here to catch
  // leftover incorrect uses.
  ASSERT(HEAP->IsAllocationAllowed());
  return GetElementWithReceiver(this, index);
}


Object* Object::GetElementNoExceptionThrown(uint32_t index) {
  MaybeObject* maybe = GetElementWithReceiver(this, index);
  ASSERT(!maybe->IsFailure());
  Object* result = NULL;  // Initialization to please compiler.
  maybe->ToObject(&result);
  return result;
}


MaybeObject* Object::GetProperty(String* key) {
  PropertyAttributes attributes;
  return GetPropertyWithReceiver(this, key, &attributes);
}


MaybeObject* Object::GetProperty(String* key, PropertyAttributes* attributes) {
  return GetPropertyWithReceiver(this, key, attributes);
}


#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<byte*>(p) + offset - kHeapObjectTag)

#define READ_FIELD(p, offset) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)))

#define WRITE_FIELD(p, offset, value) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)) = value)

// TODO(isolates): Pass heap in to these macros.
#define WRITE_BARRIER(object, offset) \
  object->GetHeap()->RecordWrite(object->address(), offset);

// CONDITIONAL_WRITE_BARRIER must be issued after the actual
// write due to the assert validating the written value.
#define CONDITIONAL_WRITE_BARRIER(heap, object, offset, mode) \
  if (mode == UPDATE_WRITE_BARRIER) { \
    heap->RecordWrite(object->address(), offset); \
  } else { \
    ASSERT(mode == SKIP_WRITE_BARRIER); \
    ASSERT(heap->InNewSpace(object) || \
           !heap->InNewSpace(READ_FIELD(object, offset)) || \
           Page::FromAddress(object->address())->           \
               IsRegionDirty(object->address() + offset));  \
  }

#ifndef V8_TARGET_ARCH_MIPS
  #define READ_DOUBLE_FIELD(p, offset) \
    (*reinterpret_cast<double*>(FIELD_ADDR(p, offset)))
#else  // V8_TARGET_ARCH_MIPS
  // Prevent gcc from using load-double (mips ldc1) on (possibly)
  // non-64-bit aligned HeapNumber::value.
  static inline double read_double_field(void* p, int offset) {
    union conversion {
      double d;
      uint32_t u[2];
    } c;
    c.u[0] = (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset)));
    c.u[1] = (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset + 4)));
    return c.d;
  }
  #define READ_DOUBLE_FIELD(p, offset) read_double_field(p, offset)
#endif  // V8_TARGET_ARCH_MIPS


#ifndef V8_TARGET_ARCH_MIPS
  #define WRITE_DOUBLE_FIELD(p, offset, value) \
    (*reinterpret_cast<double*>(FIELD_ADDR(p, offset)) = value)
#else  // V8_TARGET_ARCH_MIPS
  // Prevent gcc from using store-double (mips sdc1) on (possibly)
  // non-64-bit aligned HeapNumber::value.
  static inline void write_double_field(void* p, int offset,
                                        double value) {
    union conversion {
      double d;
      uint32_t u[2];
    } c;
    c.d = value;
    (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset))) = c.u[0];
    (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset + 4))) = c.u[1];
  }
  #define WRITE_DOUBLE_FIELD(p, offset, value) \
    write_double_field(p, offset, value)
#endif  // V8_TARGET_ARCH_MIPS


#define READ_INT_FIELD(p, offset) \
  (*reinterpret_cast<int*>(FIELD_ADDR(p, offset)))

#define WRITE_INT_FIELD(p, offset, value) \
  (*reinterpret_cast<int*>(FIELD_ADDR(p, offset)) = value)

#define READ_INTPTR_FIELD(p, offset) \
  (*reinterpret_cast<intptr_t*>(FIELD_ADDR(p, offset)))

#define WRITE_INTPTR_FIELD(p, offset, value) \
  (*reinterpret_cast<intptr_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_UINT32_FIELD(p, offset) \
  (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset)))

#define WRITE_UINT32_FIELD(p, offset, value) \
  (*reinterpret_cast<uint32_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_SHORT_FIELD(p, offset) \
  (*reinterpret_cast<uint16_t*>(FIELD_ADDR(p, offset)))

#define WRITE_SHORT_FIELD(p, offset, value) \
  (*reinterpret_cast<uint16_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_BYTE_FIELD(p, offset) \
  (*reinterpret_cast<byte*>(FIELD_ADDR(p, offset)))

#define WRITE_BYTE_FIELD(p, offset, value) \
  (*reinterpret_cast<byte*>(FIELD_ADDR(p, offset)) = value)


Object** HeapObject::RawField(HeapObject* obj, int byte_offset) {
  return &READ_FIELD(obj, byte_offset);
}


int Smi::value() {
  return Internals::SmiValue(this);
}


Smi* Smi::FromInt(int value) {
  ASSERT(Smi::IsValid(value));
  int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
  intptr_t tagged_value =
      (static_cast<intptr_t>(value) << smi_shift_bits) | kSmiTag;
  return reinterpret_cast<Smi*>(tagged_value);
}


Smi* Smi::FromIntptr(intptr_t value) {
  ASSERT(Smi::IsValid(value));
  int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
  return reinterpret_cast<Smi*>((value << smi_shift_bits) | kSmiTag);
}


Failure::Type Failure::type() const {
  return static_cast<Type>(value() & kFailureTypeTagMask);
}


bool Failure::IsInternalError() const {
  return type() == INTERNAL_ERROR;
}


bool Failure::IsOutOfMemoryException() const {
  return type() == OUT_OF_MEMORY_EXCEPTION;
}


AllocationSpace Failure::allocation_space() const {
  ASSERT_EQ(RETRY_AFTER_GC, type());
  return static_cast<AllocationSpace>((value() >> kFailureTypeTagSize)
                                      & kSpaceTagMask);
}


Failure* Failure::InternalError() {
  return Construct(INTERNAL_ERROR);
}


Failure* Failure::Exception() {
  return Construct(EXCEPTION);
}


Failure* Failure::OutOfMemoryException() {
  return Construct(OUT_OF_MEMORY_EXCEPTION);
}


intptr_t Failure::value() const {
  return static_cast<intptr_t>(
      reinterpret_cast<uintptr_t>(this) >> kFailureTagSize);
}


Failure* Failure::RetryAfterGC() {
  return RetryAfterGC(NEW_SPACE);
}


Failure* Failure::RetryAfterGC(AllocationSpace space) {
  ASSERT((space & ~kSpaceTagMask) == 0);
  return Construct(RETRY_AFTER_GC, space);
}


Failure* Failure::Construct(Type type, intptr_t value) {
  uintptr_t info =
      (static_cast<uintptr_t>(value) << kFailureTypeTagSize) | type;
  ASSERT(((info << kFailureTagSize) >> kFailureTagSize) == info);
  return reinterpret_cast<Failure*>((info << kFailureTagSize) | kFailureTag);
}


bool Smi::IsValid(intptr_t value) {
#ifdef DEBUG
  bool in_range = (value >= kMinValue) && (value <= kMaxValue);
#endif

#ifdef V8_TARGET_ARCH_X64
  // To be representable as a long smi, the value must be a 32-bit integer.
  bool result = (value == static_cast<int32_t>(value));
#else
  // To be representable as an tagged small integer, the two
  // most-significant bits of 'value' must be either 00 or 11 due to
  // sign-extension. To check this we add 01 to the two
  // most-significant bits, and check if the most-significant bit is 0
  //
  // CAUTION: The original code below:
  // bool result = ((value + 0x40000000) & 0x80000000) == 0;
  // may lead to incorrect results according to the C language spec, and
  // in fact doesn't work correctly with gcc4.1.1 in some cases: The
  // compiler may produce undefined results in case of signed integer
  // overflow. The computation must be done w/ unsigned ints.
  bool result = (static_cast<uintptr_t>(value + 0x40000000U) < 0x80000000U);
#endif
  ASSERT(result == in_range);
  return result;
}


MapWord MapWord::FromMap(Map* map) {
  return MapWord(reinterpret_cast<uintptr_t>(map));
}


Map* MapWord::ToMap() {
  return reinterpret_cast<Map*>(value_);
}


bool MapWord::IsForwardingAddress() {
  return HAS_SMI_TAG(reinterpret_cast<Object*>(value_));
}


MapWord MapWord::FromForwardingAddress(HeapObject* object) {
  Address raw = reinterpret_cast<Address>(object) - kHeapObjectTag;
  return MapWord(reinterpret_cast<uintptr_t>(raw));
}


HeapObject* MapWord::ToForwardingAddress() {
  ASSERT(IsForwardingAddress());
  return HeapObject::FromAddress(reinterpret_cast<Address>(value_));
}


bool MapWord::IsMarked() {
  return (value_ & kMarkingMask) == 0;
}


void MapWord::SetMark() {
  value_ &= ~kMarkingMask;
}


void MapWord::ClearMark() {
  value_ |= kMarkingMask;
}


bool MapWord::IsOverflowed() {
  return (value_ & kOverflowMask) != 0;
}


void MapWord::SetOverflow() {
  value_ |= kOverflowMask;
}


void MapWord::ClearOverflow() {
  value_ &= ~kOverflowMask;
}


MapWord MapWord::EncodeAddress(Address map_address, int offset) {
  // Offset is the distance in live bytes from the first live object in the
  // same page. The offset between two objects in the same page should not
  // exceed the object area size of a page.
  ASSERT(0 <= offset && offset < Page::kObjectAreaSize);

  uintptr_t compact_offset = offset >> kObjectAlignmentBits;
  ASSERT(compact_offset < (1 << kForwardingOffsetBits));

  Page* map_page = Page::FromAddress(map_address);
  ASSERT_MAP_PAGE_INDEX(map_page->mc_page_index);

  uintptr_t map_page_offset =
      map_page->Offset(map_address) >> kMapAlignmentBits;

  uintptr_t encoding =
      (compact_offset << kForwardingOffsetShift) |
      (map_page_offset << kMapPageOffsetShift) |
      (map_page->mc_page_index << kMapPageIndexShift);
  return MapWord(encoding);
}


Address MapWord::DecodeMapAddress(MapSpace* map_space) {
  int map_page_index =
      static_cast<int>((value_ & kMapPageIndexMask) >> kMapPageIndexShift);
  ASSERT_MAP_PAGE_INDEX(map_page_index);

  int map_page_offset = static_cast<int>(
      ((value_ & kMapPageOffsetMask) >> kMapPageOffsetShift) <<
      kMapAlignmentBits);

  return (map_space->PageAddress(map_page_index) + map_page_offset);
}


int MapWord::DecodeOffset() {
  // The offset field is represented in the kForwardingOffsetBits
  // most-significant bits.
  uintptr_t offset = (value_ >> kForwardingOffsetShift) << kObjectAlignmentBits;
  ASSERT(offset < static_cast<uintptr_t>(Page::kObjectAreaSize));
  return static_cast<int>(offset);
}


MapWord MapWord::FromEncodedAddress(Address address) {
  return MapWord(reinterpret_cast<uintptr_t>(address));
}


Address MapWord::ToEncodedAddress() {
  return reinterpret_cast<Address>(value_);
}


#ifdef DEBUG
void HeapObject::VerifyObjectField(int offset) {
  VerifyPointer(READ_FIELD(this, offset));
}

void HeapObject::VerifySmiField(int offset) {
  ASSERT(READ_FIELD(this, offset)->IsSmi());
}
#endif


Heap* HeapObject::GetHeap() {
  // During GC, the map pointer in HeapObject is used in various ways that
  // prevent us from retrieving Heap from the map.
  // Assert that we are not in GC, implement GC code in a way that it doesn't
  // pull heap from the map.
  ASSERT(HEAP->is_safe_to_read_maps());
  return map()->heap();
}


Isolate* HeapObject::GetIsolate() {
  return GetHeap()->isolate();
}


Map* HeapObject::map() {
  return map_word().ToMap();
}


void HeapObject::set_map(Map* value) {
  set_map_word(MapWord::FromMap(value));
}


MapWord HeapObject::map_word() {
  return MapWord(reinterpret_cast<uintptr_t>(READ_FIELD(this, kMapOffset)));
}


void HeapObject::set_map_word(MapWord map_word) {
  // WRITE_FIELD does not invoke write barrier, but there is no need
  // here.
  WRITE_FIELD(this, kMapOffset, reinterpret_cast<Object*>(map_word.value_));
}


HeapObject* HeapObject::FromAddress(Address address) {
  ASSERT_TAG_ALIGNED(address);
  return reinterpret_cast<HeapObject*>(address + kHeapObjectTag);
}


Address HeapObject::address() {
  return reinterpret_cast<Address>(this) - kHeapObjectTag;
}


int HeapObject::Size() {
  return SizeFromMap(map());
}


void HeapObject::IteratePointers(ObjectVisitor* v, int start, int end) {
  v->VisitPointers(reinterpret_cast<Object**>(FIELD_ADDR(this, start)),
                   reinterpret_cast<Object**>(FIELD_ADDR(this, end)));
}


void HeapObject::IteratePointer(ObjectVisitor* v, int offset) {
  v->VisitPointer(reinterpret_cast<Object**>(FIELD_ADDR(this, offset)));
}


bool HeapObject::IsMarked() {
  return map_word().IsMarked();
}


void HeapObject::SetMark() {
  ASSERT(!IsMarked());
  MapWord first_word = map_word();
  first_word.SetMark();
  set_map_word(first_word);
}


void HeapObject::ClearMark() {
  ASSERT(IsMarked());
  MapWord first_word = map_word();
  first_word.ClearMark();
  set_map_word(first_word);
}


bool HeapObject::IsOverflowed() {
  return map_word().IsOverflowed();
}


void HeapObject::SetOverflow() {
  MapWord first_word = map_word();
  first_word.SetOverflow();
  set_map_word(first_word);
}


void HeapObject::ClearOverflow() {
  ASSERT(IsOverflowed());
  MapWord first_word = map_word();
  first_word.ClearOverflow();
  set_map_word(first_word);
}


double HeapNumber::value() {
  return READ_DOUBLE_FIELD(this, kValueOffset);
}


void HeapNumber::set_value(double value) {
  WRITE_DOUBLE_FIELD(this, kValueOffset, value);
}


int HeapNumber::get_exponent() {
  return ((READ_INT_FIELD(this, kExponentOffset) & kExponentMask) >>
          kExponentShift) - kExponentBias;
}


int HeapNumber::get_sign() {
  return READ_INT_FIELD(this, kExponentOffset) & kSignMask;
}


ACCESSORS(JSObject, properties, FixedArray, kPropertiesOffset)


FixedArrayBase* JSObject::elements() {
  Object* array = READ_FIELD(this, kElementsOffset);
  ASSERT(array->HasValidElements());
  return static_cast<FixedArrayBase*>(array);
}


void JSObject::set_elements(FixedArrayBase* value, WriteBarrierMode mode) {
  ASSERT(map()->has_fast_elements() ==
         (value->map() == GetHeap()->fixed_array_map() ||
          value->map() == GetHeap()->fixed_cow_array_map()));
  ASSERT(map()->has_fast_double_elements() ==
         value->IsFixedDoubleArray());
  ASSERT(value->HasValidElements());
  WRITE_FIELD(this, kElementsOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kElementsOffset, mode);
}


void JSObject::initialize_properties() {
  ASSERT(!GetHeap()->InNewSpace(GetHeap()->empty_fixed_array()));
  WRITE_FIELD(this, kPropertiesOffset, GetHeap()->empty_fixed_array());
}


void JSObject::initialize_elements() {
  ASSERT(map()->has_fast_elements());
  ASSERT(!GetHeap()->InNewSpace(GetHeap()->empty_fixed_array()));
  WRITE_FIELD(this, kElementsOffset, GetHeap()->empty_fixed_array());
}


MaybeObject* JSObject::ResetElements() {
  Object* obj;
  { MaybeObject* maybe_obj = map()->GetFastElementsMap();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  set_map(Map::cast(obj));
  initialize_elements();
  return this;
}


ACCESSORS(Oddball, to_string, String, kToStringOffset)
ACCESSORS(Oddball, to_number, Object, kToNumberOffset)


byte Oddball::kind() {
  return READ_BYTE_FIELD(this, kKindOffset);
}


void Oddball::set_kind(byte value) {
  WRITE_BYTE_FIELD(this, kKindOffset, value);
}


Object* JSGlobalPropertyCell::value() {
  return READ_FIELD(this, kValueOffset);
}


void JSGlobalPropertyCell::set_value(Object* val, WriteBarrierMode ignored) {
  // The write barrier is not used for global property cells.
  ASSERT(!val->IsJSGlobalPropertyCell());
  WRITE_FIELD(this, kValueOffset, val);
}


int JSObject::GetHeaderSize() {
  InstanceType type = map()->instance_type();
  // Check for the most common kind of JavaScript object before
  // falling into the generic switch. This speeds up the internal
  // field operations considerably on average.
  if (type == JS_OBJECT_TYPE) return JSObject::kHeaderSize;
  switch (type) {
    case JS_GLOBAL_PROXY_TYPE:
      return JSGlobalProxy::kSize;
    case JS_GLOBAL_OBJECT_TYPE:
      return JSGlobalObject::kSize;
    case JS_BUILTINS_OBJECT_TYPE:
      return JSBuiltinsObject::kSize;
    case JS_FUNCTION_TYPE:
      return JSFunction::kSize;
    case JS_VALUE_TYPE:
      return JSValue::kSize;
    case JS_ARRAY_TYPE:
      return JSValue::kSize;
    case JS_WEAK_MAP_TYPE:
      return JSWeakMap::kSize;
    case JS_REGEXP_TYPE:
      return JSValue::kSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kSize;
    default:
      UNREACHABLE();
      return 0;
  }
}


int JSObject::GetInternalFieldCount() {
  ASSERT(1 << kPointerSizeLog2 == kPointerSize);
  // Make sure to adjust for the number of in-object properties. These
  // properties do contribute to the size, but are not internal fields.
  return ((Size() - GetHeaderSize()) >> kPointerSizeLog2) -
         map()->inobject_properties();
}


int JSObject::GetInternalFieldOffset(int index) {
  ASSERT(index < GetInternalFieldCount() && index >= 0);
  return GetHeaderSize() + (kPointerSize * index);
}


Object* JSObject::GetInternalField(int index) {
  ASSERT(index < GetInternalFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  return READ_FIELD(this, GetHeaderSize() + (kPointerSize * index));
}


void JSObject::SetInternalField(int index, Object* value) {
  ASSERT(index < GetInternalFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  int offset = GetHeaderSize() + (kPointerSize * index);
  WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(this, offset);
}


// Access fast-case object properties at index. The use of these routines
// is needed to correctly distinguish between properties stored in-object and
// properties stored in the properties array.
Object* JSObject::FastPropertyAt(int index) {
  // Adjust for the number of properties stored in the object.
  index -= map()->inobject_properties();
  if (index < 0) {
    int offset = map()->instance_size() + (index * kPointerSize);
    return READ_FIELD(this, offset);
  } else {
    ASSERT(index < properties()->length());
    return properties()->get(index);
  }
}


Object* JSObject::FastPropertyAtPut(int index, Object* value) {
  // Adjust for the number of properties stored in the object.
  index -= map()->inobject_properties();
  if (index < 0) {
    int offset = map()->instance_size() + (index * kPointerSize);
    WRITE_FIELD(this, offset, value);
    WRITE_BARRIER(this, offset);
  } else {
    ASSERT(index < properties()->length());
    properties()->set(index, value);
  }
  return value;
}


int JSObject::GetInObjectPropertyOffset(int index) {
  // Adjust for the number of properties stored in the object.
  index -= map()->inobject_properties();
  ASSERT(index < 0);
  return map()->instance_size() + (index * kPointerSize);
}


Object* JSObject::InObjectPropertyAt(int index) {
  // Adjust for the number of properties stored in the object.
  index -= map()->inobject_properties();
  ASSERT(index < 0);
  int offset = map()->instance_size() + (index * kPointerSize);
  return READ_FIELD(this, offset);
}


Object* JSObject::InObjectPropertyAtPut(int index,
                                        Object* value,
                                        WriteBarrierMode mode) {
  // Adjust for the number of properties stored in the object.
  index -= map()->inobject_properties();
  ASSERT(index < 0);
  int offset = map()->instance_size() + (index * kPointerSize);
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, mode);
  return value;
}



void JSObject::InitializeBody(int object_size, Object* value) {
  ASSERT(!value->IsHeapObject() || !GetHeap()->InNewSpace(value));
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}


bool JSObject::HasFastProperties() {
  return !properties()->IsDictionary();
}


int JSObject::MaxFastProperties() {
  // Allow extra fast properties if the object has more than
  // kMaxFastProperties in-object properties. When this is the case,
  // it is very unlikely that the object is being used as a dictionary
  // and there is a good chance that allowing more map transitions
  // will be worth it.
  return Max(map()->inobject_properties(), kMaxFastProperties);
}


void Struct::InitializeBody(int object_size) {
  Object* value = GetHeap()->undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}


bool Object::ToArrayIndex(uint32_t* index) {
  if (IsSmi()) {
    int value = Smi::cast(this)->value();
    if (value < 0) return false;
    *index = value;
    return true;
  }
  if (IsHeapNumber()) {
    double value = HeapNumber::cast(this)->value();
    uint32_t uint_value = static_cast<uint32_t>(value);
    if (value == static_cast<double>(uint_value)) {
      *index = uint_value;
      return true;
    }
  }
  return false;
}


bool Object::IsStringObjectWithCharacterAt(uint32_t index) {
  if (!this->IsJSValue()) return false;

  JSValue* js_value = JSValue::cast(this);
  if (!js_value->value()->IsString()) return false;

  String* str = String::cast(js_value->value());
  if (index >= (uint32_t)str->length()) return false;

  return true;
}


FixedArrayBase* FixedArrayBase::cast(Object* object) {
  ASSERT(object->IsFixedArray() || object->IsFixedDoubleArray());
  return reinterpret_cast<FixedArrayBase*>(object);
}


Object* FixedArray::get(int index) {
  ASSERT(index >= 0 && index < this->length());
  return READ_FIELD(this, kHeaderSize + index * kPointerSize);
}


void FixedArray::set(int index, Smi* value) {
  ASSERT(map() != HEAP->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  ASSERT(reinterpret_cast<Object*>(value)->IsSmi());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
}


void FixedArray::set(int index, Object* value) {
  ASSERT(map() != HEAP->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(this, offset);
}


inline bool FixedDoubleArray::is_the_hole_nan(double value) {
  return BitCast<uint64_t, double>(value) == kHoleNanInt64;
}


inline double FixedDoubleArray::hole_nan_as_double() {
  return BitCast<double, uint64_t>(kHoleNanInt64);
}


inline double FixedDoubleArray::canonical_not_the_hole_nan_as_double() {
  ASSERT(BitCast<uint64_t>(OS::nan_value()) != kHoleNanInt64);
  ASSERT((BitCast<uint64_t>(OS::nan_value()) >> 32) != kHoleNanUpper32);
  return OS::nan_value();
}


double FixedDoubleArray::get_scalar(int index) {
  ASSERT(map() != HEAP->fixed_cow_array_map() &&
         map() != HEAP->fixed_array_map());
  ASSERT(index >= 0 && index < this->length());
  double result = READ_DOUBLE_FIELD(this, kHeaderSize + index * kDoubleSize);
  ASSERT(!is_the_hole_nan(result));
  return result;
}


MaybeObject* FixedDoubleArray::get(int index) {
  if (is_the_hole(index)) {
    return GetHeap()->the_hole_value();
  } else {
    return GetHeap()->NumberFromDouble(get_scalar(index));
  }
}


void FixedDoubleArray::set(int index, double value) {
  ASSERT(map() != HEAP->fixed_cow_array_map() &&
         map() != HEAP->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  if (isnan(value)) value = canonical_not_the_hole_nan_as_double();
  WRITE_DOUBLE_FIELD(this, offset, value);
}


void FixedDoubleArray::set_the_hole(int index) {
  ASSERT(map() != HEAP->fixed_cow_array_map() &&
         map() != HEAP->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  WRITE_DOUBLE_FIELD(this, offset, hole_nan_as_double());
}


bool FixedDoubleArray::is_the_hole(int index) {
  int offset = kHeaderSize + index * kDoubleSize;
  return is_the_hole_nan(READ_DOUBLE_FIELD(this, offset));
}


void FixedDoubleArray::Initialize(FixedDoubleArray* from) {
  int old_length = from->length();
  ASSERT(old_length < length());
  if (old_length * kDoubleSize >= OS::kMinComplexMemCopy) {
    OS::MemCopy(FIELD_ADDR(this, kHeaderSize),
                FIELD_ADDR(from, kHeaderSize),
                old_length * kDoubleSize);
  } else {
    for (int i = 0; i < old_length; ++i) {
      if (from->is_the_hole(i)) {
        set_the_hole(i);
      } else {
        set(i, from->get_scalar(i));
      }
    }
  }
  int offset = kHeaderSize + old_length * kDoubleSize;
  for (int current = from->length(); current < length(); ++current) {
    WRITE_DOUBLE_FIELD(this, offset, hole_nan_as_double());
    offset += kDoubleSize;
  }
}


void FixedDoubleArray::Initialize(FixedArray* from) {
  int old_length = from->length();
  ASSERT(old_length < length());
  for (int i = 0; i < old_length; i++) {
    Object* hole_or_object = from->get(i);
    if (hole_or_object->IsTheHole()) {
      set_the_hole(i);
    } else {
      set(i, hole_or_object->Number());
    }
  }
  int offset = kHeaderSize + old_length * kDoubleSize;
  for (int current = from->length(); current < length(); ++current) {
    WRITE_DOUBLE_FIELD(this, offset, hole_nan_as_double());
    offset += kDoubleSize;
  }
}


void FixedDoubleArray::Initialize(SeededNumberDictionary* from) {
  int offset = kHeaderSize;
  for (int current = 0; current < length(); ++current) {
    WRITE_DOUBLE_FIELD(this, offset, hole_nan_as_double());
    offset += kDoubleSize;
  }
  for (int i = 0; i < from->Capacity(); i++) {
    Object* key = from->KeyAt(i);
    if (key->IsNumber()) {
      uint32_t entry = static_cast<uint32_t>(key->Number());
      set(entry, from->ValueAt(i)->Number());
    }
  }
}


WriteBarrierMode HeapObject::GetWriteBarrierMode(const AssertNoAllocation&) {
  if (GetHeap()->InNewSpace(this)) return SKIP_WRITE_BARRIER;
  return UPDATE_WRITE_BARRIER;
}


void FixedArray::set(int index,
                     Object* value,
                     WriteBarrierMode mode) {
  ASSERT(map() != HEAP->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, mode);
}


void FixedArray::fast_set(FixedArray* array, int index, Object* value) {
  ASSERT(array->map() != HEAP->raw_unchecked_fixed_cow_array_map());
  ASSERT(index >= 0 && index < array->length());
  ASSERT(!HEAP->InNewSpace(value));
  WRITE_FIELD(array, kHeaderSize + index * kPointerSize, value);
}


void FixedArray::set_undefined(int index) {
  ASSERT(map() != HEAP->fixed_cow_array_map());
  set_undefined(GetHeap(), index);
}


void FixedArray::set_undefined(Heap* heap, int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!heap->InNewSpace(heap->undefined_value()));
  WRITE_FIELD(this, kHeaderSize + index * kPointerSize,
              heap->undefined_value());
}


void FixedArray::set_null(int index) {
  set_null(GetHeap(), index);
}


void FixedArray::set_null(Heap* heap, int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!heap->InNewSpace(heap->null_value()));
  WRITE_FIELD(this, kHeaderSize + index * kPointerSize, heap->null_value());
}


void FixedArray::set_the_hole(int index) {
  ASSERT(map() != HEAP->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!HEAP->InNewSpace(HEAP->the_hole_value()));
  WRITE_FIELD(this,
              kHeaderSize + index * kPointerSize,
              GetHeap()->the_hole_value());
}


void FixedArray::set_unchecked(int index, Smi* value) {
  ASSERT(reinterpret_cast<Object*>(value)->IsSmi());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
}


void FixedArray::set_unchecked(Heap* heap,
                               int index,
                               Object* value,
                               WriteBarrierMode mode) {
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(heap, this, offset, mode);
}


void FixedArray::set_null_unchecked(Heap* heap, int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!HEAP->InNewSpace(heap->null_value()));
  WRITE_FIELD(this, kHeaderSize + index * kPointerSize, heap->null_value());
}


Object** FixedArray::data_start() {
  return HeapObject::RawField(this, kHeaderSize);
}


bool DescriptorArray::IsEmpty() {
  ASSERT(this->IsSmi() ||
         this->length() > kFirstIndex ||
         this == HEAP->empty_descriptor_array());
  return this->IsSmi() || length() <= kFirstIndex;
}


int DescriptorArray::bit_field3_storage() {
  Object* storage = READ_FIELD(this, kBitField3StorageOffset);
  return Smi::cast(storage)->value();
}

void DescriptorArray::set_bit_field3_storage(int value) {
  ASSERT(!IsEmpty());
  WRITE_FIELD(this, kBitField3StorageOffset, Smi::FromInt(value));
}


void DescriptorArray::fast_swap(FixedArray* array, int first, int second) {
  Object* tmp = array->get(first);
  fast_set(array, first, array->get(second));
  fast_set(array, second, tmp);
}


int DescriptorArray::Search(String* name) {
  SLOW_ASSERT(IsSortedNoDuplicates());

  // Check for empty descriptor array.
  int nof = number_of_descriptors();
  if (nof == 0) return kNotFound;

  // Fast case: do linear search for small arrays.
  const int kMaxElementsForLinearSearch = 8;
  if (StringShape(name).IsSymbol() && nof < kMaxElementsForLinearSearch) {
    return LinearSearch(name, nof);
  }

  // Slow case: perform binary search.
  return BinarySearch(name, 0, nof - 1);
}


int DescriptorArray::SearchWithCache(String* name) {
  int number = GetIsolate()->descriptor_lookup_cache()->Lookup(this, name);
  if (number == DescriptorLookupCache::kAbsent) {
    number = Search(name);
    GetIsolate()->descriptor_lookup_cache()->Update(this, name, number);
  }
  return number;
}


String* DescriptorArray::GetKey(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return String::cast(get(ToKeyIndex(descriptor_number)));
}


Object* DescriptorArray::GetValue(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return GetContentArray()->get(ToValueIndex(descriptor_number));
}


Smi* DescriptorArray::GetDetails(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return Smi::cast(GetContentArray()->get(ToDetailsIndex(descriptor_number)));
}


PropertyType DescriptorArray::GetType(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return PropertyDetails(GetDetails(descriptor_number)).type();
}


int DescriptorArray::GetFieldIndex(int descriptor_number) {
  return Descriptor::IndexFromValue(GetValue(descriptor_number));
}


JSFunction* DescriptorArray::GetConstantFunction(int descriptor_number) {
  return JSFunction::cast(GetValue(descriptor_number));
}


Object* DescriptorArray::GetCallbacksObject(int descriptor_number) {
  ASSERT(GetType(descriptor_number) == CALLBACKS);
  return GetValue(descriptor_number);
}


AccessorDescriptor* DescriptorArray::GetCallbacks(int descriptor_number) {
  ASSERT(GetType(descriptor_number) == CALLBACKS);
  Foreign* p = Foreign::cast(GetCallbacksObject(descriptor_number));
  return reinterpret_cast<AccessorDescriptor*>(p->address());
}


bool DescriptorArray::IsProperty(int descriptor_number) {
  return GetType(descriptor_number) < FIRST_PHANTOM_PROPERTY_TYPE;
}


bool DescriptorArray::IsTransition(int descriptor_number) {
  PropertyType t = GetType(descriptor_number);
  return t == MAP_TRANSITION || t == CONSTANT_TRANSITION ||
      t == ELEMENTS_TRANSITION;
}


bool DescriptorArray::IsNullDescriptor(int descriptor_number) {
  return GetType(descriptor_number) == NULL_DESCRIPTOR;
}


bool DescriptorArray::IsDontEnum(int descriptor_number) {
  return PropertyDetails(GetDetails(descriptor_number)).IsDontEnum();
}


void DescriptorArray::Get(int descriptor_number, Descriptor* desc) {
  desc->Init(GetKey(descriptor_number),
             GetValue(descriptor_number),
             PropertyDetails(GetDetails(descriptor_number)));
}


void DescriptorArray::Set(int descriptor_number, Descriptor* desc) {
  // Range check.
  ASSERT(descriptor_number < number_of_descriptors());

  // Make sure none of the elements in desc are in new space.
  ASSERT(!HEAP->InNewSpace(desc->GetKey()));
  ASSERT(!HEAP->InNewSpace(desc->GetValue()));

  fast_set(this, ToKeyIndex(descriptor_number), desc->GetKey());
  FixedArray* content_array = GetContentArray();
  fast_set(content_array, ToValueIndex(descriptor_number), desc->GetValue());
  fast_set(content_array, ToDetailsIndex(descriptor_number),
           desc->GetDetails().AsSmi());
}


void DescriptorArray::CopyFrom(int index, DescriptorArray* src, int src_index) {
  Descriptor desc;
  src->Get(src_index, &desc);
  Set(index, &desc);
}


void DescriptorArray::Swap(int first, int second) {
  fast_swap(this, ToKeyIndex(first), ToKeyIndex(second));
  FixedArray* content_array = GetContentArray();
  fast_swap(content_array, ToValueIndex(first), ToValueIndex(second));
  fast_swap(content_array, ToDetailsIndex(first),  ToDetailsIndex(second));
}


template<typename Shape, typename Key>
int HashTable<Shape, Key>::ComputeCapacity(int at_least_space_for) {
  const int kMinCapacity = 32;
  int capacity = RoundUpToPowerOf2(at_least_space_for * 2);
  if (capacity < kMinCapacity) {
    capacity = kMinCapacity;  // Guarantee min capacity.
  }
  return capacity;
}


template<typename Shape, typename Key>
int HashTable<Shape, Key>::FindEntry(Key key) {
  return FindEntry(GetIsolate(), key);
}


// Find entry for key otherwise return kNotFound.
template<typename Shape, typename Key>
int HashTable<Shape, Key>::FindEntry(Isolate* isolate, Key key) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(HashTable<Shape, Key>::Hash(key), capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  while (true) {
    Object* element = KeyAt(entry);
    // Empty entry.
    if (element == isolate->heap()->raw_unchecked_undefined_value()) break;
    if (element != isolate->heap()->raw_unchecked_null_value() &&
        Shape::IsMatch(key, element)) return entry;
    entry = NextProbe(entry, count++, capacity);
  }
  return kNotFound;
}


bool SeededNumberDictionary::requires_slow_elements() {
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return false;
  return 0 !=
      (Smi::cast(max_index_object)->value() & kRequiresSlowElementsMask);
}

uint32_t SeededNumberDictionary::max_number_key() {
  ASSERT(!requires_slow_elements());
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return 0;
  uint32_t value = static_cast<uint32_t>(Smi::cast(max_index_object)->value());
  return value >> kRequiresSlowElementsTagSize;
}

void SeededNumberDictionary::set_requires_slow_elements() {
  set(kMaxNumberKeyIndex, Smi::FromInt(kRequiresSlowElementsMask));
}


// ------------------------------------
// Cast operations


CAST_ACCESSOR(FixedArray)
CAST_ACCESSOR(FixedDoubleArray)
CAST_ACCESSOR(DescriptorArray)
CAST_ACCESSOR(DeoptimizationInputData)
CAST_ACCESSOR(DeoptimizationOutputData)
CAST_ACCESSOR(SymbolTable)
CAST_ACCESSOR(JSFunctionResultCache)
CAST_ACCESSOR(NormalizedMapCache)
CAST_ACCESSOR(CompilationCacheTable)
CAST_ACCESSOR(CodeCacheHashTable)
CAST_ACCESSOR(PolymorphicCodeCacheHashTable)
CAST_ACCESSOR(MapCache)
CAST_ACCESSOR(String)
CAST_ACCESSOR(SeqString)
CAST_ACCESSOR(SeqAsciiString)
CAST_ACCESSOR(SeqTwoByteString)
CAST_ACCESSOR(SlicedString)
CAST_ACCESSOR(ConsString)
CAST_ACCESSOR(ExternalString)
CAST_ACCESSOR(ExternalAsciiString)
CAST_ACCESSOR(ExternalTwoByteString)
CAST_ACCESSOR(JSReceiver)
CAST_ACCESSOR(JSObject)
CAST_ACCESSOR(Smi)
CAST_ACCESSOR(HeapObject)
CAST_ACCESSOR(HeapNumber)
CAST_ACCESSOR(Oddball)
CAST_ACCESSOR(JSGlobalPropertyCell)
CAST_ACCESSOR(SharedFunctionInfo)
CAST_ACCESSOR(Map)
CAST_ACCESSOR(JSFunction)
CAST_ACCESSOR(GlobalObject)
CAST_ACCESSOR(JSGlobalProxy)
CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSBuiltinsObject)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(JSArray)
CAST_ACCESSOR(JSRegExp)
CAST_ACCESSOR(JSProxy)
CAST_ACCESSOR(JSFunctionProxy)
CAST_ACCESSOR(JSWeakMap)
CAST_ACCESSOR(Foreign)
CAST_ACCESSOR(ByteArray)
CAST_ACCESSOR(ExternalArray)
CAST_ACCESSOR(ExternalByteArray)
CAST_ACCESSOR(ExternalUnsignedByteArray)
CAST_ACCESSOR(ExternalShortArray)
CAST_ACCESSOR(ExternalUnsignedShortArray)
CAST_ACCESSOR(ExternalIntArray)
CAST_ACCESSOR(ExternalUnsignedIntArray)
CAST_ACCESSOR(ExternalFloatArray)
CAST_ACCESSOR(ExternalDoubleArray)
CAST_ACCESSOR(ExternalPixelArray)
CAST_ACCESSOR(Struct)


#define MAKE_STRUCT_CAST(NAME, Name, name) CAST_ACCESSOR(Name)
  STRUCT_LIST(MAKE_STRUCT_CAST)
#undef MAKE_STRUCT_CAST


template <typename Shape, typename Key>
HashTable<Shape, Key>* HashTable<Shape, Key>::cast(Object* obj) {
  ASSERT(obj->IsHashTable());
  return reinterpret_cast<HashTable*>(obj);
}


SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)

SMI_ACCESSORS(String, length, kLengthOffset)


uint32_t String::hash_field() {
  return READ_UINT32_FIELD(this, kHashFieldOffset);
}


void String::set_hash_field(uint32_t value) {
  WRITE_UINT32_FIELD(this, kHashFieldOffset, value);
#if V8_HOST_ARCH_64_BIT
  WRITE_UINT32_FIELD(this, kHashFieldOffset + kIntSize, 0);
#endif
}


bool String::Equals(String* other) {
  if (other == this) return true;
  if (StringShape(this).IsSymbol() && StringShape(other).IsSymbol()) {
    return false;
  }
  return SlowEquals(other);
}


MaybeObject* String::TryFlatten(PretenureFlag pretenure) {
  if (!StringShape(this).IsCons()) return this;
  ConsString* cons = ConsString::cast(this);
  if (cons->IsFlat()) return cons->first();
  return SlowTryFlatten(pretenure);
}


String* String::TryFlattenGetString(PretenureFlag pretenure) {
  MaybeObject* flat = TryFlatten(pretenure);
  Object* successfully_flattened;
  if (!flat->ToObject(&successfully_flattened)) return this;
  return String::cast(successfully_flattened);
}


uint16_t String::Get(int index) {
  ASSERT(index >= 0 && index < length());
  switch (StringShape(this).full_representation_tag()) {
    case kSeqStringTag | kAsciiStringTag:
      return SeqAsciiString::cast(this)->SeqAsciiStringGet(index);
    case kSeqStringTag | kTwoByteStringTag:
      return SeqTwoByteString::cast(this)->SeqTwoByteStringGet(index);
    case kConsStringTag | kAsciiStringTag:
    case kConsStringTag | kTwoByteStringTag:
      return ConsString::cast(this)->ConsStringGet(index);
    case kExternalStringTag | kAsciiStringTag:
      return ExternalAsciiString::cast(this)->ExternalAsciiStringGet(index);
    case kExternalStringTag | kTwoByteStringTag:
      return ExternalTwoByteString::cast(this)->ExternalTwoByteStringGet(index);
    case kSlicedStringTag | kAsciiStringTag:
    case kSlicedStringTag | kTwoByteStringTag:
      return SlicedString::cast(this)->SlicedStringGet(index);
    default:
      break;
  }

  UNREACHABLE();
  return 0;
}


void String::Set(int index, uint16_t value) {
  ASSERT(index >= 0 && index < length());
  ASSERT(StringShape(this).IsSequential());

  return this->IsAsciiRepresentation()
      ? SeqAsciiString::cast(this)->SeqAsciiStringSet(index, value)
      : SeqTwoByteString::cast(this)->SeqTwoByteStringSet(index, value);
}


bool String::IsFlat() {
  if (!StringShape(this).IsCons()) return true;
  return ConsString::cast(this)->second()->length() == 0;
}


String* String::GetUnderlying() {
  // Giving direct access to underlying string only makes sense if the
  // wrapping string is already flattened.
  ASSERT(this->IsFlat());
  ASSERT(StringShape(this).IsIndirect());
  STATIC_ASSERT(ConsString::kFirstOffset == SlicedString::kParentOffset);
  const int kUnderlyingOffset = SlicedString::kParentOffset;
  return String::cast(READ_FIELD(this, kUnderlyingOffset));
}


uint16_t SeqAsciiString::SeqAsciiStringGet(int index) {
  ASSERT(index >= 0 && index < length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}


void SeqAsciiString::SeqAsciiStringSet(int index, uint16_t value) {
  ASSERT(index >= 0 && index < length() && value <= kMaxAsciiCharCode);
  WRITE_BYTE_FIELD(this, kHeaderSize + index * kCharSize,
                   static_cast<byte>(value));
}


Address SeqAsciiString::GetCharsAddress() {
  return FIELD_ADDR(this, kHeaderSize);
}


char* SeqAsciiString::GetChars() {
  return reinterpret_cast<char*>(GetCharsAddress());
}


Address SeqTwoByteString::GetCharsAddress() {
  return FIELD_ADDR(this, kHeaderSize);
}


uc16* SeqTwoByteString::GetChars() {
  return reinterpret_cast<uc16*>(FIELD_ADDR(this, kHeaderSize));
}


uint16_t SeqTwoByteString::SeqTwoByteStringGet(int index) {
  ASSERT(index >= 0 && index < length());
  return READ_SHORT_FIELD(this, kHeaderSize + index * kShortSize);
}


void SeqTwoByteString::SeqTwoByteStringSet(int index, uint16_t value) {
  ASSERT(index >= 0 && index < length());
  WRITE_SHORT_FIELD(this, kHeaderSize + index * kShortSize, value);
}


int SeqTwoByteString::SeqTwoByteStringSize(InstanceType instance_type) {
  return SizeFor(length());
}


int SeqAsciiString::SeqAsciiStringSize(InstanceType instance_type) {
  return SizeFor(length());
}


String* SlicedString::parent() {
  return String::cast(READ_FIELD(this, kParentOffset));
}


void SlicedString::set_parent(String* parent) {
  ASSERT(parent->IsSeqString());
  WRITE_FIELD(this, kParentOffset, parent);
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
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kFirstOffset, mode);
}


String* ConsString::second() {
  return String::cast(READ_FIELD(this, kSecondOffset));
}


Object* ConsString::unchecked_second() {
  return READ_FIELD(this, kSecondOffset);
}


void ConsString::set_second(String* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kSecondOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kSecondOffset, mode);
}


ExternalAsciiString::Resource* ExternalAsciiString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalAsciiString::set_resource(
    ExternalAsciiString::Resource* resource) {
  *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)) = resource;
}


ExternalTwoByteString::Resource* ExternalTwoByteString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalTwoByteString::set_resource(
    ExternalTwoByteString::Resource* resource) {
  *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)) = resource;
}


void JSFunctionResultCache::MakeZeroSize() {
  set_finger_index(kEntriesIndex);
  set_size(kEntriesIndex);
}


void JSFunctionResultCache::Clear() {
  int cache_size = size();
  Object** entries_start = RawField(this, OffsetOfElementAt(kEntriesIndex));
  MemsetPointer(entries_start,
                GetHeap()->the_hole_value(),
                cache_size - kEntriesIndex);
  MakeZeroSize();
}


int JSFunctionResultCache::size() {
  return Smi::cast(get(kCacheSizeIndex))->value();
}


void JSFunctionResultCache::set_size(int size) {
  set(kCacheSizeIndex, Smi::FromInt(size));
}


int JSFunctionResultCache::finger_index() {
  return Smi::cast(get(kFingerIndex))->value();
}


void JSFunctionResultCache::set_finger_index(int finger_index) {
  set(kFingerIndex, Smi::FromInt(finger_index));
}


byte ByteArray::get(int index) {
  ASSERT(index >= 0 && index < this->length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}


void ByteArray::set(int index, byte value) {
  ASSERT(index >= 0 && index < this->length());
  WRITE_BYTE_FIELD(this, kHeaderSize + index * kCharSize, value);
}


int ByteArray::get_int(int index) {
  ASSERT(index >= 0 && (index * kIntSize) < this->length());
  return READ_INT_FIELD(this, kHeaderSize + index * kIntSize);
}


ByteArray* ByteArray::FromDataStartAddress(Address address) {
  ASSERT_TAG_ALIGNED(address);
  return reinterpret_cast<ByteArray*>(address - kHeaderSize + kHeapObjectTag);
}


Address ByteArray::GetDataStartAddress() {
  return reinterpret_cast<Address>(this) - kHeapObjectTag + kHeaderSize;
}


uint8_t* ExternalPixelArray::external_pixel_pointer() {
  return reinterpret_cast<uint8_t*>(external_pointer());
}


uint8_t ExternalPixelArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = external_pixel_pointer();
  return ptr[index];
}


MaybeObject* ExternalPixelArray::get(int index) {
  return Smi::FromInt(static_cast<int>(get_scalar(index)));
}


void ExternalPixelArray::set(int index, uint8_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = external_pixel_pointer();
  ptr[index] = value;
}


void* ExternalArray::external_pointer() {
  intptr_t ptr = READ_INTPTR_FIELD(this, kExternalPointerOffset);
  return reinterpret_cast<void*>(ptr);
}


void ExternalArray::set_external_pointer(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kExternalPointerOffset, ptr);
}


int8_t ExternalByteArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  int8_t* ptr = static_cast<int8_t*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalByteArray::get(int index) {
  return Smi::FromInt(static_cast<int>(get_scalar(index)));
}


void ExternalByteArray::set(int index, int8_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  int8_t* ptr = static_cast<int8_t*>(external_pointer());
  ptr[index] = value;
}


uint8_t ExternalUnsignedByteArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = static_cast<uint8_t*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalUnsignedByteArray::get(int index) {
  return Smi::FromInt(static_cast<int>(get_scalar(index)));
}


void ExternalUnsignedByteArray::set(int index, uint8_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = static_cast<uint8_t*>(external_pointer());
  ptr[index] = value;
}


int16_t ExternalShortArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  int16_t* ptr = static_cast<int16_t*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalShortArray::get(int index) {
  return Smi::FromInt(static_cast<int>(get_scalar(index)));
}


void ExternalShortArray::set(int index, int16_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  int16_t* ptr = static_cast<int16_t*>(external_pointer());
  ptr[index] = value;
}


uint16_t ExternalUnsignedShortArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint16_t* ptr = static_cast<uint16_t*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalUnsignedShortArray::get(int index) {
  return Smi::FromInt(static_cast<int>(get_scalar(index)));
}


void ExternalUnsignedShortArray::set(int index, uint16_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint16_t* ptr = static_cast<uint16_t*>(external_pointer());
  ptr[index] = value;
}


int32_t ExternalIntArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  int32_t* ptr = static_cast<int32_t*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalIntArray::get(int index) {
    return GetHeap()->NumberFromInt32(get_scalar(index));
}


void ExternalIntArray::set(int index, int32_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  int32_t* ptr = static_cast<int32_t*>(external_pointer());
  ptr[index] = value;
}


uint32_t ExternalUnsignedIntArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint32_t* ptr = static_cast<uint32_t*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalUnsignedIntArray::get(int index) {
    return GetHeap()->NumberFromUint32(get_scalar(index));
}


void ExternalUnsignedIntArray::set(int index, uint32_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint32_t* ptr = static_cast<uint32_t*>(external_pointer());
  ptr[index] = value;
}


float ExternalFloatArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  float* ptr = static_cast<float*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalFloatArray::get(int index) {
    return GetHeap()->NumberFromDouble(get_scalar(index));
}


void ExternalFloatArray::set(int index, float value) {
  ASSERT((index >= 0) && (index < this->length()));
  float* ptr = static_cast<float*>(external_pointer());
  ptr[index] = value;
}


double ExternalDoubleArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  double* ptr = static_cast<double*>(external_pointer());
  return ptr[index];
}


MaybeObject* ExternalDoubleArray::get(int index) {
    return GetHeap()->NumberFromDouble(get_scalar(index));
}


void ExternalDoubleArray::set(int index, double value) {
  ASSERT((index >= 0) && (index < this->length()));
  double* ptr = static_cast<double*>(external_pointer());
  ptr[index] = value;
}


int Map::visitor_id() {
  return READ_BYTE_FIELD(this, kVisitorIdOffset);
}


void Map::set_visitor_id(int id) {
  ASSERT(0 <= id && id < 256);
  WRITE_BYTE_FIELD(this, kVisitorIdOffset, static_cast<byte>(id));
}


int Map::instance_size() {
  return READ_BYTE_FIELD(this, kInstanceSizeOffset) << kPointerSizeLog2;
}


int Map::inobject_properties() {
  return READ_BYTE_FIELD(this, kInObjectPropertiesOffset);
}


int Map::pre_allocated_property_fields() {
  return READ_BYTE_FIELD(this, kPreAllocatedPropertyFieldsOffset);
}


int HeapObject::SizeFromMap(Map* map) {
  int instance_size = map->instance_size();
  if (instance_size != kVariableSizeSentinel) return instance_size;
  // We can ignore the "symbol" bit becase it is only set for symbols
  // and implies a string type.
  int instance_type = static_cast<int>(map->instance_type()) & ~kIsSymbolMask;
  // Only inline the most frequent cases.
  if (instance_type == FIXED_ARRAY_TYPE) {
    return FixedArray::BodyDescriptor::SizeOf(map, this);
  }
  if (instance_type == ASCII_STRING_TYPE) {
    return SeqAsciiString::SizeFor(
        reinterpret_cast<SeqAsciiString*>(this)->length());
  }
  if (instance_type == BYTE_ARRAY_TYPE) {
    return reinterpret_cast<ByteArray*>(this)->ByteArraySize();
  }
  if (instance_type == STRING_TYPE) {
    return SeqTwoByteString::SizeFor(
        reinterpret_cast<SeqTwoByteString*>(this)->length());
  }
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
    return FixedDoubleArray::SizeFor(
        reinterpret_cast<FixedDoubleArray*>(this)->length());
  }
  ASSERT(instance_type == CODE_TYPE);
  return reinterpret_cast<Code*>(this)->CodeSize();
}


void Map::set_instance_size(int value) {
  ASSERT_EQ(0, value & (kPointerSize - 1));
  value >>= kPointerSizeLog2;
  ASSERT(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this, kInstanceSizeOffset, static_cast<byte>(value));
}


void Map::set_inobject_properties(int value) {
  ASSERT(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this, kInObjectPropertiesOffset, static_cast<byte>(value));
}


void Map::set_pre_allocated_property_fields(int value) {
  ASSERT(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this,
                   kPreAllocatedPropertyFieldsOffset,
                   static_cast<byte>(value));
}


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


byte Map::bit_field() {
  return READ_BYTE_FIELD(this, kBitFieldOffset);
}


void Map::set_bit_field(byte value) {
  WRITE_BYTE_FIELD(this, kBitFieldOffset, value);
}


byte Map::bit_field2() {
  return READ_BYTE_FIELD(this, kBitField2Offset);
}


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


void Map::set_function_with_prototype(bool value) {
  if (value) {
    set_bit_field2(bit_field2() | (1 << kFunctionWithPrototype));
  } else {
    set_bit_field2(bit_field2() & ~(1 << kFunctionWithPrototype));
  }
}


bool Map::function_with_prototype() {
  return ((1 << kFunctionWithPrototype) & bit_field2()) != 0;
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


void Map::set_attached_to_shared_function_info(bool value) {
  if (value) {
    set_bit_field2(bit_field2() | (1 << kAttachedToSharedFunctionInfo));
  } else {
    set_bit_field2(bit_field2() & ~(1 << kAttachedToSharedFunctionInfo));
  }
}

bool Map::attached_to_shared_function_info() {
  return ((1 << kAttachedToSharedFunctionInfo) & bit_field2()) != 0;
}


void Map::set_is_shared(bool value) {
  if (value) {
    set_bit_field3(bit_field3() | (1 << kIsShared));
  } else {
    set_bit_field3(bit_field3() & ~(1 << kIsShared));
  }
}

bool Map::is_shared() {
  return ((1 << kIsShared) & bit_field3()) != 0;
}


JSFunction* Map::unchecked_constructor() {
  return reinterpret_cast<JSFunction*>(READ_FIELD(this, kConstructorOffset));
}


FixedArray* Map::unchecked_prototype_transitions() {
  return reinterpret_cast<FixedArray*>(
      READ_FIELD(this, kPrototypeTransitionsOffset));
}


Code::Flags Code::flags() {
  return static_cast<Flags>(READ_INT_FIELD(this, kFlagsOffset));
}


void Code::set_flags(Code::Flags flags) {
  STATIC_ASSERT(Code::NUMBER_OF_KINDS <= KindField::kMax + 1);
  // Make sure that all call stubs have an arguments count.
  ASSERT((ExtractKindFromFlags(flags) != CALL_IC &&
          ExtractKindFromFlags(flags) != KEYED_CALL_IC) ||
         ExtractArgumentsCountFromFlags(flags) >= 0);
  WRITE_INT_FIELD(this, kFlagsOffset, flags);
}


Code::Kind Code::kind() {
  return ExtractKindFromFlags(flags());
}


InlineCacheState Code::ic_state() {
  InlineCacheState result = ExtractICStateFromFlags(flags());
  // Only allow uninitialized or debugger states for non-IC code
  // objects. This is used in the debugger to determine whether or not
  // a call to code object has been replaced with a debug break call.
  ASSERT(is_inline_cache_stub() ||
         result == UNINITIALIZED ||
         result == DEBUG_BREAK ||
         result == DEBUG_PREPARE_STEP_IN);
  return result;
}


Code::ExtraICState Code::extra_ic_state() {
  ASSERT(is_inline_cache_stub());
  return ExtractExtraICStateFromFlags(flags());
}


PropertyType Code::type() {
  return ExtractTypeFromFlags(flags());
}


int Code::arguments_count() {
  ASSERT(is_call_stub() || is_keyed_call_stub() || kind() == STUB);
  return ExtractArgumentsCountFromFlags(flags());
}


int Code::major_key() {
  ASSERT(kind() == STUB ||
         kind() == UNARY_OP_IC ||
         kind() == BINARY_OP_IC ||
         kind() == COMPARE_IC ||
         kind() == TO_BOOLEAN_IC);
  return READ_BYTE_FIELD(this, kStubMajorKeyOffset);
}


void Code::set_major_key(int major) {
  ASSERT(kind() == STUB ||
         kind() == UNARY_OP_IC ||
         kind() == BINARY_OP_IC ||
         kind() == COMPARE_IC ||
         kind() == TO_BOOLEAN_IC);
  ASSERT(0 <= major && major < 256);
  WRITE_BYTE_FIELD(this, kStubMajorKeyOffset, major);
}


bool Code::optimizable() {
  ASSERT(kind() == FUNCTION);
  return READ_BYTE_FIELD(this, kOptimizableOffset) == 1;
}


void Code::set_optimizable(bool value) {
  ASSERT(kind() == FUNCTION);
  WRITE_BYTE_FIELD(this, kOptimizableOffset, value ? 1 : 0);
}


bool Code::has_deoptimization_support() {
  ASSERT(kind() == FUNCTION);
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasDeoptimizationSupportField::decode(flags);
}


void Code::set_has_deoptimization_support(bool value) {
  ASSERT(kind() == FUNCTION);
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasDeoptimizationSupportField::update(flags, value);
  WRITE_BYTE_FIELD(this, kFullCodeFlags, flags);
}


bool Code::has_debug_break_slots() {
  ASSERT(kind() == FUNCTION);
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasDebugBreakSlotsField::decode(flags);
}


void Code::set_has_debug_break_slots(bool value) {
  ASSERT(kind() == FUNCTION);
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasDebugBreakSlotsField::update(flags, value);
  WRITE_BYTE_FIELD(this, kFullCodeFlags, flags);
}


int Code::allow_osr_at_loop_nesting_level() {
  ASSERT(kind() == FUNCTION);
  return READ_BYTE_FIELD(this, kAllowOSRAtLoopNestingLevelOffset);
}


void Code::set_allow_osr_at_loop_nesting_level(int level) {
  ASSERT(kind() == FUNCTION);
  ASSERT(level >= 0 && level <= kMaxLoopNestingMarker);
  WRITE_BYTE_FIELD(this, kAllowOSRAtLoopNestingLevelOffset, level);
}


unsigned Code::stack_slots() {
  ASSERT(kind() == OPTIMIZED_FUNCTION);
  return READ_UINT32_FIELD(this, kStackSlotsOffset);
}


void Code::set_stack_slots(unsigned slots) {
  ASSERT(kind() == OPTIMIZED_FUNCTION);
  WRITE_UINT32_FIELD(this, kStackSlotsOffset, slots);
}


unsigned Code::safepoint_table_offset() {
  ASSERT(kind() == OPTIMIZED_FUNCTION);
  return READ_UINT32_FIELD(this, kSafepointTableOffsetOffset);
}


void Code::set_safepoint_table_offset(unsigned offset) {
  ASSERT(kind() == OPTIMIZED_FUNCTION);
  ASSERT(IsAligned(offset, static_cast<unsigned>(kIntSize)));
  WRITE_UINT32_FIELD(this, kSafepointTableOffsetOffset, offset);
}


unsigned Code::stack_check_table_offset() {
  ASSERT(kind() == FUNCTION);
  return READ_UINT32_FIELD(this, kStackCheckTableOffsetOffset);
}


void Code::set_stack_check_table_offset(unsigned offset) {
  ASSERT(kind() == FUNCTION);
  ASSERT(IsAligned(offset, static_cast<unsigned>(kIntSize)));
  WRITE_UINT32_FIELD(this, kStackCheckTableOffsetOffset, offset);
}


CheckType Code::check_type() {
  ASSERT(is_call_stub() || is_keyed_call_stub());
  byte type = READ_BYTE_FIELD(this, kCheckTypeOffset);
  return static_cast<CheckType>(type);
}


void Code::set_check_type(CheckType value) {
  ASSERT(is_call_stub() || is_keyed_call_stub());
  WRITE_BYTE_FIELD(this, kCheckTypeOffset, value);
}


byte Code::unary_op_type() {
  ASSERT(is_unary_op_stub());
  return READ_BYTE_FIELD(this, kUnaryOpTypeOffset);
}


void Code::set_unary_op_type(byte value) {
  ASSERT(is_unary_op_stub());
  WRITE_BYTE_FIELD(this, kUnaryOpTypeOffset, value);
}


byte Code::binary_op_type() {
  ASSERT(is_binary_op_stub());
  return READ_BYTE_FIELD(this, kBinaryOpTypeOffset);
}


void Code::set_binary_op_type(byte value) {
  ASSERT(is_binary_op_stub());
  WRITE_BYTE_FIELD(this, kBinaryOpTypeOffset, value);
}


byte Code::binary_op_result_type() {
  ASSERT(is_binary_op_stub());
  return READ_BYTE_FIELD(this, kBinaryOpReturnTypeOffset);
}


void Code::set_binary_op_result_type(byte value) {
  ASSERT(is_binary_op_stub());
  WRITE_BYTE_FIELD(this, kBinaryOpReturnTypeOffset, value);
}


byte Code::compare_state() {
  ASSERT(is_compare_ic_stub());
  return READ_BYTE_FIELD(this, kCompareStateOffset);
}


void Code::set_compare_state(byte value) {
  ASSERT(is_compare_ic_stub());
  WRITE_BYTE_FIELD(this, kCompareStateOffset, value);
}


byte Code::to_boolean_state() {
  ASSERT(is_to_boolean_ic_stub());
  return READ_BYTE_FIELD(this, kToBooleanTypeOffset);
}


void Code::set_to_boolean_state(byte value) {
  ASSERT(is_to_boolean_ic_stub());
  WRITE_BYTE_FIELD(this, kToBooleanTypeOffset, value);
}

bool Code::is_inline_cache_stub() {
  Kind kind = this->kind();
  return kind >= FIRST_IC_KIND && kind <= LAST_IC_KIND;
}


Code::Flags Code::ComputeFlags(Kind kind,
                               InlineCacheState ic_state,
                               ExtraICState extra_ic_state,
                               PropertyType type,
                               int argc,
                               InlineCacheHolderFlag holder) {
  // Extra IC state is only allowed for call IC stubs or for store IC
  // stubs.
  ASSERT(extra_ic_state == kNoExtraICState ||
         kind == CALL_IC ||
         kind == STORE_IC ||
         kind == KEYED_STORE_IC);
  // Compute the bit mask.
  int bits = KindField::encode(kind)
      | ICStateField::encode(ic_state)
      | TypeField::encode(type)
      | ExtraICStateField::encode(extra_ic_state)
      | (argc << kArgumentsCountShift)
      | CacheHolderField::encode(holder);
  return static_cast<Flags>(bits);
}


Code::Flags Code::ComputeMonomorphicFlags(Kind kind,
                                          PropertyType type,
                                          ExtraICState extra_ic_state,
                                          InlineCacheHolderFlag holder,
                                          int argc) {
  return ComputeFlags(kind, MONOMORPHIC, extra_ic_state, type, argc, holder);
}


Code::Kind Code::ExtractKindFromFlags(Flags flags) {
  return KindField::decode(flags);
}


InlineCacheState Code::ExtractICStateFromFlags(Flags flags) {
  return ICStateField::decode(flags);
}


Code::ExtraICState Code::ExtractExtraICStateFromFlags(Flags flags) {
  return ExtraICStateField::decode(flags);
}


PropertyType Code::ExtractTypeFromFlags(Flags flags) {
  return TypeField::decode(flags);
}


int Code::ExtractArgumentsCountFromFlags(Flags flags) {
  return (flags & kArgumentsCountMask) >> kArgumentsCountShift;
}


InlineCacheHolderFlag Code::ExtractCacheHolderFromFlags(Flags flags) {
  return CacheHolderField::decode(flags);
}


Code::Flags Code::RemoveTypeFromFlags(Flags flags) {
  int bits = flags & ~TypeField::kMask;
  return static_cast<Flags>(bits);
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


Isolate* Map::isolate() {
  return heap()->isolate();
}


Heap* Map::heap() {
  // NOTE: address() helper is not used to save one instruction.
  Heap* heap = Page::FromAddress(reinterpret_cast<Address>(this))->heap_;
  ASSERT(heap != NULL);
  ASSERT(heap->isolate() == Isolate::Current());
  return heap;
}


Heap* Code::heap() {
  // NOTE: address() helper is not used to save one instruction.
  Heap* heap = Page::FromAddress(reinterpret_cast<Address>(this))->heap_;
  ASSERT(heap != NULL);
  ASSERT(heap->isolate() == Isolate::Current());
  return heap;
}


Isolate* Code::isolate() {
  return heap()->isolate();
}


Heap* JSGlobalPropertyCell::heap() {
  // NOTE: address() helper is not used to save one instruction.
  Heap* heap = Page::FromAddress(reinterpret_cast<Address>(this))->heap_;
  ASSERT(heap != NULL);
  ASSERT(heap->isolate() == Isolate::Current());
  return heap;
}


Isolate* JSGlobalPropertyCell::isolate() {
  return heap()->isolate();
}


Object* Code::GetObjectFromEntryAddress(Address location_of_address) {
  return HeapObject::
      FromAddress(Memory::Address_at(location_of_address) - Code::kHeaderSize);
}


Object* Map::prototype() {
  return READ_FIELD(this, kPrototypeOffset);
}


void Map::set_prototype(Object* value, WriteBarrierMode mode) {
  ASSERT(value->IsNull() || value->IsJSReceiver());
  WRITE_FIELD(this, kPrototypeOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kPrototypeOffset, mode);
}


MaybeObject* Map::GetFastElementsMap() {
  if (has_fast_elements()) return this;
  Object* obj;
  { MaybeObject* maybe_obj = CopyDropTransitions();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  Map* new_map = Map::cast(obj);
  new_map->set_elements_kind(FAST_ELEMENTS);
  isolate()->counters()->map_to_fast_elements()->Increment();
  return new_map;
}


MaybeObject* Map::GetFastDoubleElementsMap() {
  if (has_fast_double_elements()) return this;
  Object* obj;
  { MaybeObject* maybe_obj = CopyDropTransitions();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  Map* new_map = Map::cast(obj);
  new_map->set_elements_kind(FAST_DOUBLE_ELEMENTS);
  isolate()->counters()->map_to_fast_double_elements()->Increment();
  return new_map;
}


MaybeObject* Map::GetSlowElementsMap() {
  if (!has_fast_elements() && !has_fast_double_elements()) return this;
  Object* obj;
  { MaybeObject* maybe_obj = CopyDropTransitions();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  Map* new_map = Map::cast(obj);
  new_map->set_elements_kind(DICTIONARY_ELEMENTS);
  isolate()->counters()->map_to_slow_elements()->Increment();
  return new_map;
}


DescriptorArray* Map::instance_descriptors() {
  Object* object = READ_FIELD(this, kInstanceDescriptorsOrBitField3Offset);
  if (object->IsSmi()) {
    return HEAP->empty_descriptor_array();
  } else {
    return DescriptorArray::cast(object);
  }
}


void Map::init_instance_descriptors() {
  WRITE_FIELD(this, kInstanceDescriptorsOrBitField3Offset, Smi::FromInt(0));
}


void Map::clear_instance_descriptors() {
  Object* object = READ_FIELD(this,
                              kInstanceDescriptorsOrBitField3Offset);
  if (!object->IsSmi()) {
    WRITE_FIELD(
        this,
        kInstanceDescriptorsOrBitField3Offset,
        Smi::FromInt(DescriptorArray::cast(object)->bit_field3_storage()));
  }
}


void Map::set_instance_descriptors(DescriptorArray* value,
                                   WriteBarrierMode mode) {
  Object* object = READ_FIELD(this,
                              kInstanceDescriptorsOrBitField3Offset);
  if (value == isolate()->heap()->empty_descriptor_array()) {
    clear_instance_descriptors();
    return;
  } else {
    if (object->IsSmi()) {
      value->set_bit_field3_storage(Smi::cast(object)->value());
    } else {
      value->set_bit_field3_storage(
          DescriptorArray::cast(object)->bit_field3_storage());
    }
  }
  ASSERT(!is_shared());
  WRITE_FIELD(this, kInstanceDescriptorsOrBitField3Offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(),
                            this,
                            kInstanceDescriptorsOrBitField3Offset,
                            mode);
}


int Map::bit_field3() {
  Object* object = READ_FIELD(this,
                              kInstanceDescriptorsOrBitField3Offset);
  if (object->IsSmi()) {
    return Smi::cast(object)->value();
  } else {
    return DescriptorArray::cast(object)->bit_field3_storage();
  }
}


void Map::set_bit_field3(int value) {
  ASSERT(Smi::IsValid(value));
  Object* object = READ_FIELD(this,
                              kInstanceDescriptorsOrBitField3Offset);
  if (object->IsSmi()) {
    WRITE_FIELD(this,
                kInstanceDescriptorsOrBitField3Offset,
                Smi::FromInt(value));
  } else {
    DescriptorArray::cast(object)->set_bit_field3_storage(value);
  }
}


ACCESSORS(Map, code_cache, Object, kCodeCacheOffset)
ACCESSORS(Map, prototype_transitions, FixedArray, kPrototypeTransitionsOffset)
ACCESSORS(Map, constructor, Object, kConstructorOffset)

ACCESSORS(JSFunction, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
ACCESSORS(JSFunction, literals, FixedArray, kLiteralsOffset)
ACCESSORS_GCSAFE(JSFunction, next_function_link, Object,
                 kNextFunctionLinkOffset)

ACCESSORS(GlobalObject, builtins, JSBuiltinsObject, kBuiltinsOffset)
ACCESSORS(GlobalObject, global_context, Context, kGlobalContextOffset)
ACCESSORS(GlobalObject, global_receiver, JSObject, kGlobalReceiverOffset)

ACCESSORS(JSGlobalProxy, context, Object, kContextOffset)

ACCESSORS(AccessorInfo, getter, Object, kGetterOffset)
ACCESSORS(AccessorInfo, setter, Object, kSetterOffset)
ACCESSORS(AccessorInfo, data, Object, kDataOffset)
ACCESSORS(AccessorInfo, name, Object, kNameOffset)
ACCESSORS(AccessorInfo, flag, Smi, kFlagOffset)

ACCESSORS(AccessCheckInfo, named_callback, Object, kNamedCallbackOffset)
ACCESSORS(AccessCheckInfo, indexed_callback, Object, kIndexedCallbackOffset)
ACCESSORS(AccessCheckInfo, data, Object, kDataOffset)

ACCESSORS(InterceptorInfo, getter, Object, kGetterOffset)
ACCESSORS(InterceptorInfo, setter, Object, kSetterOffset)
ACCESSORS(InterceptorInfo, query, Object, kQueryOffset)
ACCESSORS(InterceptorInfo, deleter, Object, kDeleterOffset)
ACCESSORS(InterceptorInfo, enumerator, Object, kEnumeratorOffset)
ACCESSORS(InterceptorInfo, data, Object, kDataOffset)

ACCESSORS(CallHandlerInfo, callback, Object, kCallbackOffset)
ACCESSORS(CallHandlerInfo, data, Object, kDataOffset)

ACCESSORS(TemplateInfo, tag, Object, kTagOffset)
ACCESSORS(TemplateInfo, property_list, Object, kPropertyListOffset)

ACCESSORS(FunctionTemplateInfo, serial_number, Object, kSerialNumberOffset)
ACCESSORS(FunctionTemplateInfo, call_code, Object, kCallCodeOffset)
ACCESSORS(FunctionTemplateInfo, property_accessors, Object,
          kPropertyAccessorsOffset)
ACCESSORS(FunctionTemplateInfo, prototype_template, Object,
          kPrototypeTemplateOffset)
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
ACCESSORS(FunctionTemplateInfo, flag, Smi, kFlagOffset)

ACCESSORS(ObjectTemplateInfo, constructor, Object, kConstructorOffset)
ACCESSORS(ObjectTemplateInfo, internal_field_count, Object,
          kInternalFieldCountOffset)

ACCESSORS(SignatureInfo, receiver, Object, kReceiverOffset)
ACCESSORS(SignatureInfo, args, Object, kArgsOffset)

ACCESSORS(TypeSwitchInfo, types, Object, kTypesOffset)

ACCESSORS(Script, source, Object, kSourceOffset)
ACCESSORS(Script, name, Object, kNameOffset)
ACCESSORS(Script, id, Object, kIdOffset)
ACCESSORS(Script, line_offset, Smi, kLineOffsetOffset)
ACCESSORS(Script, column_offset, Smi, kColumnOffsetOffset)
ACCESSORS(Script, data, Object, kDataOffset)
ACCESSORS(Script, context_data, Object, kContextOffset)
ACCESSORS(Script, wrapper, Foreign, kWrapperOffset)
ACCESSORS(Script, type, Smi, kTypeOffset)
ACCESSORS(Script, compilation_type, Smi, kCompilationTypeOffset)
ACCESSORS(Script, line_ends, Object, kLineEndsOffset)
ACCESSORS(Script, eval_from_shared, Object, kEvalFromSharedOffset)
ACCESSORS(Script, eval_from_instructions_offset, Smi,
          kEvalFrominstructionsOffsetOffset)

#ifdef ENABLE_DEBUGGER_SUPPORT
ACCESSORS(DebugInfo, shared, SharedFunctionInfo, kSharedFunctionInfoIndex)
ACCESSORS(DebugInfo, original_code, Code, kOriginalCodeIndex)
ACCESSORS(DebugInfo, code, Code, kPatchedCodeIndex)
ACCESSORS(DebugInfo, break_points, FixedArray, kBreakPointsStateIndex)

ACCESSORS(BreakPointInfo, code_position, Smi, kCodePositionIndex)
ACCESSORS(BreakPointInfo, source_position, Smi, kSourcePositionIndex)
ACCESSORS(BreakPointInfo, statement_position, Smi, kStatementPositionIndex)
ACCESSORS(BreakPointInfo, break_point_objects, Object, kBreakPointObjectsIndex)
#endif

ACCESSORS(SharedFunctionInfo, name, Object, kNameOffset)
ACCESSORS_GCSAFE(SharedFunctionInfo, construct_stub, Code, kConstructStubOffset)
ACCESSORS_GCSAFE(SharedFunctionInfo, initial_map, Object, kInitialMapOffset)
ACCESSORS(SharedFunctionInfo, instance_class_name, Object,
          kInstanceClassNameOffset)
ACCESSORS(SharedFunctionInfo, function_data, Object, kFunctionDataOffset)
ACCESSORS(SharedFunctionInfo, script, Object, kScriptOffset)
ACCESSORS(SharedFunctionInfo, debug_info, Object, kDebugInfoOffset)
ACCESSORS(SharedFunctionInfo, inferred_name, String, kInferredNameOffset)
ACCESSORS(SharedFunctionInfo, this_property_assignments, Object,
          kThisPropertyAssignmentsOffset)

BOOL_ACCESSORS(FunctionTemplateInfo, flag, hidden_prototype,
               kHiddenPrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, undetectable, kUndetectableBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, needs_access_check,
               kNeedsAccessCheckBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, read_only_prototype,
               kReadOnlyPrototypeBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_expression,
               kIsExpressionBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_toplevel,
               kIsTopLevelBit)
BOOL_GETTER(SharedFunctionInfo,
            compiler_hints,
            has_only_simple_this_property_assignments,
            kHasOnlySimpleThisPropertyAssignments)
BOOL_ACCESSORS(SharedFunctionInfo,
               compiler_hints,
               allows_lazy_compilation,
               kAllowLazyCompilation)
BOOL_ACCESSORS(SharedFunctionInfo,
               compiler_hints,
               uses_arguments,
               kUsesArguments)
BOOL_ACCESSORS(SharedFunctionInfo,
               compiler_hints,
               has_duplicate_parameters,
               kHasDuplicateParameters)


#if V8_HOST_ARCH_32_BIT
SMI_ACCESSORS(SharedFunctionInfo, length, kLengthOffset)
SMI_ACCESSORS(SharedFunctionInfo, formal_parameter_count,
              kFormalParameterCountOffset)
SMI_ACCESSORS(SharedFunctionInfo, expected_nof_properties,
              kExpectedNofPropertiesOffset)
SMI_ACCESSORS(SharedFunctionInfo, num_literals, kNumLiteralsOffset)
SMI_ACCESSORS(SharedFunctionInfo, start_position_and_type,
              kStartPositionAndTypeOffset)
SMI_ACCESSORS(SharedFunctionInfo, end_position, kEndPositionOffset)
SMI_ACCESSORS(SharedFunctionInfo, function_token_position,
              kFunctionTokenPositionOffset)
SMI_ACCESSORS(SharedFunctionInfo, compiler_hints,
              kCompilerHintsOffset)
SMI_ACCESSORS(SharedFunctionInfo, this_property_assignments_count,
              kThisPropertyAssignmentsCountOffset)
SMI_ACCESSORS(SharedFunctionInfo, opt_count, kOptCountOffset)
#else

#define PSEUDO_SMI_ACCESSORS_LO(holder, name, offset)             \
  STATIC_ASSERT(holder::offset % kPointerSize == 0);              \
  int holder::name() {                                            \
    int value = READ_INT_FIELD(this, offset);                     \
    ASSERT(kHeapObjectTag == 1);                                  \
    ASSERT((value & kHeapObjectTag) == 0);                        \
    return value >> 1;                                            \
  }                                                               \
  void holder::set_##name(int value) {                            \
    ASSERT(kHeapObjectTag == 1);                                  \
    ASSERT((value & 0xC0000000) == 0xC0000000 ||                  \
           (value & 0xC0000000) == 0x000000000);                  \
    WRITE_INT_FIELD(this,                                         \
                    offset,                                       \
                    (value << 1) & ~kHeapObjectTag);              \
  }

#define PSEUDO_SMI_ACCESSORS_HI(holder, name, offset)             \
  STATIC_ASSERT(holder::offset % kPointerSize == kIntSize);       \
  INT_ACCESSORS(holder, name, offset)


PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo, length, kLengthOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo,
                        formal_parameter_count,
                        kFormalParameterCountOffset)

PSEUDO_SMI_ACCESSORS_LO(SharedFunctionInfo,
                        expected_nof_properties,
                        kExpectedNofPropertiesOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo, num_literals, kNumLiteralsOffset)

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
                        this_property_assignments_count,
                        kThisPropertyAssignmentsCountOffset)
PSEUDO_SMI_ACCESSORS_HI(SharedFunctionInfo, opt_count, kOptCountOffset)
#endif


int SharedFunctionInfo::construction_count() {
  return READ_BYTE_FIELD(this, kConstructionCountOffset);
}


void SharedFunctionInfo::set_construction_count(int value) {
  ASSERT(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this, kConstructionCountOffset, static_cast<byte>(value));
}


BOOL_ACCESSORS(SharedFunctionInfo,
               compiler_hints,
               live_objects_may_exist,
               kLiveObjectsMayExist)


bool SharedFunctionInfo::IsInobjectSlackTrackingInProgress() {
  return initial_map() != HEAP->undefined_value();
}


BOOL_GETTER(SharedFunctionInfo,
            compiler_hints,
            optimization_disabled,
            kOptimizationDisabled)


void SharedFunctionInfo::set_optimization_disabled(bool disable) {
  set_compiler_hints(BooleanBit::set(compiler_hints(),
                                     kOptimizationDisabled,
                                     disable));
  // If disabling optimizations we reflect that in the code object so
  // it will not be counted as optimizable code.
  if ((code()->kind() == Code::FUNCTION) && disable) {
    code()->set_optimizable(false);
  }
}


BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, strict_mode,
               kStrictModeFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, native, kNative)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints,
               name_should_print_as_anonymous,
               kNameShouldPrintAsAnonymous)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, bound, kBoundFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_anonymous, kIsAnonymous)

ACCESSORS(CodeCache, default_cache, FixedArray, kDefaultCacheOffset)
ACCESSORS(CodeCache, normal_type_cache, Object, kNormalTypeCacheOffset)

ACCESSORS(PolymorphicCodeCache, cache, Object, kCacheOffset)

bool Script::HasValidSource() {
  Object* src = this->source();
  if (!src->IsString()) return true;
  String* src_str = String::cast(src);
  if (!StringShape(src_str).IsExternal()) return true;
  if (src_str->IsAsciiRepresentation()) {
    return ExternalAsciiString::cast(src)->resource() != NULL;
  } else if (src_str->IsTwoByteRepresentation()) {
    return ExternalTwoByteString::cast(src)->resource() != NULL;
  }
  return true;
}


void SharedFunctionInfo::DontAdaptArguments() {
  ASSERT(code()->kind() == Code::BUILTIN);
  set_formal_parameter_count(kDontAdaptArgumentsSentinel);
}


int SharedFunctionInfo::start_position() {
  return start_position_and_type() >> kStartPositionShift;
}


void SharedFunctionInfo::set_start_position(int start_position) {
  set_start_position_and_type((start_position << kStartPositionShift)
    | (start_position_and_type() & ~kStartPositionMask));
}


Code* SharedFunctionInfo::code() {
  return Code::cast(READ_FIELD(this, kCodeOffset));
}


Code* SharedFunctionInfo::unchecked_code() {
  return reinterpret_cast<Code*>(READ_FIELD(this, kCodeOffset));
}


void SharedFunctionInfo::set_code(Code* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kCodeOffset, value);
  ASSERT(!Isolate::Current()->heap()->InNewSpace(value));
}


SerializedScopeInfo* SharedFunctionInfo::scope_info() {
  return reinterpret_cast<SerializedScopeInfo*>(
      READ_FIELD(this, kScopeInfoOffset));
}


void SharedFunctionInfo::set_scope_info(SerializedScopeInfo* value,
                                        WriteBarrierMode mode) {
  WRITE_FIELD(this, kScopeInfoOffset, reinterpret_cast<Object*>(value));
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kScopeInfoOffset, mode);
}


Smi* SharedFunctionInfo::deopt_counter() {
  return reinterpret_cast<Smi*>(READ_FIELD(this, kDeoptCounterOffset));
}


void SharedFunctionInfo::set_deopt_counter(Smi* value) {
  WRITE_FIELD(this, kDeoptCounterOffset, value);
}


bool SharedFunctionInfo::is_compiled() {
  return code() !=
      Isolate::Current()->builtins()->builtin(Builtins::kLazyCompile);
}


bool SharedFunctionInfo::IsApiFunction() {
  return function_data()->IsFunctionTemplateInfo();
}


FunctionTemplateInfo* SharedFunctionInfo::get_api_func_data() {
  ASSERT(IsApiFunction());
  return FunctionTemplateInfo::cast(function_data());
}


bool SharedFunctionInfo::HasBuiltinFunctionId() {
  return function_data()->IsSmi();
}


BuiltinFunctionId SharedFunctionInfo::builtin_function_id() {
  ASSERT(HasBuiltinFunctionId());
  return static_cast<BuiltinFunctionId>(Smi::cast(function_data())->value());
}


int SharedFunctionInfo::code_age() {
  return (compiler_hints() >> kCodeAgeShift) & kCodeAgeMask;
}


void SharedFunctionInfo::set_code_age(int code_age) {
  set_compiler_hints(compiler_hints() |
                     ((code_age & kCodeAgeMask) << kCodeAgeShift));
}


bool SharedFunctionInfo::has_deoptimization_support() {
  Code* code = this->code();
  return code->kind() == Code::FUNCTION && code->has_deoptimization_support();
}


bool JSFunction::IsBuiltin() {
  return context()->global()->IsJSBuiltinsObject();
}


bool JSFunction::NeedsArgumentsAdaption() {
  return shared()->formal_parameter_count() !=
      SharedFunctionInfo::kDontAdaptArgumentsSentinel;
}


bool JSFunction::IsOptimized() {
  return code()->kind() == Code::OPTIMIZED_FUNCTION;
}


bool JSFunction::IsOptimizable() {
  return code()->kind() == Code::FUNCTION && code()->optimizable();
}


bool JSFunction::IsMarkedForLazyRecompilation() {
  return code() == GetIsolate()->builtins()->builtin(Builtins::kLazyRecompile);
}


Code* JSFunction::code() {
  return Code::cast(unchecked_code());
}


Code* JSFunction::unchecked_code() {
  return reinterpret_cast<Code*>(
      Code::GetObjectFromEntryAddress(FIELD_ADDR(this, kCodeEntryOffset)));
}


void JSFunction::set_code(Code* value) {
  // Skip the write barrier because code is never in new space.
  ASSERT(!HEAP->InNewSpace(value));
  Address entry = value->entry();
  WRITE_INTPTR_FIELD(this, kCodeEntryOffset, reinterpret_cast<intptr_t>(entry));
}


void JSFunction::ReplaceCode(Code* code) {
  bool was_optimized = IsOptimized();
  bool is_optimized = code->kind() == Code::OPTIMIZED_FUNCTION;

  set_code(code);

  // Add/remove the function from the list of optimized functions for this
  // context based on the state change.
  if (!was_optimized && is_optimized) {
    context()->global_context()->AddOptimizedFunction(this);
  }
  if (was_optimized && !is_optimized) {
    context()->global_context()->RemoveOptimizedFunction(this);
  }
}


Context* JSFunction::context() {
  return Context::cast(READ_FIELD(this, kContextOffset));
}


Object* JSFunction::unchecked_context() {
  return READ_FIELD(this, kContextOffset);
}


SharedFunctionInfo* JSFunction::unchecked_shared() {
  return reinterpret_cast<SharedFunctionInfo*>(
      READ_FIELD(this, kSharedFunctionInfoOffset));
}


void JSFunction::set_context(Object* value) {
  ASSERT(value->IsUndefined() || value->IsContext());
  WRITE_FIELD(this, kContextOffset, value);
  WRITE_BARRIER(this, kContextOffset);
}

ACCESSORS(JSFunction, prototype_or_initial_map, Object,
          kPrototypeOrInitialMapOffset)


Map* JSFunction::initial_map() {
  return Map::cast(prototype_or_initial_map());
}


void JSFunction::set_initial_map(Map* value) {
  set_prototype_or_initial_map(value);
}


bool JSFunction::has_initial_map() {
  return prototype_or_initial_map()->IsMap();
}


bool JSFunction::has_instance_prototype() {
  return has_initial_map() || !prototype_or_initial_map()->IsTheHole();
}


bool JSFunction::has_prototype() {
  return map()->has_non_instance_prototype() || has_instance_prototype();
}


Object* JSFunction::instance_prototype() {
  ASSERT(has_instance_prototype());
  if (has_initial_map()) return initial_map()->prototype();
  // When there is no initial map and the prototype is a JSObject, the
  // initial map field is used for the prototype field.
  return prototype_or_initial_map();
}


Object* JSFunction::prototype() {
  ASSERT(has_prototype());
  // If the function's prototype property has been set to a non-JSObject
  // value, that value is stored in the constructor field of the map.
  if (map()->has_non_instance_prototype()) return map()->constructor();
  return instance_prototype();
}

bool JSFunction::should_have_prototype() {
  return map()->function_with_prototype();
}


bool JSFunction::is_compiled() {
  return code() != GetIsolate()->builtins()->builtin(Builtins::kLazyCompile);
}


int JSFunction::NumberOfLiterals() {
  return literals()->length();
}


Object* JSBuiltinsObject::javascript_builtin(Builtins::JavaScript id) {
  ASSERT(id < kJSBuiltinsCount);  // id is unsigned.
  return READ_FIELD(this, OffsetOfFunctionWithId(id));
}


void JSBuiltinsObject::set_javascript_builtin(Builtins::JavaScript id,
                                              Object* value) {
  ASSERT(id < kJSBuiltinsCount);  // id is unsigned.
  WRITE_FIELD(this, OffsetOfFunctionWithId(id), value);
  WRITE_BARRIER(this, OffsetOfFunctionWithId(id));
}


Code* JSBuiltinsObject::javascript_builtin_code(Builtins::JavaScript id) {
  ASSERT(id < kJSBuiltinsCount);  // id is unsigned.
  return Code::cast(READ_FIELD(this, OffsetOfCodeWithId(id)));
}


void JSBuiltinsObject::set_javascript_builtin_code(Builtins::JavaScript id,
                                                   Code* value) {
  ASSERT(id < kJSBuiltinsCount);  // id is unsigned.
  WRITE_FIELD(this, OffsetOfCodeWithId(id), value);
  ASSERT(!HEAP->InNewSpace(value));
}


ACCESSORS(JSProxy, handler, Object, kHandlerOffset)
ACCESSORS(JSFunctionProxy, call_trap, Object, kCallTrapOffset)
ACCESSORS(JSFunctionProxy, construct_trap, Object, kConstructTrapOffset)


void JSProxy::InitializeBody(int object_size, Object* value) {
  ASSERT(!value->IsHeapObject() || !GetHeap()->InNewSpace(value));
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}


ACCESSORS(JSWeakMap, table, ObjectHashTable, kTableOffset)
ACCESSORS_GCSAFE(JSWeakMap, next, Object, kNextOffset)


ObjectHashTable* JSWeakMap::unchecked_table() {
  return reinterpret_cast<ObjectHashTable*>(READ_FIELD(this, kTableOffset));
}


Address Foreign::address() {
  return AddressFrom<Address>(READ_INTPTR_FIELD(this, kAddressOffset));
}


void Foreign::set_address(Address value) {
  WRITE_INTPTR_FIELD(this, kAddressOffset, OffsetFrom(value));
}


ACCESSORS(JSValue, value, Object, kValueOffset)


JSValue* JSValue::cast(Object* obj) {
  ASSERT(obj->IsJSValue());
  ASSERT(HeapObject::cast(obj)->Size() == JSValue::kSize);
  return reinterpret_cast<JSValue*>(obj);
}


ACCESSORS(JSMessageObject, type, String, kTypeOffset)
ACCESSORS(JSMessageObject, arguments, JSArray, kArgumentsOffset)
ACCESSORS(JSMessageObject, script, Object, kScriptOffset)
ACCESSORS(JSMessageObject, stack_trace, Object, kStackTraceOffset)
ACCESSORS(JSMessageObject, stack_frames, Object, kStackFramesOffset)
SMI_ACCESSORS(JSMessageObject, start_position, kStartPositionOffset)
SMI_ACCESSORS(JSMessageObject, end_position, kEndPositionOffset)


JSMessageObject* JSMessageObject::cast(Object* obj) {
  ASSERT(obj->IsJSMessageObject());
  ASSERT(HeapObject::cast(obj)->Size() == JSMessageObject::kSize);
  return reinterpret_cast<JSMessageObject*>(obj);
}


INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
ACCESSORS(Code, relocation_info, ByteArray, kRelocationInfoOffset)
ACCESSORS(Code, deoptimization_data, FixedArray, kDeoptimizationDataOffset)
ACCESSORS(Code, next_code_flushing_candidate,
          Object, kNextCodeFlushingCandidateOffset)


byte* Code::instruction_start()  {
  return FIELD_ADDR(this, kHeaderSize);
}


byte* Code::instruction_end()  {
  return instruction_start() + instruction_size();
}


int Code::body_size() {
  return RoundUp(instruction_size(), kObjectAlignment);
}


FixedArray* Code::unchecked_deoptimization_data() {
  return reinterpret_cast<FixedArray*>(
      READ_FIELD(this, kDeoptimizationDataOffset));
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


bool Code::contains(byte* pc) {
  return (instruction_start() <= pc) &&
      (pc <= instruction_start() + instruction_size());
}


ACCESSORS(JSArray, length, Object, kLengthOffset)


ACCESSORS(JSRegExp, data, Object, kDataOffset)


JSRegExp::Type JSRegExp::TypeTag() {
  Object* data = this->data();
  if (data->IsUndefined()) return JSRegExp::NOT_COMPILED;
  Smi* smi = Smi::cast(FixedArray::cast(data)->get(kTagIndex));
  return static_cast<JSRegExp::Type>(smi->value());
}


JSRegExp::Type JSRegExp::TypeTagUnchecked() {
  Smi* smi = Smi::cast(DataAtUnchecked(kTagIndex));
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
  ASSERT(this->data()->IsFixedArray());
  Object* data = this->data();
  Smi* smi = Smi::cast(FixedArray::cast(data)->get(kFlagsIndex));
  return Flags(smi->value());
}


String* JSRegExp::Pattern() {
  ASSERT(this->data()->IsFixedArray());
  Object* data = this->data();
  String* pattern= String::cast(FixedArray::cast(data)->get(kSourceIndex));
  return pattern;
}


Object* JSRegExp::DataAt(int index) {
  ASSERT(TypeTag() != NOT_COMPILED);
  return FixedArray::cast(data())->get(index);
}


Object* JSRegExp::DataAtUnchecked(int index) {
  FixedArray* fa = reinterpret_cast<FixedArray*>(data());
  int offset = FixedArray::kHeaderSize + index * kPointerSize;
  return READ_FIELD(fa, offset);
}


void JSRegExp::SetDataAt(int index, Object* value) {
  ASSERT(TypeTag() != NOT_COMPILED);
  ASSERT(index >= kDataIndex);  // Only implementation data can be set this way.
  FixedArray::cast(data())->set(index, value);
}


void JSRegExp::SetDataAtUnchecked(int index, Object* value, Heap* heap) {
  ASSERT(index >= kDataIndex);  // Only implementation data can be set this way.
  FixedArray* fa = reinterpret_cast<FixedArray*>(data());
  if (value->IsSmi()) {
    fa->set_unchecked(index, Smi::cast(value));
  } else {
    fa->set_unchecked(heap, index, value, SKIP_WRITE_BARRIER);
  }
}


ElementsKind JSObject::GetElementsKind() {
  ElementsKind kind = map()->elements_kind();
  ASSERT((kind == FAST_ELEMENTS &&
          (elements()->map() == GetHeap()->fixed_array_map() ||
           elements()->map() == GetHeap()->fixed_cow_array_map())) ||
         (kind == FAST_DOUBLE_ELEMENTS &&
          elements()->IsFixedDoubleArray()) ||
         (kind == DICTIONARY_ELEMENTS &&
          elements()->IsFixedArray() &&
          elements()->IsDictionary()) ||
         (kind > DICTIONARY_ELEMENTS));
  return kind;
}


ElementsAccessor* JSObject::GetElementsAccessor() {
  return ElementsAccessor::ForKind(GetElementsKind());
}


bool JSObject::HasFastElements() {
  return GetElementsKind() == FAST_ELEMENTS;
}


bool JSObject::HasFastDoubleElements() {
  return GetElementsKind() == FAST_DOUBLE_ELEMENTS;
}


bool JSObject::HasDictionaryElements() {
  return GetElementsKind() == DICTIONARY_ELEMENTS;
}


bool JSObject::HasExternalArrayElements() {
  HeapObject* array = elements();
  ASSERT(array != NULL);
  return array->IsExternalArray();
}


#define EXTERNAL_ELEMENTS_CHECK(name, type)          \
bool JSObject::HasExternal##name##Elements() {       \
  HeapObject* array = elements();                    \
  ASSERT(array != NULL);                             \
  if (!array->IsHeapObject())                        \
    return false;                                    \
  return array->map()->instance_type() == type;      \
}


EXTERNAL_ELEMENTS_CHECK(Byte, EXTERNAL_BYTE_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(UnsignedByte, EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(Short, EXTERNAL_SHORT_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(UnsignedShort,
                        EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(Int, EXTERNAL_INT_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(UnsignedInt,
                        EXTERNAL_UNSIGNED_INT_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(Float,
                        EXTERNAL_FLOAT_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(Double,
                        EXTERNAL_DOUBLE_ARRAY_TYPE)
EXTERNAL_ELEMENTS_CHECK(Pixel, EXTERNAL_PIXEL_ARRAY_TYPE)


bool JSObject::HasNamedInterceptor() {
  return map()->has_named_interceptor();
}


bool JSObject::HasIndexedInterceptor() {
  return map()->has_indexed_interceptor();
}


bool JSObject::AllowsSetElementsLength() {
  bool result = elements()->IsFixedArray() ||
      elements()->IsFixedDoubleArray();
  ASSERT(result == !HasExternalArrayElements());
  return result;
}


MaybeObject* JSObject::EnsureWritableFastElements() {
  ASSERT(HasFastElements());
  FixedArray* elems = FixedArray::cast(elements());
  Isolate* isolate = GetIsolate();
  if (elems->map() != isolate->heap()->fixed_cow_array_map()) return elems;
  Object* writable_elems;
  { MaybeObject* maybe_writable_elems = isolate->heap()->CopyFixedArrayWithMap(
      elems, isolate->heap()->fixed_array_map());
    if (!maybe_writable_elems->ToObject(&writable_elems)) {
      return maybe_writable_elems;
    }
  }
  set_elements(FixedArray::cast(writable_elems));
  isolate->counters()->cow_arrays_converted()->Increment();
  return writable_elems;
}


StringDictionary* JSObject::property_dictionary() {
  ASSERT(!HasFastProperties());
  return StringDictionary::cast(properties());
}


SeededNumberDictionary* JSObject::element_dictionary() {
  ASSERT(HasDictionaryElements());
  return SeededNumberDictionary::cast(elements());
}


bool String::IsHashFieldComputed(uint32_t field) {
  return (field & kHashNotComputedMask) == 0;
}


bool String::HasHashCode() {
  return IsHashFieldComputed(hash_field());
}


uint32_t String::Hash() {
  // Fast case: has hash code already been computed?
  uint32_t field = hash_field();
  if (IsHashFieldComputed(field)) return field >> kHashShift;
  // Slow case: compute hash code and set it.
  return ComputeAndSetHash();
}


StringHasher::StringHasher(int length, uint32_t seed)
  : length_(length),
    raw_running_hash_(seed),
    array_index_(0),
    is_array_index_(0 < length_ && length_ <= String::kMaxArrayIndexSize),
    is_first_char_(true),
    is_valid_(true) {
  ASSERT(FLAG_randomize_hashes || raw_running_hash_ == 0);
}


bool StringHasher::has_trivial_hash() {
  return length_ > String::kMaxHashCalcLength;
}


void StringHasher::AddCharacter(uc32 c) {
  // Use the Jenkins one-at-a-time hash function to update the hash
  // for the given character.
  raw_running_hash_ += c;
  raw_running_hash_ += (raw_running_hash_ << 10);
  raw_running_hash_ ^= (raw_running_hash_ >> 6);
  // Incremental array index computation.
  if (is_array_index_) {
    if (c < '0' || c > '9') {
      is_array_index_ = false;
    } else {
      int d = c - '0';
      if (is_first_char_) {
        is_first_char_ = false;
        if (c == '0' && length_ > 1) {
          is_array_index_ = false;
          return;
        }
      }
      if (array_index_ > 429496729U - ((d + 2) >> 3)) {
        is_array_index_ = false;
      } else {
        array_index_ = array_index_ * 10 + d;
      }
    }
  }
}


void StringHasher::AddCharacterNoIndex(uc32 c) {
  ASSERT(!is_array_index());
  raw_running_hash_ += c;
  raw_running_hash_ += (raw_running_hash_ << 10);
  raw_running_hash_ ^= (raw_running_hash_ >> 6);
}


uint32_t StringHasher::GetHash() {
  // Get the calculated raw hash value and do some more bit ops to distribute
  // the hash further. Ensure that we never return zero as the hash value.
  uint32_t result = raw_running_hash_;
  result += (result << 3);
  result ^= (result >> 11);
  result += (result << 15);
  if ((result & String::kHashBitMask) == 0) {
    result = 27;
  }
  return result;
}


template <typename schar>
uint32_t HashSequentialString(const schar* chars, int length, uint32_t seed) {
  StringHasher hasher(length, seed);
  if (!hasher.has_trivial_hash()) {
    int i;
    for (i = 0; hasher.is_array_index() && (i < length); i++) {
      hasher.AddCharacter(chars[i]);
    }
    for (; i < length; i++) {
      hasher.AddCharacterNoIndex(chars[i]);
    }
  }
  return hasher.GetHashField();
}


bool String::AsArrayIndex(uint32_t* index) {
  uint32_t field = hash_field();
  if (IsHashFieldComputed(field) && (field & kIsNotArrayIndexMask)) {
    return false;
  }
  return SlowAsArrayIndex(index);
}


Object* JSReceiver::GetPrototype() {
  return HeapObject::cast(this)->map()->prototype();
}


bool JSReceiver::HasProperty(String* name) {
  if (IsJSProxy()) {
    return JSProxy::cast(this)->HasPropertyWithHandler(name);
  }
  return GetPropertyAttribute(name) != ABSENT;
}


bool JSReceiver::HasLocalProperty(String* name) {
  if (IsJSProxy()) {
    return JSProxy::cast(this)->HasPropertyWithHandler(name);
  }
  return GetLocalPropertyAttribute(name) != ABSENT;
}


PropertyAttributes JSReceiver::GetPropertyAttribute(String* key) {
  return GetPropertyAttributeWithReceiver(this, key);
}

// TODO(504): this may be useful in other places too where JSGlobalProxy
// is used.
Object* JSObject::BypassGlobalProxy() {
  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return GetHeap()->undefined_value();
    ASSERT(proto->IsJSGlobalObject());
    return proto;
  }
  return this;
}


bool JSObject::HasHiddenPropertiesObject() {
  ASSERT(!IsJSGlobalProxy());
  return GetPropertyAttributePostInterceptor(this,
                                             GetHeap()->hidden_symbol(),
                                             false) != ABSENT;
}


Object* JSObject::GetHiddenPropertiesObject() {
  ASSERT(!IsJSGlobalProxy());
  PropertyAttributes attributes;
  // You can't install a getter on a property indexed by the hidden symbol,
  // so we can be sure that GetLocalPropertyPostInterceptor returns a real
  // object.
  Object* result =
      GetLocalPropertyPostInterceptor(this,
                                      GetHeap()->hidden_symbol(),
                                      &attributes)->ToObjectUnchecked();
  return result;
}


MaybeObject* JSObject::SetHiddenPropertiesObject(Object* hidden_obj) {
  ASSERT(!IsJSGlobalProxy());
  return SetPropertyPostInterceptor(GetHeap()->hidden_symbol(),
                                    hidden_obj,
                                    DONT_ENUM,
                                    kNonStrictMode);
}


bool JSObject::HasHiddenProperties() {
  return !GetHiddenProperties(OMIT_CREATION)->ToObjectChecked()->IsUndefined();
}


bool JSObject::HasElement(uint32_t index) {
  return HasElementWithReceiver(this, index);
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


bool AccessorInfo::prohibits_overwriting() {
  return BooleanBit::get(flag(), kProhibitsOverwritingBit);
}


void AccessorInfo::set_prohibits_overwriting(bool value) {
  set_flag(BooleanBit::set(flag(), kProhibitsOverwritingBit, value));
}


PropertyAttributes AccessorInfo::property_attributes() {
  return AttributesField::decode(static_cast<uint32_t>(flag()->value()));
}


void AccessorInfo::set_property_attributes(PropertyAttributes attributes) {
  set_flag(Smi::FromInt(AttributesField::update(flag()->value(), attributes)));
}


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::SetEntry(int entry,
                                      Object* key,
                                      Object* value) {
  SetEntry(entry, key, value, PropertyDetails(Smi::FromInt(0)));
}


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::SetEntry(int entry,
                                      Object* key,
                                      Object* value,
                                      PropertyDetails details) {
  ASSERT(!key->IsString() || details.IsDeleted() || details.index() > 0);
  int index = HashTable<Shape, Key>::EntryToIndex(entry);
  AssertNoAllocation no_gc;
  WriteBarrierMode mode = FixedArray::GetWriteBarrierMode(no_gc);
  FixedArray::set(index, key, mode);
  FixedArray::set(index+1, value, mode);
  FixedArray::fast_set(this, index+2, details.AsSmi());
}


bool NumberDictionaryShape::IsMatch(uint32_t key, Object* other) {
  ASSERT(other->IsNumber());
  return key == static_cast<uint32_t>(other->Number());
}


uint32_t UnseededNumberDictionaryShape::Hash(uint32_t key) {
  return ComputeIntegerHash(key, 0);
}


uint32_t UnseededNumberDictionaryShape::HashForObject(uint32_t key,
                                                      Object* other) {
  ASSERT(other->IsNumber());
  return ComputeIntegerHash(static_cast<uint32_t>(other->Number()), 0);
}

uint32_t SeededNumberDictionaryShape::SeededHash(uint32_t key, uint32_t seed) {
  return ComputeIntegerHash(key, seed);
}

uint32_t SeededNumberDictionaryShape::SeededHashForObject(uint32_t key,
                                                          uint32_t seed,
                                                          Object* other) {
  ASSERT(other->IsNumber());
  return ComputeIntegerHash(static_cast<uint32_t>(other->Number()), seed);
}

MaybeObject* NumberDictionaryShape::AsObject(uint32_t key) {
  return Isolate::Current()->heap()->NumberFromUint32(key);
}


bool StringDictionaryShape::IsMatch(String* key, Object* other) {
  // We know that all entries in a hash table had their hash keys created.
  // Use that knowledge to have fast failure.
  if (key->Hash() != String::cast(other)->Hash()) return false;
  return key->Equals(String::cast(other));
}


uint32_t StringDictionaryShape::Hash(String* key) {
  return key->Hash();
}


uint32_t StringDictionaryShape::HashForObject(String* key, Object* other) {
  return String::cast(other)->Hash();
}


MaybeObject* StringDictionaryShape::AsObject(String* key) {
  return key;
}


bool ObjectHashTableShape::IsMatch(JSObject* key, Object* other) {
  return key == JSObject::cast(other);
}


uint32_t ObjectHashTableShape::Hash(JSObject* key) {
  MaybeObject* maybe_hash = key->GetIdentityHash(JSObject::OMIT_CREATION);
  ASSERT(!maybe_hash->IsFailure());
  return Smi::cast(maybe_hash->ToObjectUnchecked())->value();
}


uint32_t ObjectHashTableShape::HashForObject(JSObject* key, Object* other) {
  MaybeObject* maybe_hash = JSObject::cast(other)->GetIdentityHash(
      JSObject::OMIT_CREATION);
  ASSERT(!maybe_hash->IsFailure());
  return Smi::cast(maybe_hash->ToObjectUnchecked())->value();
}


MaybeObject* ObjectHashTableShape::AsObject(JSObject* key) {
  return key;
}


void ObjectHashTable::RemoveEntry(int entry) {
  RemoveEntry(entry, GetHeap());
}


void Map::ClearCodeCache(Heap* heap) {
  // No write barrier is needed since empty_fixed_array is not in new space.
  // Please note this function is used during marking:
  //  - MarkCompactCollector::MarkUnmarkedObject
  ASSERT(!heap->InNewSpace(heap->raw_unchecked_empty_fixed_array()));
  WRITE_FIELD(this, kCodeCacheOffset, heap->raw_unchecked_empty_fixed_array());
}


void JSArray::EnsureSize(int required_size) {
  ASSERT(HasFastElements());
  FixedArray* elts = FixedArray::cast(elements());
  const int kArraySizeThatFitsComfortablyInNewSpace = 128;
  if (elts->length() < required_size) {
    // Doubling in size would be overkill, but leave some slack to avoid
    // constantly growing.
    Expand(required_size + (required_size >> 3));
    // It's a performance benefit to keep a frequently used array in new-space.
  } else if (!GetHeap()->new_space()->Contains(elts) &&
             required_size < kArraySizeThatFitsComfortablyInNewSpace) {
    // Expand will allocate a new backing store in new space even if the size
    // we asked for isn't larger than what we had before.
    Expand(required_size);
  }
}


void JSArray::set_length(Smi* length) {
  set_length(static_cast<Object*>(length), SKIP_WRITE_BARRIER);
}


void JSArray::SetContent(FixedArray* storage) {
  set_length(Smi::FromInt(storage->length()));
  set_elements(storage);
}


MaybeObject* FixedArray::Copy() {
  if (length() == 0) return this;
  return GetHeap()->CopyFixedArray(this);
}


Relocatable::Relocatable(Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
  isolate_ = isolate;
  prev_ = isolate->relocatable_top();
  isolate->set_relocatable_top(this);
}


Relocatable::~Relocatable() {
  ASSERT(isolate_ == Isolate::Current());
  ASSERT_EQ(isolate_->relocatable_top(), this);
  isolate_->set_relocatable_top(prev_);
}


int JSObject::BodyDescriptor::SizeOf(Map* map, HeapObject* object) {
  return map->instance_size();
}


void Foreign::ForeignIterateBody(ObjectVisitor* v) {
  v->VisitExternalReference(
      reinterpret_cast<Address *>(FIELD_ADDR(this, kAddressOffset)));
}


template<typename StaticVisitor>
void Foreign::ForeignIterateBody() {
  StaticVisitor::VisitExternalReference(
      reinterpret_cast<Address *>(FIELD_ADDR(this, kAddressOffset)));
}


void ExternalAsciiString::ExternalAsciiStringIterateBody(ObjectVisitor* v) {
  typedef v8::String::ExternalAsciiStringResource Resource;
  v->VisitExternalAsciiString(
      reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)));
}


template<typename StaticVisitor>
void ExternalAsciiString::ExternalAsciiStringIterateBody() {
  typedef v8::String::ExternalAsciiStringResource Resource;
  StaticVisitor::VisitExternalAsciiString(
      reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)));
}


void ExternalTwoByteString::ExternalTwoByteStringIterateBody(ObjectVisitor* v) {
  typedef v8::String::ExternalStringResource Resource;
  v->VisitExternalTwoByteString(
      reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)));
}


template<typename StaticVisitor>
void ExternalTwoByteString::ExternalTwoByteStringIterateBody() {
  typedef v8::String::ExternalStringResource Resource;
  StaticVisitor::VisitExternalTwoByteString(
      reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)));
}

#define SLOT_ADDR(obj, offset) \
  reinterpret_cast<Object**>((obj)->address() + offset)

template<int start_offset, int end_offset, int size>
void FixedBodyDescriptor<start_offset, end_offset, size>::IterateBody(
    HeapObject* obj,
    ObjectVisitor* v) {
    v->VisitPointers(SLOT_ADDR(obj, start_offset), SLOT_ADDR(obj, end_offset));
}


template<int start_offset>
void FlexibleBodyDescriptor<start_offset>::IterateBody(HeapObject* obj,
                                                       int object_size,
                                                       ObjectVisitor* v) {
  v->VisitPointers(SLOT_ADDR(obj, start_offset), SLOT_ADDR(obj, object_size));
}

#undef SLOT_ADDR


#undef CAST_ACCESSOR
#undef INT_ACCESSORS
#undef SMI_ACCESSORS
#undef ACCESSORS
#undef FIELD_ADDR
#undef READ_FIELD
#undef WRITE_FIELD
#undef WRITE_BARRIER
#undef CONDITIONAL_WRITE_BARRIER
#undef READ_MEMADDR_FIELD
#undef WRITE_MEMADDR_FIELD
#undef READ_DOUBLE_FIELD
#undef WRITE_DOUBLE_FIELD
#undef READ_INT_FIELD
#undef WRITE_INT_FIELD
#undef READ_SHORT_FIELD
#undef WRITE_SHORT_FIELD
#undef READ_BYTE_FIELD
#undef WRITE_BYTE_FIELD


} }  // namespace v8::internal

#endif  // V8_OBJECTS_INL_H_
