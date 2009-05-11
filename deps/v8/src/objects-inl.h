// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "objects.h"
#include "contexts.h"
#include "conversions-inl.h"
#include "property.h"

namespace v8 { namespace internal {

PropertyDetails::PropertyDetails(Smi* smi) {
  value_ = smi->value();
}


Smi* PropertyDetails::AsSmi() {
  return Smi::FromInt(value_);
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
    CONDITIONAL_WRITE_BARRIER(this, offset, mode);                      \
  }



#define SMI_ACCESSORS(holder, name, offset)             \
  int holder::name() {                                  \
    Object* value = READ_FIELD(this, offset);           \
    return Smi::cast(value)->value();                   \
  }                                                     \
  void holder::set_##name(int value) {                  \
    WRITE_FIELD(this, offset, Smi::FromInt(value));     \
  }


#define BOOL_ACCESSORS(holder, field, name, offset) \
  bool holder::name() {                                    \
    return BooleanBit::get(field(), offset);               \
  }                                                        \
  void holder::set_##name(bool value) {                    \
    set_##field(BooleanBit::set(field(), offset, value));  \
  }


bool Object::IsSmi() {
  return HAS_SMI_TAG(this);
}


bool Object::IsHeapObject() {
  return HAS_HEAP_OBJECT_TAG(this);
}


bool Object::IsHeapNumber() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() == HEAP_NUMBER_TYPE;
}


bool Object::IsString() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() < FIRST_NONSTRING_TYPE;
}


bool Object::IsSymbol() {
  if (!this->IsHeapObject()) return false;
  uint32_t type = HeapObject::cast(this)->map()->instance_type();
  return (type & (kIsNotStringMask | kIsSymbolMask)) ==
         (kStringTag | kSymbolTag);
}


bool Object::IsConsString() {
  if (!this->IsHeapObject()) return false;
  uint32_t type = HeapObject::cast(this)->map()->instance_type();
  return (type & (kIsNotStringMask | kStringRepresentationMask)) ==
         (kStringTag | kConsStringTag);
}


#ifdef DEBUG
// These are for cast checks.  If you need one of these in release
// mode you should consider using a StringShape before moving it out
// of the ifdef

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


bool Object::IsSlicedString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSliced();
}


#endif  // DEBUG


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
  return (type_ & kIsSymbolMask) == kSymbolTag;
}


bool String::IsAsciiRepresentation() {
  uint32_t type = map()->instance_type();
  if ((type & kStringRepresentationMask) == kSlicedStringTag) {
    return SlicedString::cast(this)->buffer()->IsAsciiRepresentation();
  }
  if ((type & kStringRepresentationMask) == kConsStringTag &&
      ConsString::cast(this)->second()->length() == 0) {
    return ConsString::cast(this)->first()->IsAsciiRepresentation();
  }
  return (type & kStringEncodingMask) == kAsciiStringTag;
}


bool String::IsTwoByteRepresentation() {
  uint32_t type = map()->instance_type();
  if ((type & kStringRepresentationMask) == kSlicedStringTag) {
    return SlicedString::cast(this)->buffer()->IsTwoByteRepresentation();
  } else if ((type & kStringRepresentationMask) == kConsStringTag &&
             ConsString::cast(this)->second()->length() == 0) {
    return ConsString::cast(this)->first()->IsTwoByteRepresentation();
  }
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}


bool StringShape::IsCons() {
  return (type_ & kStringRepresentationMask) == kConsStringTag;
}


bool StringShape::IsSliced() {
  return (type_ & kStringRepresentationMask) == kSlicedStringTag;
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


uint32_t StringShape::full_representation_tag() {
  return (type_ & (kStringRepresentationMask | kStringEncodingMask));
}


uint32_t StringShape::size_tag() {
  return (type_ & kStringSizeMask);
}


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


bool Object::IsFailure() {
  return HAS_FAILURE_TAG(this);
}


bool Object::IsRetryAfterGC() {
  return HAS_FAILURE_TAG(this)
    && Failure::cast(this)->type() == Failure::RETRY_AFTER_GC;
}


bool Object::IsOutOfMemoryFailure() {
  return HAS_FAILURE_TAG(this)
    && Failure::cast(this)->IsOutOfMemoryException();
}


bool Object::IsException() {
  return this == Failure::Exception();
}


bool Object::IsJSObject() {
  return IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() >= FIRST_JS_OBJECT_TYPE;
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


bool Object::IsDescriptorArray() {
  return IsFixedArray();
}


bool Object::IsContext() {
  return Object::IsHeapObject()
    && (HeapObject::cast(this)->map() == Heap::context_map() ||
        HeapObject::cast(this)->map() == Heap::catch_context_map() ||
        HeapObject::cast(this)->map() == Heap::global_context_map());
}


bool Object::IsCatchContext() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map() == Heap::catch_context_map();
}


bool Object::IsGlobalContext() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map() == Heap::global_context_map();
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
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() == ODDBALL_TYPE;
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


bool Object::IsStringWrapper() {
  return IsJSValue() && JSValue::cast(this)->value()->IsString();
}


bool Object::IsProxy() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() == PROXY_TYPE;
}


bool Object::IsBoolean() {
  return IsTrue() || IsFalse();
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
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map() == Heap::hash_table_map();
}


bool Object::IsDictionary() {
  return IsHashTable() && this != Heap::symbol_table();
}


bool Object::IsSymbolTable() {
  return IsHashTable() && this == Heap::symbol_table();
}


bool Object::IsCompilationCacheTable() {
  return IsHashTable();
}


bool Object::IsMapCache() {
  return IsHashTable();
}


bool Object::IsLookupCache() {
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
  return this == Heap::undefined_value();
}


bool Object::IsTheHole() {
  return this == Heap::the_hole_value();
}


bool Object::IsNull() {
  return this == Heap::null_value();
}


bool Object::IsTrue() {
  return this == Heap::true_value();
}


bool Object::IsFalse() {
  return this == Heap::false_value();
}


double Object::Number() {
  ASSERT(IsNumber());
  return IsSmi()
    ? static_cast<double>(reinterpret_cast<Smi*>(this)->value())
    : reinterpret_cast<HeapNumber*>(this)->value();
}



Object* Object::ToSmi() {
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


Object* Object::GetElement(uint32_t index) {
  return GetElementWithReceiver(this, index);
}


Object* Object::GetProperty(String* key) {
  PropertyAttributes attributes;
  return GetPropertyWithReceiver(this, key, &attributes);
}


Object* Object::GetProperty(String* key, PropertyAttributes* attributes) {
  return GetPropertyWithReceiver(this, key, attributes);
}


#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<byte*>(p) + offset - kHeapObjectTag)

#define READ_FIELD(p, offset) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)))

#define WRITE_FIELD(p, offset, value) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)) = value)


#define WRITE_BARRIER(object, offset) \
  Heap::RecordWrite(object->address(), offset);

// CONDITIONAL_WRITE_BARRIER must be issued after the actual
// write due to the assert validating the written value.
#define CONDITIONAL_WRITE_BARRIER(object, offset, mode) \
  if (mode == UPDATE_WRITE_BARRIER) { \
    Heap::RecordWrite(object->address(), offset); \
  } else { \
    ASSERT(mode == SKIP_WRITE_BARRIER); \
    ASSERT(Heap::InNewSpace(object) || \
           !Heap::InNewSpace(READ_FIELD(object, offset))); \
  }

#define READ_DOUBLE_FIELD(p, offset) \
  (*reinterpret_cast<double*>(FIELD_ADDR(p, offset)))

#define WRITE_DOUBLE_FIELD(p, offset, value) \
  (*reinterpret_cast<double*>(FIELD_ADDR(p, offset)) = value)

#define READ_INT_FIELD(p, offset) \
  (*reinterpret_cast<int*>(FIELD_ADDR(p, offset)))

#define WRITE_INT_FIELD(p, offset, value) \
  (*reinterpret_cast<int*>(FIELD_ADDR(p, offset)) = value)

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
  return static_cast<int>(reinterpret_cast<intptr_t>(this) >> kSmiTagSize);
}


Smi* Smi::FromInt(int value) {
  ASSERT(Smi::IsValid(value));
  intptr_t tagged_value =
      (static_cast<intptr_t>(value) << kSmiTagSize) | kSmiTag;
  return reinterpret_cast<Smi*>(tagged_value);
}


Smi* Smi::FromIntptr(intptr_t value) {
  ASSERT(Smi::IsValid(value));
  return reinterpret_cast<Smi*>((value << kSmiTagSize) | kSmiTag);
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


int Failure::requested() const {
  const int kShiftBits =
      kFailureTypeTagSize + kSpaceTagSize - kObjectAlignmentBits;
  STATIC_ASSERT(kShiftBits >= 0);
  ASSERT(type() == RETRY_AFTER_GC);
  return value() >> kShiftBits;
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


int Failure::value() const {
  return static_cast<int>(reinterpret_cast<intptr_t>(this) >> kFailureTagSize);
}


Failure* Failure::RetryAfterGC(int requested_bytes) {
  int requested = requested_bytes >> kObjectAlignmentBits;
  int value = (requested << kSpaceTagSize) | NEW_SPACE;
  ASSERT(value >> kSpaceTagSize == requested);
  ASSERT(Smi::IsValid(value));
  ASSERT(value == ((value << kFailureTypeTagSize) >> kFailureTypeTagSize));
  ASSERT(Smi::IsValid(value << kFailureTypeTagSize));
  return Construct(RETRY_AFTER_GC, value);
}


Failure* Failure::Construct(Type type, int value) {
  int info = (value << kFailureTypeTagSize) | type;
  ASSERT(Smi::IsValid(info));  // Same validation check as in Smi
  return reinterpret_cast<Failure*>(
      static_cast<intptr_t>((info << kFailureTagSize) | kFailureTag));
}


bool Smi::IsValid(int value) {
#ifdef DEBUG
  bool in_range = (value >= kMinValue) && (value <= kMaxValue);
#endif
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
  bool result =
      ((static_cast<unsigned int>(value) + 0x40000000U) & 0x80000000U) == 0;
  ASSERT(result == in_range);
  return result;
}


bool Smi::IsIntptrValid(intptr_t value) {
#ifdef DEBUG
  bool in_range = (value >= kMinValue) && (value <= kMaxValue);
#endif
  // See Smi::IsValid(int) for description.
  bool result =
      ((static_cast<uintptr_t>(value) + 0x40000000U) < 0x80000000U);
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

  int compact_offset = offset >> kObjectAlignmentBits;
  ASSERT(compact_offset < (1 << kForwardingOffsetBits));

  Page* map_page = Page::FromAddress(map_address);
  ASSERT_MAP_PAGE_INDEX(map_page->mc_page_index);

  int map_page_offset =
      map_page->Offset(map_address) >> kObjectAlignmentBits;

  uintptr_t encoding =
      (compact_offset << kForwardingOffsetShift) |
      (map_page_offset << kMapPageOffsetShift) |
      (map_page->mc_page_index << kMapPageIndexShift);
  return MapWord(encoding);
}


Address MapWord::DecodeMapAddress(MapSpace* map_space) {
  int map_page_index = (value_ & kMapPageIndexMask) >> kMapPageIndexShift;
  ASSERT_MAP_PAGE_INDEX(map_page_index);

  int map_page_offset =
      ((value_ & kMapPageOffsetMask) >> kMapPageOffsetShift)
      << kObjectAlignmentBits;

  return (map_space->PageAddress(map_page_index) + map_page_offset);
}


int MapWord::DecodeOffset() {
  // The offset field is represented in the kForwardingOffsetBits
  // most-significant bits.
  int offset = (value_ >> kForwardingOffsetShift) << kObjectAlignmentBits;
  ASSERT(0 <= offset && offset < Page::kObjectAreaSize);
  return offset;
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
#endif


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
  // WRITE_FIELD does not update the remembered set, but there is no need
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


ACCESSORS(JSObject, properties, FixedArray, kPropertiesOffset)
ACCESSORS(JSObject, elements, FixedArray, kElementsOffset)


void JSObject::initialize_properties() {
  ASSERT(!Heap::InNewSpace(Heap::empty_fixed_array()));
  WRITE_FIELD(this, kPropertiesOffset, Heap::empty_fixed_array());
}


void JSObject::initialize_elements() {
  ASSERT(!Heap::InNewSpace(Heap::empty_fixed_array()));
  WRITE_FIELD(this, kElementsOffset, Heap::empty_fixed_array());
}


ACCESSORS(Oddball, to_string, String, kToStringOffset)
ACCESSORS(Oddball, to_number, Object, kToNumberOffset)


int JSObject::GetHeaderSize() {
  switch (map()->instance_type()) {
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
    case JS_REGEXP_TYPE:
      return JSValue::kSize;
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
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
  CONDITIONAL_WRITE_BARRIER(this, offset, mode);
  return value;
}



void JSObject::InitializeBody(int object_size) {
  Object* value = Heap::undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}


void Struct::InitializeBody(int object_size) {
  Object* value = Heap::undefined_value();
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}


bool JSObject::HasFastProperties() {
  return !properties()->IsDictionary();
}


bool Array::IndexFromObject(Object* object, uint32_t* index) {
  if (object->IsSmi()) {
    int value = Smi::cast(object)->value();
    if (value < 0) return false;
    *index = value;
    return true;
  }
  if (object->IsHeapNumber()) {
    double value = HeapNumber::cast(object)->value();
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


Object* FixedArray::get(int index) {
  ASSERT(index >= 0 && index < this->length());
  return READ_FIELD(this, kHeaderSize + index * kPointerSize);
}


void FixedArray::set(int index, Smi* value) {
  ASSERT(reinterpret_cast<Object*>(value)->IsSmi());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
}


void FixedArray::set(int index, Object* value) {
  ASSERT(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(this, offset);
}


WriteBarrierMode HeapObject::GetWriteBarrierMode() {
  if (Heap::InNewSpace(this)) return SKIP_WRITE_BARRIER;
  return UPDATE_WRITE_BARRIER;
}


void FixedArray::set(int index,
                     Object* value,
                     WriteBarrierMode mode) {
  ASSERT(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(this, offset, mode);
}


void FixedArray::fast_set(FixedArray* array, int index, Object* value) {
  ASSERT(index >= 0 && index < array->length());
  WRITE_FIELD(array, kHeaderSize + index * kPointerSize, value);
}


void FixedArray::set_undefined(int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!Heap::InNewSpace(Heap::undefined_value()));
  WRITE_FIELD(this, kHeaderSize + index * kPointerSize,
              Heap::undefined_value());
}


void FixedArray::set_null(int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!Heap::InNewSpace(Heap::null_value()));
  WRITE_FIELD(this, kHeaderSize + index * kPointerSize, Heap::null_value());
}


void FixedArray::set_the_hole(int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!Heap::InNewSpace(Heap::the_hole_value()));
  WRITE_FIELD(this, kHeaderSize + index * kPointerSize, Heap::the_hole_value());
}


bool DescriptorArray::IsEmpty() {
  ASSERT(this == Heap::empty_descriptor_array() ||
         this->length() > 2);
  return this == Heap::empty_descriptor_array();
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


void DescriptorArray::Get(int descriptor_number, Descriptor* desc) {
  desc->Init(GetKey(descriptor_number),
             GetValue(descriptor_number),
             GetDetails(descriptor_number));
}


void DescriptorArray::Set(int descriptor_number, Descriptor* desc) {
  // Range check.
  ASSERT(descriptor_number < number_of_descriptors());

  // Make sure non of the elements in desc are in new space.
  ASSERT(!Heap::InNewSpace(desc->GetKey()));
  ASSERT(!Heap::InNewSpace(desc->GetValue()));

  fast_set(this, ToKeyIndex(descriptor_number), desc->GetKey());
  FixedArray* content_array = GetContentArray();
  fast_set(content_array, ToValueIndex(descriptor_number), desc->GetValue());
  fast_set(content_array, ToDetailsIndex(descriptor_number),
           desc->GetDetails().AsSmi());
}


void DescriptorArray::Swap(int first, int second) {
  fast_swap(this, ToKeyIndex(first), ToKeyIndex(second));
  FixedArray* content_array = GetContentArray();
  fast_swap(content_array, ToValueIndex(first), ToValueIndex(second));
  fast_swap(content_array, ToDetailsIndex(first),  ToDetailsIndex(second));
}


bool Dictionary::requires_slow_elements() {
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return false;
  return 0 !=
      (Smi::cast(max_index_object)->value() & kRequiresSlowElementsMask);
}


uint32_t Dictionary::max_number_key() {
  ASSERT(!requires_slow_elements());
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi()) return 0;
  uint32_t value = static_cast<uint32_t>(Smi::cast(max_index_object)->value());
  return value >> kRequiresSlowElementsTagSize;
}


void Dictionary::set_requires_slow_elements() {
  set(kMaxNumberKeyIndex,
      Smi::FromInt(kRequiresSlowElementsMask),
      SKIP_WRITE_BARRIER);
}


// ------------------------------------
// Cast operations


CAST_ACCESSOR(FixedArray)
CAST_ACCESSOR(DescriptorArray)
CAST_ACCESSOR(Dictionary)
CAST_ACCESSOR(SymbolTable)
CAST_ACCESSOR(CompilationCacheTable)
CAST_ACCESSOR(MapCache)
CAST_ACCESSOR(LookupCache)
CAST_ACCESSOR(String)
CAST_ACCESSOR(SeqString)
CAST_ACCESSOR(SeqAsciiString)
CAST_ACCESSOR(SeqTwoByteString)
CAST_ACCESSOR(ConsString)
CAST_ACCESSOR(SlicedString)
CAST_ACCESSOR(ExternalString)
CAST_ACCESSOR(ExternalAsciiString)
CAST_ACCESSOR(ExternalTwoByteString)
CAST_ACCESSOR(JSObject)
CAST_ACCESSOR(Smi)
CAST_ACCESSOR(Failure)
CAST_ACCESSOR(HeapObject)
CAST_ACCESSOR(HeapNumber)
CAST_ACCESSOR(Oddball)
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
CAST_ACCESSOR(Proxy)
CAST_ACCESSOR(ByteArray)
CAST_ACCESSOR(Struct)


#define MAKE_STRUCT_CAST(NAME, Name, name) CAST_ACCESSOR(Name)
  STRUCT_LIST(MAKE_STRUCT_CAST)
#undef MAKE_STRUCT_CAST

template <int prefix_size, int elem_size>
HashTable<prefix_size, elem_size>* HashTable<prefix_size, elem_size>::cast(
    Object* obj) {
  ASSERT(obj->IsHashTable());
  return reinterpret_cast<HashTable*>(obj);
}


INT_ACCESSORS(Array, length, kLengthOffset)


bool String::Equals(String* other) {
  if (other == this) return true;
  if (StringShape(this).IsSymbol() && StringShape(other).IsSymbol()) {
    return false;
  }
  return SlowEquals(other);
}


int String::length() {
  uint32_t len = READ_INT_FIELD(this, kLengthOffset);

  ASSERT(kShortStringTag + kLongLengthShift == kShortLengthShift);
  ASSERT(kMediumStringTag + kLongLengthShift == kMediumLengthShift);
  ASSERT(kLongStringTag == 0);

  return len >> (StringShape(this).size_tag() + kLongLengthShift);
}


void String::set_length(int value) {
  ASSERT(kShortStringTag + kLongLengthShift == kShortLengthShift);
  ASSERT(kMediumStringTag + kLongLengthShift == kMediumLengthShift);
  ASSERT(kLongStringTag == 0);

  WRITE_INT_FIELD(this,
                  kLengthOffset,
                  value << (StringShape(this).size_tag() + kLongLengthShift));
}


uint32_t String::length_field() {
  return READ_UINT32_FIELD(this, kLengthOffset);
}


void String::set_length_field(uint32_t value) {
  WRITE_UINT32_FIELD(this, kLengthOffset, value);
}


Object* String::TryFlattenIfNotFlat() {
  // We don't need to flatten strings that are already flat.  Since this code
  // is inlined, it can be helpful in the flat case to not call out to Flatten.
  if (!IsFlat()) {
    return TryFlatten();
  }
  return this;
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
    case kSlicedStringTag | kAsciiStringTag:
    case kSlicedStringTag | kTwoByteStringTag:
      return SlicedString::cast(this)->SlicedStringGet(index);
    case kExternalStringTag | kAsciiStringTag:
      return ExternalAsciiString::cast(this)->ExternalAsciiStringGet(index);
    case kExternalStringTag | kTwoByteStringTag:
      return ExternalTwoByteString::cast(this)->ExternalTwoByteStringGet(index);
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
  switch (StringShape(this).representation_tag()) {
    case kConsStringTag: {
      String* second = ConsString::cast(this)->second();
      // Only flattened strings have second part empty.
      return second->length() == 0;
    }
    case kSlicedStringTag: {
      StringRepresentationTag tag =
          StringShape(SlicedString::cast(this)->buffer()).representation_tag();
      return tag == kSeqStringTag || tag == kExternalStringTag;
    }
    default:
      return true;
  }
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
  uint32_t length = READ_INT_FIELD(this, kLengthOffset);

  ASSERT(kShortStringTag + kLongLengthShift == kShortLengthShift);
  ASSERT(kMediumStringTag + kLongLengthShift == kMediumLengthShift);
  ASSERT(kLongStringTag == 0);

  // Use the map (and not 'this') to compute the size tag, since
  // TwoByteStringSize is called during GC when maps are encoded.
  length >>= StringShape(instance_type).size_tag() + kLongLengthShift;

  return SizeFor(length);
}


int SeqAsciiString::SeqAsciiStringSize(InstanceType instance_type) {
  uint32_t length = READ_INT_FIELD(this, kLengthOffset);

  ASSERT(kShortStringTag + kLongLengthShift == kShortLengthShift);
  ASSERT(kMediumStringTag + kLongLengthShift == kMediumLengthShift);
  ASSERT(kLongStringTag == 0);

  // Use the map (and not 'this') to compute the size tag, since
  // AsciiStringSize is called during GC when maps are encoded.
  length >>= StringShape(instance_type).size_tag() + kLongLengthShift;

  return SizeFor(length);
}


String* ConsString::first() {
  return String::cast(READ_FIELD(this, kFirstOffset));
}


Object* ConsString::unchecked_first() {
  return READ_FIELD(this, kFirstOffset);
}


void ConsString::set_first(String* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kFirstOffset, value);
  CONDITIONAL_WRITE_BARRIER(this, kFirstOffset, mode);
}


String* ConsString::second() {
  return String::cast(READ_FIELD(this, kSecondOffset));
}


Object* ConsString::unchecked_second() {
  return READ_FIELD(this, kSecondOffset);
}


void ConsString::set_second(String* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kSecondOffset, value);
  CONDITIONAL_WRITE_BARRIER(this, kSecondOffset, mode);
}


String* SlicedString::buffer() {
  return String::cast(READ_FIELD(this, kBufferOffset));
}


void SlicedString::set_buffer(String* buffer) {
  WRITE_FIELD(this, kBufferOffset, buffer);
  WRITE_BARRIER(this, kBufferOffset);
}


int SlicedString::start() {
  return READ_INT_FIELD(this, kStartOffset);
}


void SlicedString::set_start(int start) {
  WRITE_INT_FIELD(this, kStartOffset, start);
}


ExternalAsciiString::Resource* ExternalAsciiString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalAsciiString::set_resource(
    ExternalAsciiString::Resource* resource) {
  *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)) = resource;
}


Map* ExternalAsciiString::StringMap(int length) {
  Map* map;
  // Number of characters: determines the map.
  if (length <= String::kMaxShortStringSize) {
    map = Heap::short_external_ascii_string_map();
  } else if (length <= String::kMaxMediumStringSize) {
    map = Heap::medium_external_ascii_string_map();
  } else {
    map = Heap::long_external_ascii_string_map();
  }
  return map;
}


Map* ExternalAsciiString::SymbolMap(int length) {
  Map* map;
  // Number of characters: determines the map.
  if (length <= String::kMaxShortStringSize) {
    map = Heap::short_external_ascii_symbol_map();
  } else if (length <= String::kMaxMediumStringSize) {
    map = Heap::medium_external_ascii_symbol_map();
  } else {
    map = Heap::long_external_ascii_symbol_map();
  }
  return map;
}


ExternalTwoByteString::Resource* ExternalTwoByteString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalTwoByteString::set_resource(
    ExternalTwoByteString::Resource* resource) {
  *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset)) = resource;
}


Map* ExternalTwoByteString::StringMap(int length) {
  Map* map;
  // Number of characters: determines the map.
  if (length <= String::kMaxShortStringSize) {
    map = Heap::short_external_string_map();
  } else if (length <= String::kMaxMediumStringSize) {
    map = Heap::medium_external_string_map();
  } else {
    map = Heap::long_external_string_map();
  }
  return map;
}


Map* ExternalTwoByteString::SymbolMap(int length) {
  Map* map;
  // Number of characters: determines the map.
  if (length <= String::kMaxShortStringSize) {
    map = Heap::short_external_symbol_map();
  } else if (length <= String::kMaxMediumStringSize) {
    map = Heap::medium_external_symbol_map();
  } else {
    map = Heap::long_external_symbol_map();
  }
  return map;
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


int Map::instance_size() {
  return READ_BYTE_FIELD(this, kInstanceSizeOffset) << kPointerSizeLog2;
}


int Map::inobject_properties() {
  return READ_BYTE_FIELD(this, kInObjectPropertiesOffset);
}


int HeapObject::SizeFromMap(Map* map) {
  InstanceType instance_type = map->instance_type();
  // Only inline the two most frequent cases.
  if (instance_type == JS_OBJECT_TYPE) return  map->instance_size();
  if (instance_type == FIXED_ARRAY_TYPE) {
    return reinterpret_cast<FixedArray*>(this)->FixedArraySize();
  }
  // Otherwise do the general size computation.
  return SlowSizeFromMap(map);
}


void Map::set_instance_size(int value) {
  ASSERT((value & ~(kPointerSize - 1)) == value);
  value >>= kPointerSizeLog2;
  ASSERT(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this, kInstanceSizeOffset, static_cast<byte>(value));
}


void Map::set_inobject_properties(int value) {
  ASSERT(0 <= value && value < 256);
  WRITE_BYTE_FIELD(this, kInObjectPropertiesOffset, static_cast<byte>(value));
}


InstanceType Map::instance_type() {
  return static_cast<InstanceType>(READ_BYTE_FIELD(this, kInstanceTypeOffset));
}


void Map::set_instance_type(InstanceType value) {
  ASSERT(0 <= value && value < 256);
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


Code::Flags Code::flags() {
  return static_cast<Flags>(READ_INT_FIELD(this, kFlagsOffset));
}


void Code::set_flags(Code::Flags flags) {
  STATIC_ASSERT(Code::NUMBER_OF_KINDS <= (kFlagsKindMask >> kFlagsKindShift)+1);
  // Make sure that all call stubs have an arguments count.
  ASSERT(ExtractKindFromFlags(flags) != CALL_IC ||
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


PropertyType Code::type() {
  ASSERT(ic_state() == MONOMORPHIC);
  return ExtractTypeFromFlags(flags());
}


int Code::arguments_count() {
  ASSERT(is_call_stub() || kind() == STUB);
  return ExtractArgumentsCountFromFlags(flags());
}


CodeStub::Major Code::major_key() {
  ASSERT(kind() == STUB);
  return static_cast<CodeStub::Major>(READ_BYTE_FIELD(this,
                                                      kStubMajorKeyOffset));
}


void Code::set_major_key(CodeStub::Major major) {
  ASSERT(kind() == STUB);
  ASSERT(0 <= major && major < 256);
  WRITE_BYTE_FIELD(this, kStubMajorKeyOffset, major);
}


bool Code::is_inline_cache_stub() {
  Kind kind = this->kind();
  return kind >= FIRST_IC_KIND && kind <= LAST_IC_KIND;
}


Code::Flags Code::ComputeFlags(Kind kind,
                               InlineCacheState ic_state,
                               PropertyType type,
                               int argc) {
  // Compute the bit mask.
  int bits = kind << kFlagsKindShift;
  bits |= ic_state << kFlagsICStateShift;
  bits |= type << kFlagsTypeShift;
  bits |= argc << kFlagsArgumentsCountShift;
  // Cast to flags and validate result before returning it.
  Flags result = static_cast<Flags>(bits);
  ASSERT(ExtractKindFromFlags(result) == kind);
  ASSERT(ExtractICStateFromFlags(result) == ic_state);
  ASSERT(ExtractTypeFromFlags(result) == type);
  ASSERT(ExtractArgumentsCountFromFlags(result) == argc);
  return result;
}


Code::Flags Code::ComputeMonomorphicFlags(Kind kind,
                                          PropertyType type,
                                          int argc) {
  return ComputeFlags(kind, MONOMORPHIC, type, argc);
}


Code::Kind Code::ExtractKindFromFlags(Flags flags) {
  int bits = (flags & kFlagsKindMask) >> kFlagsKindShift;
  return static_cast<Kind>(bits);
}


InlineCacheState Code::ExtractICStateFromFlags(Flags flags) {
  int bits = (flags & kFlagsICStateMask) >> kFlagsICStateShift;
  return static_cast<InlineCacheState>(bits);
}


PropertyType Code::ExtractTypeFromFlags(Flags flags) {
  int bits = (flags & kFlagsTypeMask) >> kFlagsTypeShift;
  return static_cast<PropertyType>(bits);
}


int Code::ExtractArgumentsCountFromFlags(Flags flags) {
  return (flags & kFlagsArgumentsCountMask) >> kFlagsArgumentsCountShift;
}


Code::Flags Code::RemoveTypeFromFlags(Flags flags) {
  int bits = flags & ~kFlagsTypeMask;
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


Object* Map::prototype() {
  return READ_FIELD(this, kPrototypeOffset);
}


void Map::set_prototype(Object* value, WriteBarrierMode mode) {
  ASSERT(value->IsNull() || value->IsJSObject());
  WRITE_FIELD(this, kPrototypeOffset, value);
  CONDITIONAL_WRITE_BARRIER(this, kPrototypeOffset, mode);
}


ACCESSORS(Map, instance_descriptors, DescriptorArray,
          kInstanceDescriptorsOffset)
ACCESSORS(Map, code_cache, FixedArray, kCodeCacheOffset)
ACCESSORS(Map, constructor, Object, kConstructorOffset)

ACCESSORS(JSFunction, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
ACCESSORS(JSFunction, literals, FixedArray, kLiteralsOffset)

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
ACCESSORS(Script, wrapper, Proxy, kWrapperOffset)
ACCESSORS(Script, type, Smi, kTypeOffset)
ACCESSORS(Script, line_ends, Object, kLineEndsOffset)

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
ACCESSORS(SharedFunctionInfo, instance_class_name, Object,
          kInstanceClassNameOffset)
ACCESSORS(SharedFunctionInfo, function_data, Object,
          kExternalReferenceDataOffset)
ACCESSORS(SharedFunctionInfo, script, Object, kScriptOffset)
ACCESSORS(SharedFunctionInfo, debug_info, Object, kDebugInfoOffset)
ACCESSORS(SharedFunctionInfo, inferred_name, String, kInferredNameOffset)

BOOL_ACCESSORS(FunctionTemplateInfo, flag, hidden_prototype,
               kHiddenPrototypeBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, undetectable, kUndetectableBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, needs_access_check,
               kNeedsAccessCheckBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_expression,
               kIsExpressionBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_toplevel,
               kIsTopLevelBit)

INT_ACCESSORS(SharedFunctionInfo, length, kLengthOffset)
INT_ACCESSORS(SharedFunctionInfo, formal_parameter_count,
              kFormalParameterCountOffset)
INT_ACCESSORS(SharedFunctionInfo, expected_nof_properties,
              kExpectedNofPropertiesOffset)
INT_ACCESSORS(SharedFunctionInfo, start_position_and_type,
              kStartPositionAndTypeOffset)
INT_ACCESSORS(SharedFunctionInfo, end_position, kEndPositionOffset)
INT_ACCESSORS(SharedFunctionInfo, function_token_position,
              kFunctionTokenPositionOffset)


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


void SharedFunctionInfo::set_code(Code* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kCodeOffset, value);
  CONDITIONAL_WRITE_BARRIER(this, kCodeOffset, mode);
}


bool SharedFunctionInfo::is_compiled() {
  // TODO(1242782): Create a code kind for uncompiled code.
  return code()->kind() != Code::STUB;
}


bool JSFunction::IsBoilerplate() {
  return map() == Heap::boilerplate_function_map();
}


bool JSObject::IsLoaded() {
  return !map()->needs_loading();
}


Code* JSFunction::code() {
  return shared()->code();
}


void JSFunction::set_code(Code* value) {
  shared()->set_code(value);
}


Context* JSFunction::context() {
  return Context::cast(READ_FIELD(this, kContextOffset));
}


Object* JSFunction::unchecked_context() {
  return READ_FIELD(this, kContextOffset);
}


void JSFunction::set_context(Object* value) {
  ASSERT(value == Heap::undefined_value() || value->IsContext());
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


bool JSFunction::is_compiled() {
  return shared()->is_compiled();
}


int JSFunction::NumberOfLiterals() {
  return literals()->length();
}


Object* JSBuiltinsObject::javascript_builtin(Builtins::JavaScript id) {
  ASSERT(0 <= id && id < kJSBuiltinsCount);
  return READ_FIELD(this, kJSBuiltinsOffset + (id * kPointerSize));
}


void JSBuiltinsObject::set_javascript_builtin(Builtins::JavaScript id,
                                              Object* value) {
  ASSERT(0 <= id && id < kJSBuiltinsCount);
  WRITE_FIELD(this, kJSBuiltinsOffset + (id * kPointerSize), value);
  WRITE_BARRIER(this, kJSBuiltinsOffset + (id * kPointerSize));
}


Address Proxy::proxy() {
  return AddressFrom<Address>(READ_INT_FIELD(this, kProxyOffset));
}


void Proxy::set_proxy(Address value) {
  WRITE_INT_FIELD(this, kProxyOffset, OffsetFrom(value));
}


void Proxy::ProxyIterateBody(ObjectVisitor* visitor) {
  visitor->VisitExternalReference(
      reinterpret_cast<Address *>(FIELD_ADDR(this, kProxyOffset)));
}


ACCESSORS(JSValue, value, Object, kValueOffset)


JSValue* JSValue::cast(Object* obj) {
  ASSERT(obj->IsJSValue());
  ASSERT(HeapObject::cast(obj)->Size() == JSValue::kSize);
  return reinterpret_cast<JSValue*>(obj);
}


INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, relocation_size, kRelocationSizeOffset)
INT_ACCESSORS(Code, sinfo_size, kSInfoSizeOffset)


Code::ICTargetState Code::ic_flag() {
  return static_cast<ICTargetState>(READ_BYTE_FIELD(this, kICFlagOffset));
}


void Code::set_ic_flag(ICTargetState value) {
  WRITE_BYTE_FIELD(this, kICFlagOffset, value);
}


byte* Code::instruction_start()  {
  return FIELD_ADDR(this, kHeaderSize);
}


int Code::body_size() {
  return RoundUp(instruction_size() + relocation_size(), kObjectAlignment);
}


byte* Code::relocation_start() {
  return FIELD_ADDR(this, kHeaderSize + instruction_size());
}


byte* Code::entry() {
  return instruction_start();
}


bool Code::contains(byte* pc) {
  return (instruction_start() <= pc) &&
      (pc < instruction_start() + instruction_size());
}


byte* Code::sinfo_start() {
  return FIELD_ADDR(this, kHeaderSize + body_size());
}


ACCESSORS(JSArray, length, Object, kLengthOffset)


ACCESSORS(JSRegExp, data, Object, kDataOffset)


JSRegExp::Type JSRegExp::TypeTag() {
  Object* data = this->data();
  if (data->IsUndefined()) return JSRegExp::NOT_COMPILED;
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


void JSRegExp::SetDataAt(int index, Object* value) {
  ASSERT(TypeTag() != NOT_COMPILED);
  ASSERT(index >= kDataIndex);  // Only implementation data can be set this way.
  FixedArray::cast(data())->set(index, value);
}


bool JSObject::HasFastElements() {
  return !elements()->IsDictionary();
}


bool JSObject::HasNamedInterceptor() {
  return map()->has_named_interceptor();
}


bool JSObject::HasIndexedInterceptor() {
  return map()->has_indexed_interceptor();
}


Dictionary* JSObject::property_dictionary() {
  ASSERT(!HasFastProperties());
  return Dictionary::cast(properties());
}


Dictionary* JSObject::element_dictionary() {
  ASSERT(!HasFastElements());
  return Dictionary::cast(elements());
}


bool String::HasHashCode() {
  return (length_field() & kHashComputedMask) != 0;
}


uint32_t String::Hash() {
  // Fast case: has hash code already been computed?
  uint32_t field = length_field();
  if (field & kHashComputedMask) return field >> kHashShift;
  // Slow case: compute hash code and set it.
  return ComputeAndSetHash();
}


StringHasher::StringHasher(int length)
  : length_(length),
    raw_running_hash_(0),
    array_index_(0),
    is_array_index_(0 < length_ && length_ <= String::kMaxArrayIndexSize),
    is_first_char_(true),
    is_valid_(true) { }


bool StringHasher::has_trivial_hash() {
  return length_ > String::kMaxMediumStringSize;
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
  if (result == 0) {
    result = 27;
  }
  return result;
}


bool String::AsArrayIndex(uint32_t* index) {
  uint32_t field = length_field();
  if ((field & kHashComputedMask) && !(field & kIsArrayIndexMask)) return false;
  return SlowAsArrayIndex(index);
}


Object* JSObject::GetPrototype() {
  return JSObject::cast(this)->map()->prototype();
}


PropertyAttributes JSObject::GetPropertyAttribute(String* key) {
  return GetPropertyAttributeWithReceiver(this, key);
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
  ASSERT(AttributesField::is_valid(attributes));
  int rest_value = flag()->value() & ~AttributesField::mask();
  set_flag(Smi::FromInt(rest_value | AttributesField::encode(attributes)));
}

void Dictionary::SetEntry(int entry,
                          Object* key,
                          Object* value,
                          PropertyDetails details) {
  ASSERT(!key->IsString() || details.index() > 0);
  int index = EntryToIndex(entry);
  WriteBarrierMode mode = GetWriteBarrierMode();
  set(index, key, mode);
  set(index+1, value, mode);
  fast_set(this, index+2, details.AsSmi());
}


void Map::ClearCodeCache() {
  // No write barrier is needed since empty_fixed_array is not in new space.
  // Please note this function is used during marking:
  //  - MarkCompactCollector::MarkUnmarkedObject
  ASSERT(!Heap::InNewSpace(Heap::empty_fixed_array()));
  WRITE_FIELD(this, kCodeCacheOffset, Heap::empty_fixed_array());
}


void JSArray::SetContent(FixedArray* storage) {
  set_length(Smi::FromInt(storage->length()), SKIP_WRITE_BARRIER);
  set_elements(storage);
}


Object* FixedArray::Copy() {
  if (length() == 0) return this;
  return Heap::CopyFixedArray(this);
}


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
