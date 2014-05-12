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

#include "elements.h"
#include "objects.h"
#include "contexts.h"
#include "conversions-inl.h"
#include "heap.h"
#include "isolate.h"
#include "heap-inl.h"
#include "property.h"
#include "spaces.h"
#include "store-buffer.h"
#include "v8memory.h"
#include "factory.h"
#include "incremental-marking.h"
#include "transitions-inl.h"
#include "objects-visiting.h"

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


PropertyDetails PropertyDetails::AsDeleted() const {
  Smi* smi = Smi::FromInt(value_ | DeletedField::encode(1));
  return PropertyDetails(smi);
}


#define TYPE_CHECKER(type, instancetype)                                \
  bool Object::Is##type() {                                             \
  return Object::IsHeapObject() &&                                      \
      HeapObject::cast(this)->map()->instance_type() == instancetype;   \
  }


#define CAST_ACCESSOR(type)                     \
  type* type::cast(Object* object) {            \
    SLOW_ASSERT(object->Is##type());            \
    return reinterpret_cast<type*>(object);     \
  }


#define INT_ACCESSORS(holder, name, offset)                             \
  int holder::name() { return READ_INT_FIELD(this, offset); }           \
  void holder::set_##name(int value) { WRITE_INT_FIELD(this, offset, value); }


#define ACCESSORS(holder, name, type, offset)                           \
  type* holder::name() { return type::cast(READ_FIELD(this, offset)); } \
  void holder::set_##name(type* value, WriteBarrierMode mode) {         \
    WRITE_FIELD(this, offset, value);                                   \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);    \
  }


// Getter that returns a tagged Smi and setter that writes a tagged Smi.
#define ACCESSORS_TO_SMI(holder, name, offset)                          \
  Smi* holder::name() { return Smi::cast(READ_FIELD(this, offset)); }   \
  void holder::set_##name(Smi* value, WriteBarrierMode mode) {          \
    WRITE_FIELD(this, offset, value);                                   \
  }


// Getter that returns a Smi as an int and writes an int as a Smi.
#define SMI_ACCESSORS(holder, name, offset)             \
  int holder::name() {                                  \
    Object* value = READ_FIELD(this, offset);           \
    return Smi::cast(value)->value();                   \
  }                                                     \
  void holder::set_##name(int value) {                  \
    WRITE_FIELD(this, offset, Smi::FromInt(value));     \
  }

#define SYNCHRONIZED_SMI_ACCESSORS(holder, name, offset)    \
  int holder::synchronized_##name() {                       \
    Object* value = ACQUIRE_READ_FIELD(this, offset);       \
    return Smi::cast(value)->value();                       \
  }                                                         \
  void holder::synchronized_set_##name(int value) {         \
    RELEASE_WRITE_FIELD(this, offset, Smi::FromInt(value)); \
  }

#define NOBARRIER_SMI_ACCESSORS(holder, name, offset)          \
  int holder::nobarrier_##name() {                             \
    Object* value = NOBARRIER_READ_FIELD(this, offset);        \
    return Smi::cast(value)->value();                          \
  }                                                            \
  void holder::nobarrier_set_##name(int value) {               \
    NOBARRIER_WRITE_FIELD(this, offset, Smi::FromInt(value));  \
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


bool Object::IsFixedArrayBase() {
  return IsFixedArray() || IsFixedDoubleArray() || IsConstantPoolArray() ||
         IsFixedTypedArrayBase() || IsExternalArray();
}


// External objects are not extensible, so the map check is enough.
bool Object::IsExternal() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->external_map();
}


bool Object::IsAccessorInfo() {
  return IsExecutableAccessorInfo() || IsDeclaredAccessorInfo();
}


bool Object::IsSmi() {
  return HAS_SMI_TAG(this);
}


bool Object::IsHeapObject() {
  return Internals::HasHeapObjectTag(this);
}


TYPE_CHECKER(HeapNumber, HEAP_NUMBER_TYPE)
TYPE_CHECKER(Symbol, SYMBOL_TYPE)


bool Object::IsString() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() < FIRST_NONSTRING_TYPE;
}


bool Object::IsName() {
  return IsString() || IsSymbol();
}


bool Object::IsUniqueName() {
  return IsInternalizedString() || IsSymbol();
}


bool Object::IsSpecObject() {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() >= FIRST_SPEC_OBJECT_TYPE;
}


bool Object::IsSpecFunction() {
  if (!Object::IsHeapObject()) return false;
  InstanceType type = HeapObject::cast(this)->map()->instance_type();
  return type == JS_FUNCTION_TYPE || type == JS_FUNCTION_PROXY_TYPE;
}


bool Object::IsTemplateInfo() {
  return IsObjectTemplateInfo() || IsFunctionTemplateInfo();
}


bool Object::IsInternalizedString() {
  if (!this->IsHeapObject()) return false;
  uint32_t type = HeapObject::cast(this)->map()->instance_type();
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (type & (kIsNotStringMask | kIsNotInternalizedMask)) ==
      (kStringTag | kInternalizedTag);
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


bool Object::IsSeqOneByteString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsOneByteRepresentation();
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
         String::cast(this)->IsOneByteRepresentation();
}


bool Object::IsExternalTwoByteString() {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsTwoByteRepresentation();
}


bool Object::HasValidElements() {
  // Dictionary is covered under FixedArray.
  return IsFixedArray() || IsFixedDoubleArray() || IsExternalArray() ||
         IsFixedTypedArrayBase();
}


Handle<Object> Object::NewStorageFor(Isolate* isolate,
                                     Handle<Object> object,
                                     Representation representation) {
  if (representation.IsSmi() && object->IsUninitialized()) {
    return handle(Smi::FromInt(0), isolate);
  }
  if (!representation.IsDouble()) return object;
  if (object->IsUninitialized()) {
    return isolate->factory()->NewHeapNumber(0);
  }
  return isolate->factory()->NewHeapNumber(object->Number());
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


bool StringShape::IsInternalized() {
  ASSERT(valid());
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (type_ & (kIsNotStringMask | kIsNotInternalizedMask)) ==
      (kStringTag | kInternalizedTag);
}


bool String::IsOneByteRepresentation() {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kOneByteStringTag;
}


bool String::IsTwoByteRepresentation() {
  uint32_t type = map()->instance_type();
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}


bool String::IsOneByteRepresentationUnderneath() {
  uint32_t type = map()->instance_type();
  STATIC_ASSERT(kIsIndirectStringTag != 0);
  STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
  ASSERT(IsFlat());
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
  ASSERT(IsFlat());
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

STATIC_CHECK(static_cast<uint32_t>(kStringEncodingMask) ==
             Internals::kStringEncodingMask);


bool StringShape::IsSequentialAscii() {
  return full_representation_tag() == (kSeqStringTag | kOneByteStringTag);
}


bool StringShape::IsSequentialTwoByte() {
  return full_representation_tag() == (kSeqStringTag | kTwoByteStringTag);
}


bool StringShape::IsExternalAscii() {
  return full_representation_tag() == (kExternalStringTag | kOneByteStringTag);
}


STATIC_CHECK((kExternalStringTag | kOneByteStringTag) ==
             Internals::kExternalAsciiRepresentationTag);

STATIC_CHECK(v8::String::ASCII_ENCODING == kOneByteStringTag);


bool StringShape::IsExternalTwoByte() {
  return full_representation_tag() == (kExternalStringTag | kTwoByteStringTag);
}


STATIC_CHECK((kExternalStringTag | kTwoByteStringTag) ==
             Internals::kExternalTwoByteRepresentationTag);

STATIC_CHECK(v8::String::TWO_BYTE_ENCODING == kTwoByteStringTag);

uc32 FlatStringReader::Get(int index) {
  ASSERT(0 <= index && index <= length_);
  if (is_ascii_) {
    return static_cast<const byte*>(start_)[index];
  } else {
    return static_cast<const uc16*>(start_)[index];
  }
}


Handle<Object> StringTableShape::AsHandle(Isolate* isolate, HashTableKey* key) {
  return key->AsHandle(isolate);
}


Handle<Object> MapCacheShape::AsHandle(Isolate* isolate, HashTableKey* key) {
  return key->AsHandle(isolate);
}


Handle<Object> CompilationCacheShape::AsHandle(Isolate* isolate,
                                               HashTableKey* key) {
  return key->AsHandle(isolate);
}


Handle<Object> CodeCacheHashTableShape::AsHandle(Isolate* isolate,
                                                 HashTableKey* key) {
  return key->AsHandle(isolate);
}

template <typename Char>
class SequentialStringKey : public HashTableKey {
 public:
  explicit SequentialStringKey(Vector<const Char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

  virtual uint32_t Hash() V8_OVERRIDE {
    hash_field_ = StringHasher::HashSequentialString<Char>(string_.start(),
                                                           string_.length(),
                                                           seed_);

    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }


  virtual uint32_t HashForObject(Object* other) V8_OVERRIDE {
    return String::cast(other)->Hash();
  }

  Vector<const Char> string_;
  uint32_t hash_field_;
  uint32_t seed_;
};


class OneByteStringKey : public SequentialStringKey<uint8_t> {
 public:
  OneByteStringKey(Vector<const uint8_t> str, uint32_t seed)
      : SequentialStringKey<uint8_t>(str, seed) { }

  virtual bool IsMatch(Object* string) V8_OVERRIDE {
    return String::cast(string)->IsOneByteEqualTo(string_);
  }

  virtual Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE;
};


template<class Char>
class SubStringKey : public HashTableKey {
 public:
  SubStringKey(Handle<String> string, int from, int length)
      : string_(string), from_(from), length_(length) {
    if (string_->IsSlicedString()) {
      string_ = Handle<String>(Unslice(*string_, &from_));
    }
    ASSERT(string_->IsSeqString() || string->IsExternalString());
  }

  virtual uint32_t Hash() V8_OVERRIDE {
    ASSERT(length_ >= 0);
    ASSERT(from_ + length_ <= string_->length());
    const Char* chars = GetChars() + from_;
    hash_field_ = StringHasher::HashSequentialString(
        chars, length_, string_->GetHeap()->HashSeed());
    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  virtual uint32_t HashForObject(Object* other) V8_OVERRIDE {
    return String::cast(other)->Hash();
  }

  virtual bool IsMatch(Object* string) V8_OVERRIDE;
  virtual Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE;

 private:
  const Char* GetChars();
  String* Unslice(String* string, int* offset) {
    while (string->IsSlicedString()) {
      SlicedString* sliced = SlicedString::cast(string);
      *offset += sliced->offset();
      string = sliced->parent();
    }
    return string;
  }

  Handle<String> string_;
  int from_;
  int length_;
  uint32_t hash_field_;
};


class TwoByteStringKey : public SequentialStringKey<uc16> {
 public:
  explicit TwoByteStringKey(Vector<const uc16> str, uint32_t seed)
      : SequentialStringKey<uc16>(str, seed) { }

  virtual bool IsMatch(Object* string) V8_OVERRIDE {
    return String::cast(string)->IsTwoByteEqualTo(string_);
  }

  virtual Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE;
};


// Utf8StringKey carries a vector of chars as key.
class Utf8StringKey : public HashTableKey {
 public:
  explicit Utf8StringKey(Vector<const char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

  virtual bool IsMatch(Object* string) V8_OVERRIDE {
    return String::cast(string)->IsUtf8EqualTo(string_);
  }

  virtual uint32_t Hash() V8_OVERRIDE {
    if (hash_field_ != 0) return hash_field_ >> String::kHashShift;
    hash_field_ = StringHasher::ComputeUtf8Hash(string_, seed_, &chars_);
    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  virtual uint32_t HashForObject(Object* other) V8_OVERRIDE {
    return String::cast(other)->Hash();
  }

  virtual Handle<Object> AsHandle(Isolate* isolate) V8_OVERRIDE {
    if (hash_field_ == 0) Hash();
    return isolate->factory()->NewInternalizedStringFromUtf8(
        string_, chars_, hash_field_);
  }

  Vector<const char> string_;
  uint32_t hash_field_;
  int chars_;  // Caches the number of characters when computing the hash code.
  uint32_t seed_;
};


bool Object::IsNumber() {
  return IsSmi() || IsHeapNumber();
}


TYPE_CHECKER(ByteArray, BYTE_ARRAY_TYPE)
TYPE_CHECKER(FreeSpace, FREE_SPACE_TYPE)


bool Object::IsFiller() {
  if (!Object::IsHeapObject()) return false;
  InstanceType instance_type = HeapObject::cast(this)->map()->instance_type();
  return instance_type == FREE_SPACE_TYPE || instance_type == FILLER_TYPE;
}


bool Object::IsExternalArray() {
  if (!Object::IsHeapObject())
    return false;
  InstanceType instance_type =
      HeapObject::cast(this)->map()->instance_type();
  return (instance_type >= FIRST_EXTERNAL_ARRAY_TYPE &&
          instance_type <= LAST_EXTERNAL_ARRAY_TYPE);
}


#define TYPED_ARRAY_TYPE_CHECKER(Type, type, TYPE, ctype, size)               \
  TYPE_CHECKER(External##Type##Array, EXTERNAL_##TYPE##_ARRAY_TYPE)           \
  TYPE_CHECKER(Fixed##Type##Array, FIXED_##TYPE##_ARRAY_TYPE)

TYPED_ARRAYS(TYPED_ARRAY_TYPE_CHECKER)
#undef TYPED_ARRAY_TYPE_CHECKER


bool Object::IsFixedTypedArrayBase() {
  if (!Object::IsHeapObject()) return false;

  InstanceType instance_type =
      HeapObject::cast(this)->map()->instance_type();
  return (instance_type >= FIRST_FIXED_TYPED_ARRAY_TYPE &&
          instance_type <= LAST_FIXED_TYPED_ARRAY_TYPE);
}


bool Object::IsJSReceiver() {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() >= FIRST_JS_RECEIVER_TYPE;
}


bool Object::IsJSObject() {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() >= FIRST_JS_OBJECT_TYPE;
}


bool Object::IsJSProxy() {
  if (!Object::IsHeapObject()) return false;
  InstanceType type = HeapObject::cast(this)->map()->instance_type();
  return FIRST_JS_PROXY_TYPE <= type && type <= LAST_JS_PROXY_TYPE;
}


TYPE_CHECKER(JSFunctionProxy, JS_FUNCTION_PROXY_TYPE)
TYPE_CHECKER(JSSet, JS_SET_TYPE)
TYPE_CHECKER(JSMap, JS_MAP_TYPE)
TYPE_CHECKER(JSSetIterator, JS_SET_ITERATOR_TYPE)
TYPE_CHECKER(JSMapIterator, JS_MAP_ITERATOR_TYPE)
TYPE_CHECKER(JSWeakMap, JS_WEAK_MAP_TYPE)
TYPE_CHECKER(JSWeakSet, JS_WEAK_SET_TYPE)
TYPE_CHECKER(JSContextExtensionObject, JS_CONTEXT_EXTENSION_OBJECT_TYPE)
TYPE_CHECKER(Map, MAP_TYPE)
TYPE_CHECKER(FixedArray, FIXED_ARRAY_TYPE)
TYPE_CHECKER(FixedDoubleArray, FIXED_DOUBLE_ARRAY_TYPE)
TYPE_CHECKER(ConstantPoolArray, CONSTANT_POOL_ARRAY_TYPE)


bool Object::IsJSWeakCollection() {
  return IsJSWeakMap() || IsJSWeakSet();
}


bool Object::IsDescriptorArray() {
  return IsFixedArray();
}


bool Object::IsTransitionArray() {
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


bool Object::IsDependentCode() {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a dependent codes array.
  return true;
}


bool Object::IsContext() {
  if (!Object::IsHeapObject()) return false;
  Map* map = HeapObject::cast(this)->map();
  Heap* heap = map->GetHeap();
  return (map == heap->function_context_map() ||
      map == heap->catch_context_map() ||
      map == heap->with_context_map() ||
      map == heap->native_context_map() ||
      map == heap->block_context_map() ||
      map == heap->module_context_map() ||
      map == heap->global_context_map());
}


bool Object::IsNativeContext() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->native_context_map();
}


bool Object::IsScopeInfo() {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->scope_info_map();
}


TYPE_CHECKER(JSFunction, JS_FUNCTION_TYPE)


template <> inline bool Is<JSFunction>(Object* obj) {
  return obj->IsJSFunction();
}


TYPE_CHECKER(Code, CODE_TYPE)
TYPE_CHECKER(Oddball, ODDBALL_TYPE)
TYPE_CHECKER(Cell, CELL_TYPE)
TYPE_CHECKER(PropertyCell, PROPERTY_CELL_TYPE)
TYPE_CHECKER(SharedFunctionInfo, SHARED_FUNCTION_INFO_TYPE)
TYPE_CHECKER(JSGeneratorObject, JS_GENERATOR_OBJECT_TYPE)
TYPE_CHECKER(JSModule, JS_MODULE_TYPE)
TYPE_CHECKER(JSValue, JS_VALUE_TYPE)
TYPE_CHECKER(JSDate, JS_DATE_TYPE)
TYPE_CHECKER(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)


bool Object::IsStringWrapper() {
  return IsJSValue() && JSValue::cast(this)->value()->IsString();
}


TYPE_CHECKER(Foreign, FOREIGN_TYPE)


bool Object::IsBoolean() {
  return IsOddball() &&
      ((Oddball::cast(this)->kind() & Oddball::kNotBooleanMask) == 0);
}


TYPE_CHECKER(JSArray, JS_ARRAY_TYPE)
TYPE_CHECKER(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)
TYPE_CHECKER(JSTypedArray, JS_TYPED_ARRAY_TYPE)
TYPE_CHECKER(JSDataView, JS_DATA_VIEW_TYPE)


bool Object::IsJSArrayBufferView() {
  return IsJSDataView() || IsJSTypedArray();
}


TYPE_CHECKER(JSRegExp, JS_REGEXP_TYPE)


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
      this != HeapObject::cast(this)->GetHeap()->string_table();
}


bool Object::IsStringTable() {
  return IsHashTable();
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
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    reinterpret_cast<JSFunctionResultCache*>(this)->
        JSFunctionResultCacheVerify();
  }
#endif
  return true;
}


bool Object::IsNormalizedMapCache() {
  return NormalizedMapCache::IsNormalizedMapCache(this);
}


int NormalizedMapCache::GetIndex(Handle<Map> map) {
  return map->Hash() % NormalizedMapCache::kEntries;
}


bool NormalizedMapCache::IsNormalizedMapCache(Object* obj) {
  if (!obj->IsFixedArray()) return false;
  if (FixedArray::cast(obj)->length() != NormalizedMapCache::kEntries) {
    return false;
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    reinterpret_cast<NormalizedMapCache*>(obj)->NormalizedMapCacheVerify();
  }
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


bool Object::IsObjectHashTable() {
  return IsHashTable();
}


bool Object::IsOrderedHashTable() {
  return IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->ordered_hash_table_map();
}


bool Object::IsPrimitive() {
  return IsOddball() || IsNumber() || IsString();
}


bool Object::IsJSGlobalProxy() {
  bool result = IsHeapObject() &&
                (HeapObject::cast(this)->map()->instance_type() ==
                 JS_GLOBAL_PROXY_TYPE);
  ASSERT(!result ||
         HeapObject::cast(this)->map()->is_access_check_needed());
  return result;
}


bool Object::IsGlobalObject() {
  if (!IsHeapObject()) return false;

  InstanceType type = HeapObject::cast(this)->map()->instance_type();
  return type == JS_GLOBAL_OBJECT_TYPE ||
         type == JS_BUILTINS_OBJECT_TYPE;
}


TYPE_CHECKER(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)
TYPE_CHECKER(JSBuiltinsObject, JS_BUILTINS_OBJECT_TYPE)


bool Object::IsUndetectableObject() {
  return IsHeapObject()
    && HeapObject::cast(this)->map()->is_undetectable();
}


bool Object::IsAccessCheckNeeded() {
  if (!IsHeapObject()) return false;
  if (IsJSGlobalProxy()) {
    JSGlobalProxy* proxy = JSGlobalProxy::cast(this);
    GlobalObject* global =
        proxy->GetIsolate()->context()->global_object();
    return proxy->IsDetachedFrom(global);
  }
  return HeapObject::cast(this)->map()->is_access_check_needed();
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


bool Object::IsException() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kException;
}


bool Object::IsUninitialized() {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kUninitialized;
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


bool Object::IsNaN() {
  return this->IsHeapNumber() && std::isnan(HeapNumber::cast(this)->value());
}


MaybeHandle<Smi> Object::ToSmi(Isolate* isolate, Handle<Object> object) {
  if (object->IsSmi()) return Handle<Smi>::cast(object);
  if (object->IsHeapNumber()) {
    double value = Handle<HeapNumber>::cast(object)->value();
    int int_value = FastD2I(value);
    if (value == FastI2D(int_value) && Smi::IsValid(int_value)) {
      return handle(Smi::FromInt(int_value), isolate);
    }
  }
  return Handle<Smi>();
}


MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object) {
  return ToObject(
      isolate, object, handle(isolate->context()->native_context(), isolate));
}


bool Object::HasSpecificClassOf(String* name) {
  return this->IsJSObject() && (JSObject::cast(this)->class_name() == name);
}


MaybeHandle<Object> Object::GetProperty(Handle<Object> object,
                                        Handle<Name> name) {
  PropertyAttributes attributes;
  return GetPropertyWithReceiver(object, object, name, &attributes);
}


MaybeHandle<Object> Object::GetElement(Isolate* isolate,
                                       Handle<Object> object,
                                       uint32_t index) {
  // GetElement can trigger a getter which can cause allocation.
  // This was not always the case. This ASSERT is here to catch
  // leftover incorrect uses.
  ASSERT(AllowHeapAllocation::IsAllowed());
  return Object::GetElementWithReceiver(isolate, object, object, index);
}


MaybeHandle<Object> Object::GetPropertyOrElement(Handle<Object> object,
                                                 Handle<Name> name) {
  uint32_t index;
  Isolate* isolate = name->GetIsolate();
  if (name->AsArrayIndex(&index)) return GetElement(isolate, object, index);
  return GetProperty(object, name);
}


MaybeHandle<Object> Object::GetProperty(Isolate* isolate,
                                        Handle<Object> object,
                                        const char* name) {
  Handle<String> str = isolate->factory()->InternalizeUtf8String(name);
  ASSERT(!str.is_null());
#ifdef DEBUG
  uint32_t index;  // Assert that the name is not an array index.
  ASSERT(!str->AsArrayIndex(&index));
#endif  // DEBUG
  return GetProperty(object, str);
}


MaybeHandle<Object> JSProxy::GetElementWithHandler(Handle<JSProxy> proxy,
                                                   Handle<Object> receiver,
                                                   uint32_t index) {
  return GetPropertyWithHandler(
      proxy, receiver, proxy->GetIsolate()->factory()->Uint32ToString(index));
}


MaybeHandle<Object> JSProxy::SetElementWithHandler(Handle<JSProxy> proxy,
                                                   Handle<JSReceiver> receiver,
                                                   uint32_t index,
                                                   Handle<Object> value,
                                                   StrictMode strict_mode) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return SetPropertyWithHandler(
      proxy, receiver, name, value, NONE, strict_mode);
}


bool JSProxy::HasElementWithHandler(Handle<JSProxy> proxy, uint32_t index) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return HasPropertyWithHandler(proxy, name);
}


#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<byte*>(p) + offset - kHeapObjectTag)

#define READ_FIELD(p, offset) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)))

#define ACQUIRE_READ_FIELD(p, offset)                                    \
  reinterpret_cast<Object*>(                                             \
      Acquire_Load(reinterpret_cast<AtomicWord*>(FIELD_ADDR(p, offset))))

#define NOBARRIER_READ_FIELD(p, offset)                                     \
  reinterpret_cast<Object*>(                                                \
      NoBarrier_Load(reinterpret_cast<AtomicWord*>(FIELD_ADDR(p, offset))))

#define WRITE_FIELD(p, offset, value) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)) = value)

#define RELEASE_WRITE_FIELD(p, offset, value)                           \
  Release_Store(reinterpret_cast<AtomicWord*>(FIELD_ADDR(p, offset)),   \
                reinterpret_cast<AtomicWord>(value));

#define NOBARRIER_WRITE_FIELD(p, offset, value)                            \
  NoBarrier_Store(reinterpret_cast<AtomicWord*>(FIELD_ADDR(p, offset)),    \
                  reinterpret_cast<AtomicWord>(value));

#define WRITE_BARRIER(heap, object, offset, value)                      \
  heap->incremental_marking()->RecordWrite(                             \
      object, HeapObject::RawField(object, offset), value);             \
  if (heap->InNewSpace(value)) {                                        \
    heap->RecordWrite(object->address(), offset);                       \
  }

#define CONDITIONAL_WRITE_BARRIER(heap, object, offset, value, mode)    \
  if (mode == UPDATE_WRITE_BARRIER) {                                   \
    heap->incremental_marking()->RecordWrite(                           \
      object, HeapObject::RawField(object, offset), value);             \
    if (heap->InNewSpace(value)) {                                      \
      heap->RecordWrite(object->address(), offset);                     \
    }                                                                   \
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

#define READ_INT32_FIELD(p, offset) \
  (*reinterpret_cast<int32_t*>(FIELD_ADDR(p, offset)))

#define WRITE_INT32_FIELD(p, offset, value) \
  (*reinterpret_cast<int32_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_INT64_FIELD(p, offset) \
  (*reinterpret_cast<int64_t*>(FIELD_ADDR(p, offset)))

#define WRITE_INT64_FIELD(p, offset, value) \
  (*reinterpret_cast<int64_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_SHORT_FIELD(p, offset) \
  (*reinterpret_cast<uint16_t*>(FIELD_ADDR(p, offset)))

#define WRITE_SHORT_FIELD(p, offset, value) \
  (*reinterpret_cast<uint16_t*>(FIELD_ADDR(p, offset)) = value)

#define READ_BYTE_FIELD(p, offset) \
  (*reinterpret_cast<byte*>(FIELD_ADDR(p, offset)))

#define NOBARRIER_READ_BYTE_FIELD(p, offset)               \
  static_cast<byte>(NoBarrier_Load(                        \
      reinterpret_cast<Atomic8*>(FIELD_ADDR(p, offset))) )

#define WRITE_BYTE_FIELD(p, offset, value) \
  (*reinterpret_cast<byte*>(FIELD_ADDR(p, offset)) = value)

#define NOBARRIER_WRITE_BYTE_FIELD(p, offset, value)                 \
  NoBarrier_Store(reinterpret_cast<Atomic8*>(FIELD_ADDR(p, offset)), \
                  static_cast<Atomic8>(value));

Object** HeapObject::RawField(HeapObject* obj, int byte_offset) {
  return &READ_FIELD(obj, byte_offset);
}


int Smi::value() {
  return Internals::SmiValue(this);
}


Smi* Smi::FromInt(int value) {
  ASSERT(Smi::IsValid(value));
  return reinterpret_cast<Smi*>(Internals::IntToSmi(value));
}


Smi* Smi::FromIntptr(intptr_t value) {
  ASSERT(Smi::IsValid(value));
  int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
  return reinterpret_cast<Smi*>((value << smi_shift_bits) | kSmiTag);
}


bool Smi::IsValid(intptr_t value) {
  bool result = Internals::IsValidSmi(value);
  ASSERT_EQ(result, value >= kMinValue && value <= kMaxValue);
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


#ifdef VERIFY_HEAP
void HeapObject::VerifyObjectField(int offset) {
  VerifyPointer(READ_FIELD(this, offset));
}

void HeapObject::VerifySmiField(int offset) {
  CHECK(READ_FIELD(this, offset)->IsSmi());
}
#endif


Heap* HeapObject::GetHeap() {
  Heap* heap =
      MemoryChunk::FromAddress(reinterpret_cast<Address>(this))->heap();
  SLOW_ASSERT(heap != NULL);
  return heap;
}


Isolate* HeapObject::GetIsolate() {
  return GetHeap()->isolate();
}


Map* HeapObject::map() {
  return map_word().ToMap();
}


void HeapObject::set_map(Map* value) {
  set_map_word(MapWord::FromMap(value));
  if (value != NULL) {
    // TODO(1600) We are passing NULL as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, NULL, value);
  }
}


Map* HeapObject::synchronized_map() {
  return synchronized_map_word().ToMap();
}


void HeapObject::synchronized_set_map(Map* value) {
  synchronized_set_map_word(MapWord::FromMap(value));
  if (value != NULL) {
    // TODO(1600) We are passing NULL as a slot because maps can never be on
    // evacuation candidate.
    value->GetHeap()->incremental_marking()->RecordWrite(this, NULL, value);
  }
}


void HeapObject::synchronized_set_map_no_write_barrier(Map* value) {
  synchronized_set_map_word(MapWord::FromMap(value));
}


// Unsafe accessor omitting write barrier.
void HeapObject::set_map_no_write_barrier(Map* value) {
  set_map_word(MapWord::FromMap(value));
}


MapWord HeapObject::map_word() {
  return MapWord(
      reinterpret_cast<uintptr_t>(NOBARRIER_READ_FIELD(this, kMapOffset)));
}


void HeapObject::set_map_word(MapWord map_word) {
  NOBARRIER_WRITE_FIELD(
      this, kMapOffset, reinterpret_cast<Object*>(map_word.value_));
}


MapWord HeapObject::synchronized_map_word() {
  return MapWord(
      reinterpret_cast<uintptr_t>(ACQUIRE_READ_FIELD(this, kMapOffset)));
}


void HeapObject::synchronized_set_map_word(MapWord map_word) {
  RELEASE_WRITE_FIELD(
      this, kMapOffset, reinterpret_cast<Object*>(map_word.value_));
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


void HeapObject::IterateNextCodeLink(ObjectVisitor* v, int offset) {
  v->VisitNextCodeLink(reinterpret_cast<Object**>(FIELD_ADDR(this, offset)));
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


FixedArrayBase* JSObject::elements() {
  Object* array = READ_FIELD(this, kElementsOffset);
  return static_cast<FixedArrayBase*>(array);
}


void JSObject::ValidateElements(Handle<JSObject> object) {
#ifdef ENABLE_SLOW_ASSERTS
  if (FLAG_enable_slow_asserts) {
    ElementsAccessor* accessor = object->GetElementsAccessor();
    accessor->Validate(object);
  }
#endif
}


void AllocationSite::Initialize() {
  set_transition_info(Smi::FromInt(0));
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::FromInt(0));
  set_pretenure_data(Smi::FromInt(0));
  set_pretenure_create_count(Smi::FromInt(0));
  set_dependent_code(DependentCode::cast(GetHeap()->empty_fixed_array()),
                     SKIP_WRITE_BARRIER);
}


void AllocationSite::MarkZombie() {
  ASSERT(!IsZombie());
  Initialize();
  set_pretenure_decision(kZombie);
}


// Heuristic: We only need to create allocation site info if the boilerplate
// elements kind is the initial elements kind.
AllocationSiteMode AllocationSite::GetMode(
    ElementsKind boilerplate_elements_kind) {
  if (FLAG_pretenuring_call_new ||
      IsFastSmiElementsKind(boilerplate_elements_kind)) {
    return TRACK_ALLOCATION_SITE;
  }

  return DONT_TRACK_ALLOCATION_SITE;
}


AllocationSiteMode AllocationSite::GetMode(ElementsKind from,
                                           ElementsKind to) {
  if (FLAG_pretenuring_call_new ||
      (IsFastSmiElementsKind(from) &&
       IsMoreGeneralElementsKindTransition(from, to))) {
    return TRACK_ALLOCATION_SITE;
  }

  return DONT_TRACK_ALLOCATION_SITE;
}


inline bool AllocationSite::CanTrack(InstanceType type) {
  if (FLAG_allocation_site_pretenuring) {
    return type == JS_ARRAY_TYPE ||
        type == JS_OBJECT_TYPE ||
        type < FIRST_NONSTRING_TYPE;
  }
  return type == JS_ARRAY_TYPE;
}


inline DependentCode::DependencyGroup AllocationSite::ToDependencyGroup(
    Reason reason) {
  switch (reason) {
    case TENURING:
      return DependentCode::kAllocationSiteTenuringChangedGroup;
      break;
    case TRANSITIONS:
      return DependentCode::kAllocationSiteTransitionChangedGroup;
      break;
  }
  UNREACHABLE();
  return DependentCode::kAllocationSiteTransitionChangedGroup;
}


inline void AllocationSite::set_memento_found_count(int count) {
  int value = pretenure_data()->value();
  // Verify that we can count more mementos than we can possibly find in one
  // new space collection.
  ASSERT((GetHeap()->MaxSemiSpaceSize() /
          (StaticVisitorBase::kMinObjectSizeInWords * kPointerSize +
           AllocationMemento::kSize)) < MementoFoundCountBits::kMax);
  ASSERT(count < MementoFoundCountBits::kMax);
  set_pretenure_data(
      Smi::FromInt(MementoFoundCountBits::update(value, count)),
      SKIP_WRITE_BARRIER);
}

inline bool AllocationSite::IncrementMementoFoundCount() {
  if (IsZombie()) return false;

  int value = memento_found_count();
  set_memento_found_count(value + 1);
  return value == 0;
}


inline void AllocationSite::IncrementMementoCreateCount() {
  ASSERT(FLAG_allocation_site_pretenuring);
  int value = memento_create_count();
  set_memento_create_count(value + 1);
}


inline bool AllocationSite::DigestPretenuringFeedback() {
  bool decision_changed = false;
  int create_count = memento_create_count();
  int found_count = memento_found_count();
  bool minimum_mementos_created = create_count >= kPretenureMinimumCreated;
  double ratio =
      minimum_mementos_created || FLAG_trace_pretenuring_statistics ?
          static_cast<double>(found_count) / create_count : 0.0;
  PretenureFlag current_mode = GetPretenureMode();

  if (minimum_mementos_created) {
    PretenureDecision result = ratio >= kPretenureRatio
        ? kTenure
        : kDontTenure;
    set_pretenure_decision(result);
    if (current_mode != GetPretenureMode()) {
      decision_changed = true;
      set_deopt_dependent_code(true);
    }
  }

  if (FLAG_trace_pretenuring_statistics) {
    PrintF(
        "AllocationSite(%p): (created, found, ratio) (%d, %d, %f) %s => %s\n",
         static_cast<void*>(this), create_count, found_count, ratio,
         current_mode == TENURED ? "tenured" : "not tenured",
         GetPretenureMode() == TENURED ? "tenured" : "not tenured");
  }

  // Clear feedback calculation fields until the next gc.
  set_memento_found_count(0);
  set_memento_create_count(0);
  return decision_changed;
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
  ElementsKind current_kind = object->map()->elements_kind();
  ElementsKind target_kind = current_kind;
  {
    DisallowHeapAllocation no_allocation;
    ASSERT(mode != ALLOW_COPIED_DOUBLE_ELEMENTS);
    bool is_holey = IsFastHoleyElementsKind(current_kind);
    if (current_kind == FAST_HOLEY_ELEMENTS) return;
    Heap* heap = object->GetHeap();
    Object* the_hole = heap->the_hole_value();
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
    ASSERT(elements->map() == heap->fixed_array_map() ||
           elements->map() == heap->fixed_cow_array_map());
    if (mode == ALLOW_COPIED_DOUBLE_ELEMENTS) {
      mode = DONT_ALLOW_DOUBLE_ELEMENTS;
    }
    Object** objects =
        Handle<FixedArray>::cast(elements)->GetFirstElementAddress();
    EnsureCanContainElements(object, objects, length, mode);
    return;
  }

  ASSERT(mode == ALLOW_COPIED_DOUBLE_ELEMENTS);
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
  ASSERT((object->map()->has_fast_smi_or_object_elements() ||
          (*value == object->GetHeap()->empty_fixed_array())) ==
         (value->map() == object->GetHeap()->fixed_array_map() ||
          value->map() == object->GetHeap()->fixed_cow_array_map()));
  ASSERT((*value == object->GetHeap()->empty_fixed_array()) ||
         (object->map()->has_fast_double_elements() ==
          value->IsFixedDoubleArray()));
  object->set_elements(*value);
}


void JSObject::set_elements(FixedArrayBase* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kElementsOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kElementsOffset, value, mode);
}


void JSObject::initialize_properties() {
  ASSERT(!GetHeap()->InNewSpace(GetHeap()->empty_fixed_array()));
  WRITE_FIELD(this, kPropertiesOffset, GetHeap()->empty_fixed_array());
}


void JSObject::initialize_elements() {
  FixedArrayBase* elements = map()->GetInitialElements();
  WRITE_FIELD(this, kElementsOffset, elements);
}


Handle<String> JSObject::ExpectedTransitionKey(Handle<Map> map) {
  DisallowHeapAllocation no_gc;
  if (!map->HasTransitionArray()) return Handle<String>::null();
  TransitionArray* transitions = map->transitions();
  if (!transitions->IsSimpleTransition()) return Handle<String>::null();
  int transition = TransitionArray::kSimpleTransitionIndex;
  PropertyDetails details = transitions->GetTargetDetails(transition);
  Name* name = transitions->GetKey(transition);
  if (details.type() != FIELD) return Handle<String>::null();
  if (details.attributes() != NONE) return Handle<String>::null();
  if (!name->IsString()) return Handle<String>::null();
  return Handle<String>(String::cast(name));
}


Handle<Map> JSObject::ExpectedTransitionTarget(Handle<Map> map) {
  ASSERT(!ExpectedTransitionKey(map).is_null());
  return Handle<Map>(map->transitions()->GetTarget(
      TransitionArray::kSimpleTransitionIndex));
}


Handle<Map> JSObject::FindTransitionToField(Handle<Map> map, Handle<Name> key) {
  DisallowHeapAllocation no_allocation;
  if (!map->HasTransitionArray()) return Handle<Map>::null();
  TransitionArray* transitions = map->transitions();
  int transition = transitions->Search(*key);
  if (transition == TransitionArray::kNotFound) return Handle<Map>::null();
  PropertyDetails target_details = transitions->GetTargetDetails(transition);
  if (target_details.type() != FIELD) return Handle<Map>::null();
  if (target_details.attributes() != NONE) return Handle<Map>::null();
  return Handle<Map>(transitions->GetTarget(transition));
}


ACCESSORS(Oddball, to_string, String, kToStringOffset)
ACCESSORS(Oddball, to_number, Object, kToNumberOffset)


byte Oddball::kind() {
  return Smi::cast(READ_FIELD(this, kKindOffset))->value();
}


void Oddball::set_kind(byte value) {
  WRITE_FIELD(this, kKindOffset, Smi::FromInt(value));
}


Object* Cell::value() {
  return READ_FIELD(this, kValueOffset);
}


void Cell::set_value(Object* val, WriteBarrierMode ignored) {
  // The write barrier is not used for global property cells.
  ASSERT(!val->IsPropertyCell() && !val->IsCell());
  WRITE_FIELD(this, kValueOffset, val);
}

ACCESSORS(PropertyCell, dependent_code, DependentCode, kDependentCodeOffset)

Object* PropertyCell::type_raw() {
  return READ_FIELD(this, kTypeOffset);
}


void PropertyCell::set_type_raw(Object* val, WriteBarrierMode ignored) {
  WRITE_FIELD(this, kTypeOffset, val);
}


int JSObject::GetHeaderSize() {
  InstanceType type = map()->instance_type();
  // Check for the most common kind of JavaScript object before
  // falling into the generic switch. This speeds up the internal
  // field operations considerably on average.
  if (type == JS_OBJECT_TYPE) return JSObject::kHeaderSize;
  switch (type) {
    case JS_GENERATOR_OBJECT_TYPE:
      return JSGeneratorObject::kSize;
    case JS_MODULE_TYPE:
      return JSModule::kSize;
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
    case JS_REGEXP_TYPE:
      return JSRegExp::kSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kSize;
    default:
      // TODO(jkummerow): Re-enable this. Blink currently hits this
      // from its CustomElementConstructorBuilder.
      // UNREACHABLE();
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
  WRITE_BARRIER(GetHeap(), this, offset, value);
}


void JSObject::SetInternalField(int index, Smi* value) {
  ASSERT(index < GetInternalFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  int offset = GetHeaderSize() + (kPointerSize * index);
  WRITE_FIELD(this, offset, value);
}


// Access fast-case object properties at index. The use of these routines
// is needed to correctly distinguish between properties stored in-object and
// properties stored in the properties array.
Object* JSObject::RawFastPropertyAt(int index) {
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


void JSObject::FastPropertyAtPut(int index, Object* value) {
  // Adjust for the number of properties stored in the object.
  index -= map()->inobject_properties();
  if (index < 0) {
    int offset = map()->instance_size() + (index * kPointerSize);
    WRITE_FIELD(this, offset, value);
    WRITE_BARRIER(GetHeap(), this, offset, value);
  } else {
    ASSERT(index < properties()->length());
    properties()->set(index, value);
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



void JSObject::InitializeBody(Map* map,
                              Object* pre_allocated_value,
                              Object* filler_value) {
  ASSERT(!filler_value->IsHeapObject() ||
         !GetHeap()->InNewSpace(filler_value));
  ASSERT(!pre_allocated_value->IsHeapObject() ||
         !GetHeap()->InNewSpace(pre_allocated_value));
  int size = map->instance_size();
  int offset = kHeaderSize;
  if (filler_value != pre_allocated_value) {
    int pre_allocated = map->pre_allocated_property_fields();
    ASSERT(pre_allocated * kPointerSize + kHeaderSize <= size);
    for (int i = 0; i < pre_allocated; i++) {
      WRITE_FIELD(this, offset, pre_allocated_value);
      offset += kPointerSize;
    }
  }
  while (offset < size) {
    WRITE_FIELD(this, offset, filler_value);
    offset += kPointerSize;
  }
}


bool JSObject::HasFastProperties() {
  ASSERT(properties()->IsDictionary() == map()->is_dictionary_map());
  return !properties()->IsDictionary();
}


bool JSObject::TooManyFastProperties(StoreFromKeyed store_mode) {
  // Allow extra fast properties if the object has more than
  // kFastPropertiesSoftLimit in-object properties. When this is the case, it is
  // very unlikely that the object is being used as a dictionary and there is a
  // good chance that allowing more map transitions will be worth it.
  Map* map = this->map();
  if (map->unused_property_fields() != 0) return false;

  int inobject = map->inobject_properties();

  int limit;
  if (store_mode == CERTAINLY_NOT_STORE_FROM_KEYED) {
    limit = Max(inobject, kMaxFastProperties);
  } else {
    limit = Max(inobject, kFastPropertiesSoftLimit);
  }
  return properties()->length() > limit;
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
  if (index >= static_cast<uint32_t>(str->length())) return false;

  return true;
}


void Object::VerifyApiCallResultType() {
#if ENABLE_EXTRA_CHECKS
  if (!(IsSmi() ||
        IsString() ||
        IsSymbol() ||
        IsSpecObject() ||
        IsHeapNumber() ||
        IsUndefined() ||
        IsTrue() ||
        IsFalse() ||
        IsNull())) {
    FATAL("API call returned invalid object");
  }
#endif  // ENABLE_EXTRA_CHECKS
}


FixedArrayBase* FixedArrayBase::cast(Object* object) {
  ASSERT(object->IsFixedArrayBase());
  return reinterpret_cast<FixedArrayBase*>(object);
}


Object* FixedArray::get(int index) {
  SLOW_ASSERT(index >= 0 && index < this->length());
  return READ_FIELD(this, kHeaderSize + index * kPointerSize);
}


Handle<Object> FixedArray::get(Handle<FixedArray> array, int index) {
  return handle(array->get(index), array->GetIsolate());
}


bool FixedArray::is_the_hole(int index) {
  return get(index) == GetHeap()->the_hole_value();
}


void FixedArray::set(int index, Smi* value) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  ASSERT(reinterpret_cast<Object*>(value)->IsSmi());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
}


void FixedArray::set(int index, Object* value) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(GetHeap(), this, offset, value);
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
  ASSERT(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  ASSERT(index >= 0 && index < this->length());
  double result = READ_DOUBLE_FIELD(this, kHeaderSize + index * kDoubleSize);
  ASSERT(!is_the_hole_nan(result));
  return result;
}

int64_t FixedDoubleArray::get_representation(int index) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  ASSERT(index >= 0 && index < this->length());
  return READ_INT64_FIELD(this, kHeaderSize + index * kDoubleSize);
}


Handle<Object> FixedDoubleArray::get(Handle<FixedDoubleArray> array,
                                     int index) {
  if (array->is_the_hole(index)) {
    return array->GetIsolate()->factory()->the_hole_value();
  } else {
    return array->GetIsolate()->factory()->NewNumber(array->get_scalar(index));
  }
}


void FixedDoubleArray::set(int index, double value) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  if (std::isnan(value)) value = canonical_not_the_hole_nan_as_double();
  WRITE_DOUBLE_FIELD(this, offset, value);
}


void FixedDoubleArray::set_the_hole(int index) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  WRITE_DOUBLE_FIELD(this, offset, hole_nan_as_double());
}


bool FixedDoubleArray::is_the_hole(int index) {
  int offset = kHeaderSize + index * kDoubleSize;
  return is_the_hole_nan(READ_DOUBLE_FIELD(this, offset));
}


double* FixedDoubleArray::data_start() {
  return reinterpret_cast<double*>(FIELD_ADDR(this, kHeaderSize));
}


void FixedDoubleArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}


void ConstantPoolArray::set_weak_object_state(
      ConstantPoolArray::WeakObjectState state) {
  int old_layout_field = READ_INT_FIELD(this, kArrayLayoutOffset);
  int new_layout_field = WeakObjectStateField::update(old_layout_field, state);
  WRITE_INT_FIELD(this, kArrayLayoutOffset, new_layout_field);
}


ConstantPoolArray::WeakObjectState ConstantPoolArray::get_weak_object_state() {
  int layout_field = READ_INT_FIELD(this, kArrayLayoutOffset);
  return WeakObjectStateField::decode(layout_field);
}


int ConstantPoolArray::first_int64_index() {
  return 0;
}


int ConstantPoolArray::first_code_ptr_index() {
  int layout_field = READ_INT_FIELD(this, kArrayLayoutOffset);
  return first_int64_index() +
      NumberOfInt64EntriesField::decode(layout_field);
}


int ConstantPoolArray::first_heap_ptr_index() {
  int layout_field = READ_INT_FIELD(this, kArrayLayoutOffset);
  return first_code_ptr_index() +
      NumberOfCodePtrEntriesField::decode(layout_field);
}


int ConstantPoolArray::first_int32_index() {
  int layout_field = READ_INT_FIELD(this, kArrayLayoutOffset);
  return first_heap_ptr_index() +
      NumberOfHeapPtrEntriesField::decode(layout_field);
}


int ConstantPoolArray::count_of_int64_entries() {
  return first_code_ptr_index();
}


int ConstantPoolArray::count_of_code_ptr_entries() {
  return first_heap_ptr_index() - first_code_ptr_index();
}


int ConstantPoolArray::count_of_heap_ptr_entries() {
  return first_int32_index() - first_heap_ptr_index();
}


int ConstantPoolArray::count_of_int32_entries() {
  return length() - first_int32_index();
}


void ConstantPoolArray::Init(int number_of_int64_entries,
                             int number_of_code_ptr_entries,
                             int number_of_heap_ptr_entries,
                             int number_of_int32_entries) {
  set_length(number_of_int64_entries +
             number_of_code_ptr_entries +
             number_of_heap_ptr_entries +
             number_of_int32_entries);
  int layout_field =
      NumberOfInt64EntriesField::encode(number_of_int64_entries) |
      NumberOfCodePtrEntriesField::encode(number_of_code_ptr_entries) |
      NumberOfHeapPtrEntriesField::encode(number_of_heap_ptr_entries) |
      WeakObjectStateField::encode(NO_WEAK_OBJECTS);
  WRITE_INT_FIELD(this, kArrayLayoutOffset, layout_field);
}


int64_t ConstantPoolArray::get_int64_entry(int index) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= 0 && index < first_code_ptr_index());
  return READ_INT64_FIELD(this, OffsetOfElementAt(index));
}

double ConstantPoolArray::get_int64_entry_as_double(int index) {
  STATIC_ASSERT(kDoubleSize == kInt64Size);
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= 0 && index < first_code_ptr_index());
  return READ_DOUBLE_FIELD(this, OffsetOfElementAt(index));
}


Address ConstantPoolArray::get_code_ptr_entry(int index) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_code_ptr_index() && index < first_heap_ptr_index());
  return reinterpret_cast<Address>(READ_FIELD(this, OffsetOfElementAt(index)));
}


Object* ConstantPoolArray::get_heap_ptr_entry(int index) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_heap_ptr_index() && index < first_int32_index());
  return READ_FIELD(this, OffsetOfElementAt(index));
}


int32_t ConstantPoolArray::get_int32_entry(int index) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_int32_index() && index < length());
  return READ_INT32_FIELD(this, OffsetOfElementAt(index));
}


void ConstantPoolArray::set(int index, Address value) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_code_ptr_index() && index < first_heap_ptr_index());
  WRITE_FIELD(this, OffsetOfElementAt(index), reinterpret_cast<Object*>(value));
}


void ConstantPoolArray::set(int index, Object* value) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_code_ptr_index() && index < first_int32_index());
  WRITE_FIELD(this, OffsetOfElementAt(index), value);
  WRITE_BARRIER(GetHeap(), this, OffsetOfElementAt(index), value);
}


void ConstantPoolArray::set(int index, int64_t value) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_int64_index() && index < first_code_ptr_index());
  WRITE_INT64_FIELD(this, OffsetOfElementAt(index), value);
}


void ConstantPoolArray::set(int index, double value) {
  STATIC_ASSERT(kDoubleSize == kInt64Size);
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= first_int64_index() && index < first_code_ptr_index());
  WRITE_DOUBLE_FIELD(this, OffsetOfElementAt(index), value);
}


void ConstantPoolArray::set(int index, int32_t value) {
  ASSERT(map() == GetHeap()->constant_pool_array_map());
  ASSERT(index >= this->first_int32_index() && index < length());
  WRITE_INT32_FIELD(this, OffsetOfElementAt(index), value);
}


WriteBarrierMode HeapObject::GetWriteBarrierMode(
    const DisallowHeapAllocation& promise) {
  Heap* heap = GetHeap();
  if (heap->incremental_marking()->IsMarking()) return UPDATE_WRITE_BARRIER;
  if (heap->InNewSpace(this)) return SKIP_WRITE_BARRIER;
  return UPDATE_WRITE_BARRIER;
}


void FixedArray::set(int index,
                     Object* value,
                     WriteBarrierMode mode) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);
}


void FixedArray::NoIncrementalWriteBarrierSet(FixedArray* array,
                                              int index,
                                              Object* value) {
  ASSERT(array->map() != array->GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < array->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(array, offset, value);
  Heap* heap = array->GetHeap();
  if (heap->InNewSpace(value)) {
    heap->RecordWrite(array->address(), offset);
  }
}


void FixedArray::NoWriteBarrierSet(FixedArray* array,
                                   int index,
                                   Object* value) {
  ASSERT(array->map() != array->GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < array->length());
  ASSERT(!array->GetHeap()->InNewSpace(value));
  WRITE_FIELD(array, kHeaderSize + index * kPointerSize, value);
}


void FixedArray::set_undefined(int index) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!GetHeap()->InNewSpace(GetHeap()->undefined_value()));
  WRITE_FIELD(this,
              kHeaderSize + index * kPointerSize,
              GetHeap()->undefined_value());
}


void FixedArray::set_null(int index) {
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!GetHeap()->InNewSpace(GetHeap()->null_value()));
  WRITE_FIELD(this,
              kHeaderSize + index * kPointerSize,
              GetHeap()->null_value());
}


void FixedArray::set_the_hole(int index) {
  ASSERT(map() != GetHeap()->fixed_cow_array_map());
  ASSERT(index >= 0 && index < this->length());
  ASSERT(!GetHeap()->InNewSpace(GetHeap()->the_hole_value()));
  WRITE_FIELD(this,
              kHeaderSize + index * kPointerSize,
              GetHeap()->the_hole_value());
}


void FixedArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}


Object** FixedArray::data_start() {
  return HeapObject::RawField(this, kHeaderSize);
}


bool DescriptorArray::IsEmpty() {
  ASSERT(length() >= kFirstIndex ||
         this == GetHeap()->empty_descriptor_array());
  return length() < kFirstIndex;
}


void DescriptorArray::SetNumberOfDescriptors(int number_of_descriptors) {
  WRITE_FIELD(
      this, kDescriptorLengthOffset, Smi::FromInt(number_of_descriptors));
}


// Perform a binary search in a fixed array. Low and high are entry indices. If
// there are three entries in this array it should be called with low=0 and
// high=2.
template<SearchMode search_mode, typename T>
int BinarySearch(T* array, Name* name, int low, int high, int valid_entries) {
  uint32_t hash = name->Hash();
  int limit = high;

  ASSERT(low <= high);

  while (low != high) {
    int mid = (low + high) / 2;
    Name* mid_name = array->GetSortedKey(mid);
    uint32_t mid_hash = mid_name->Hash();

    if (mid_hash >= hash) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }

  for (; low <= limit; ++low) {
    int sort_index = array->GetSortedKeyIndex(low);
    Name* entry = array->GetKey(sort_index);
    if (entry->Hash() != hash) break;
    if (entry->Equals(name)) {
      if (search_mode == ALL_ENTRIES || sort_index < valid_entries) {
        return sort_index;
      }
      return T::kNotFound;
    }
  }

  return T::kNotFound;
}


// Perform a linear search in this fixed array. len is the number of entry
// indices that are valid.
template<SearchMode search_mode, typename T>
int LinearSearch(T* array, Name* name, int len, int valid_entries) {
  uint32_t hash = name->Hash();
  if (search_mode == ALL_ENTRIES) {
    for (int number = 0; number < len; number++) {
      int sorted_index = array->GetSortedKeyIndex(number);
      Name* entry = array->GetKey(sorted_index);
      uint32_t current_hash = entry->Hash();
      if (current_hash > hash) break;
      if (current_hash == hash && entry->Equals(name)) return sorted_index;
    }
  } else {
    ASSERT(len >= valid_entries);
    for (int number = 0; number < valid_entries; number++) {
      Name* entry = array->GetKey(number);
      uint32_t current_hash = entry->Hash();
      if (current_hash == hash && entry->Equals(name)) return number;
    }
  }
  return T::kNotFound;
}


template<SearchMode search_mode, typename T>
int Search(T* array, Name* name, int valid_entries) {
  if (search_mode == VALID_ENTRIES) {
    SLOW_ASSERT(array->IsSortedNoDuplicates(valid_entries));
  } else {
    SLOW_ASSERT(array->IsSortedNoDuplicates());
  }

  int nof = array->number_of_entries();
  if (nof == 0) return T::kNotFound;

  // Fast case: do linear search for small arrays.
  const int kMaxElementsForLinearSearch = 8;
  if ((search_mode == ALL_ENTRIES &&
       nof <= kMaxElementsForLinearSearch) ||
      (search_mode == VALID_ENTRIES &&
       valid_entries <= (kMaxElementsForLinearSearch * 3))) {
    return LinearSearch<search_mode>(array, name, nof, valid_entries);
  }

  // Slow case: perform binary search.
  return BinarySearch<search_mode>(array, name, 0, nof - 1, valid_entries);
}


int DescriptorArray::Search(Name* name, int valid_descriptors) {
  return internal::Search<VALID_ENTRIES>(this, name, valid_descriptors);
}


int DescriptorArray::SearchWithCache(Name* name, Map* map) {
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return kNotFound;

  DescriptorLookupCache* cache = GetIsolate()->descriptor_lookup_cache();
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


void Map::LookupDescriptor(JSObject* holder,
                           Name* name,
                           LookupResult* result) {
  DescriptorArray* descriptors = this->instance_descriptors();
  int number = descriptors->SearchWithCache(name, this);
  if (number == DescriptorArray::kNotFound) return result->NotFound();
  result->DescriptorResult(holder, descriptors->GetDetails(number), number);
}


void Map::LookupTransition(JSObject* holder,
                           Name* name,
                           LookupResult* result) {
  int transition_index = this->SearchTransition(name);
  if (transition_index == TransitionArray::kNotFound) return result->NotFound();
  result->TransitionResult(holder, this->GetTransition(transition_index));
}


FixedArrayBase* Map::GetInitialElements() {
  if (has_fast_smi_or_object_elements() ||
      has_fast_double_elements()) {
    ASSERT(!GetHeap()->InNewSpace(GetHeap()->empty_fixed_array()));
    return GetHeap()->empty_fixed_array();
  } else if (has_external_array_elements()) {
    ExternalArray* empty_array = GetHeap()->EmptyExternalArrayForMap(this);
    ASSERT(!GetHeap()->InNewSpace(empty_array));
    return empty_array;
  } else if (has_fixed_typed_array_elements()) {
    FixedTypedArrayBase* empty_array =
      GetHeap()->EmptyFixedTypedArrayForMap(this);
    ASSERT(!GetHeap()->InNewSpace(empty_array));
    return empty_array;
  } else {
    UNREACHABLE();
  }
  return NULL;
}


Object** DescriptorArray::GetKeySlot(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return RawFieldOfElementAt(ToKeyIndex(descriptor_number));
}


Object** DescriptorArray::GetDescriptorStartSlot(int descriptor_number) {
  return GetKeySlot(descriptor_number);
}


Object** DescriptorArray::GetDescriptorEndSlot(int descriptor_number) {
  return GetValueSlot(descriptor_number - 1) + 1;
}


Name* DescriptorArray::GetKey(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
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


void DescriptorArray::SetRepresentation(int descriptor_index,
                                        Representation representation) {
  ASSERT(!representation.IsNone());
  PropertyDetails details = GetDetails(descriptor_index);
  set(ToDetailsIndex(descriptor_index),
      details.CopyWithRepresentation(representation).AsSmi());
}


Object** DescriptorArray::GetValueSlot(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return RawFieldOfElementAt(ToValueIndex(descriptor_number));
}


Object* DescriptorArray::GetValue(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  return get(ToValueIndex(descriptor_number));
}


void DescriptorArray::SetValue(int descriptor_index, Object* value) {
  set(ToValueIndex(descriptor_index), value);
}


PropertyDetails DescriptorArray::GetDetails(int descriptor_number) {
  ASSERT(descriptor_number < number_of_descriptors());
  Object* details = get(ToDetailsIndex(descriptor_number));
  return PropertyDetails(Smi::cast(details));
}


PropertyType DescriptorArray::GetType(int descriptor_number) {
  return GetDetails(descriptor_number).type();
}


int DescriptorArray::GetFieldIndex(int descriptor_number) {
  ASSERT(GetDetails(descriptor_number).type() == FIELD);
  return GetDetails(descriptor_number).field_index();
}


HeapType* DescriptorArray::GetFieldType(int descriptor_number) {
  ASSERT(GetDetails(descriptor_number).type() == FIELD);
  return HeapType::cast(GetValue(descriptor_number));
}


Object* DescriptorArray::GetConstant(int descriptor_number) {
  return GetValue(descriptor_number);
}


Object* DescriptorArray::GetCallbacksObject(int descriptor_number) {
  ASSERT(GetType(descriptor_number) == CALLBACKS);
  return GetValue(descriptor_number);
}


AccessorDescriptor* DescriptorArray::GetCallbacks(int descriptor_number) {
  ASSERT(GetType(descriptor_number) == CALLBACKS);
  Foreign* p = Foreign::cast(GetCallbacksObject(descriptor_number));
  return reinterpret_cast<AccessorDescriptor*>(p->foreign_address());
}


void DescriptorArray::Get(int descriptor_number, Descriptor* desc) {
  desc->Init(handle(GetKey(descriptor_number), GetIsolate()),
             handle(GetValue(descriptor_number), GetIsolate()),
             GetDetails(descriptor_number));
}


void DescriptorArray::Set(int descriptor_number,
                          Descriptor* desc,
                          const WhitenessWitness&) {
  // Range check.
  ASSERT(descriptor_number < number_of_descriptors());

  NoIncrementalWriteBarrierSet(this,
                               ToKeyIndex(descriptor_number),
                               *desc->GetKey());
  NoIncrementalWriteBarrierSet(this,
                               ToValueIndex(descriptor_number),
                               *desc->GetValue());
  NoIncrementalWriteBarrierSet(this,
                               ToDetailsIndex(descriptor_number),
                               desc->GetDetails().AsSmi());
}


void DescriptorArray::Set(int descriptor_number, Descriptor* desc) {
  // Range check.
  ASSERT(descriptor_number < number_of_descriptors());

  set(ToKeyIndex(descriptor_number), *desc->GetKey());
  set(ToValueIndex(descriptor_number), *desc->GetValue());
  set(ToDetailsIndex(descriptor_number), desc->GetDetails().AsSmi());
}


void DescriptorArray::Append(Descriptor* desc,
                             const WhitenessWitness& witness) {
  DisallowHeapAllocation no_gc;
  int descriptor_number = number_of_descriptors();
  SetNumberOfDescriptors(descriptor_number + 1);
  Set(descriptor_number, desc, witness);

  uint32_t hash = desc->GetKey()->Hash();

  int insertion;

  for (insertion = descriptor_number; insertion > 0; --insertion) {
    Name* key = GetSortedKey(insertion - 1);
    if (key->Hash() <= hash) break;
    SetSortedKey(insertion, GetSortedKeyIndex(insertion - 1));
  }

  SetSortedKey(insertion, descriptor_number);
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


DescriptorArray::WhitenessWitness::WhitenessWitness(DescriptorArray* array)
    : marking_(array->GetHeap()->incremental_marking()) {
  marking_->EnterNoMarkingScope();
  ASSERT(!marking_->IsMarking() ||
         Marking::Color(array) == Marking::WHITE_OBJECT);
}


DescriptorArray::WhitenessWitness::~WhitenessWitness() {
  marking_->LeaveNoMarkingScope();
}


template<typename Derived, typename Shape, typename Key>
int HashTable<Derived, Shape, Key>::ComputeCapacity(int at_least_space_for) {
  const int kMinCapacity = 32;
  int capacity = RoundUpToPowerOf2(at_least_space_for * 2);
  if (capacity < kMinCapacity) {
    capacity = kMinCapacity;  // Guarantee min capacity.
  }
  return capacity;
}


template<typename Derived, typename Shape, typename Key>
int HashTable<Derived, Shape, Key>::FindEntry(Key key) {
  return FindEntry(GetIsolate(), key);
}


// Find entry for key otherwise return kNotFound.
template<typename Derived, typename Shape, typename Key>
int HashTable<Derived, Shape, Key>::FindEntry(Isolate* isolate, Key key) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(HashTable::Hash(key), capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  while (true) {
    Object* element = KeyAt(entry);
    // Empty entry. Uses raw unchecked accessors because it is called by the
    // string table during bootstrapping.
    if (element == isolate->heap()->raw_unchecked_undefined_value()) break;
    if (element != isolate->heap()->raw_unchecked_the_hole_value() &&
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
CAST_ACCESSOR(FixedTypedArrayBase)
CAST_ACCESSOR(ConstantPoolArray)
CAST_ACCESSOR(DescriptorArray)
CAST_ACCESSOR(DeoptimizationInputData)
CAST_ACCESSOR(DeoptimizationOutputData)
CAST_ACCESSOR(DependentCode)
CAST_ACCESSOR(StringTable)
CAST_ACCESSOR(JSFunctionResultCache)
CAST_ACCESSOR(NormalizedMapCache)
CAST_ACCESSOR(ScopeInfo)
CAST_ACCESSOR(CompilationCacheTable)
CAST_ACCESSOR(CodeCacheHashTable)
CAST_ACCESSOR(PolymorphicCodeCacheHashTable)
CAST_ACCESSOR(MapCache)
CAST_ACCESSOR(String)
CAST_ACCESSOR(SeqString)
CAST_ACCESSOR(SeqOneByteString)
CAST_ACCESSOR(SeqTwoByteString)
CAST_ACCESSOR(SlicedString)
CAST_ACCESSOR(ConsString)
CAST_ACCESSOR(ExternalString)
CAST_ACCESSOR(ExternalAsciiString)
CAST_ACCESSOR(ExternalTwoByteString)
CAST_ACCESSOR(Symbol)
CAST_ACCESSOR(Name)
CAST_ACCESSOR(JSReceiver)
CAST_ACCESSOR(JSObject)
CAST_ACCESSOR(Smi)
CAST_ACCESSOR(HeapObject)
CAST_ACCESSOR(HeapNumber)
CAST_ACCESSOR(Oddball)
CAST_ACCESSOR(Cell)
CAST_ACCESSOR(PropertyCell)
CAST_ACCESSOR(SharedFunctionInfo)
CAST_ACCESSOR(Map)
CAST_ACCESSOR(JSFunction)
CAST_ACCESSOR(GlobalObject)
CAST_ACCESSOR(JSGlobalProxy)
CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSBuiltinsObject)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(JSArray)
CAST_ACCESSOR(JSArrayBuffer)
CAST_ACCESSOR(JSArrayBufferView)
CAST_ACCESSOR(JSTypedArray)
CAST_ACCESSOR(JSDataView)
CAST_ACCESSOR(JSRegExp)
CAST_ACCESSOR(JSProxy)
CAST_ACCESSOR(JSFunctionProxy)
CAST_ACCESSOR(JSSet)
CAST_ACCESSOR(JSMap)
CAST_ACCESSOR(JSSetIterator)
CAST_ACCESSOR(JSMapIterator)
CAST_ACCESSOR(JSWeakMap)
CAST_ACCESSOR(JSWeakSet)
CAST_ACCESSOR(Foreign)
CAST_ACCESSOR(ByteArray)
CAST_ACCESSOR(FreeSpace)
CAST_ACCESSOR(ExternalArray)
CAST_ACCESSOR(ExternalInt8Array)
CAST_ACCESSOR(ExternalUint8Array)
CAST_ACCESSOR(ExternalInt16Array)
CAST_ACCESSOR(ExternalUint16Array)
CAST_ACCESSOR(ExternalInt32Array)
CAST_ACCESSOR(ExternalUint32Array)
CAST_ACCESSOR(ExternalFloat32Array)
CAST_ACCESSOR(ExternalFloat64Array)
CAST_ACCESSOR(ExternalUint8ClampedArray)
CAST_ACCESSOR(Struct)
CAST_ACCESSOR(AccessorInfo)

template <class Traits>
FixedTypedArray<Traits>* FixedTypedArray<Traits>::cast(Object* object) {
  SLOW_ASSERT(object->IsHeapObject() &&
      HeapObject::cast(object)->map()->instance_type() ==
          Traits::kInstanceType);
  return reinterpret_cast<FixedTypedArray<Traits>*>(object);
}


#define MAKE_STRUCT_CAST(NAME, Name, name) CAST_ACCESSOR(Name)
  STRUCT_LIST(MAKE_STRUCT_CAST)
#undef MAKE_STRUCT_CAST


template <typename Derived, typename Shape, typename Key>
HashTable<Derived, Shape, Key>*
HashTable<Derived, Shape, Key>::cast(Object* obj) {
  ASSERT(obj->IsHashTable());
  return reinterpret_cast<HashTable*>(obj);
}


SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)

SMI_ACCESSORS(FreeSpace, size, kSizeOffset)
NOBARRIER_SMI_ACCESSORS(FreeSpace, size, kSizeOffset)

SMI_ACCESSORS(String, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(String, length, kLengthOffset)


uint32_t Name::hash_field() {
  return READ_UINT32_FIELD(this, kHashFieldOffset);
}


void Name::set_hash_field(uint32_t value) {
  WRITE_UINT32_FIELD(this, kHashFieldOffset, value);
#if V8_HOST_ARCH_64_BIT
  WRITE_UINT32_FIELD(this, kHashFieldOffset + kIntSize, 0);
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
ACCESSORS(Symbol, flags, Smi, kFlagsOffset)
BOOL_ACCESSORS(Symbol, flags, is_private, kPrivateBit)


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
  if (!string->IsConsString()) return string;
  Handle<ConsString> cons = Handle<ConsString>::cast(string);
  if (cons->IsFlat()) return handle(cons->first());
  return SlowFlatten(cons, pretenure);
}


uint16_t String::Get(int index) {
  ASSERT(index >= 0 && index < length());
  switch (StringShape(this).full_representation_tag()) {
    case kSeqStringTag | kOneByteStringTag:
      return SeqOneByteString::cast(this)->SeqOneByteStringGet(index);
    case kSeqStringTag | kTwoByteStringTag:
      return SeqTwoByteString::cast(this)->SeqTwoByteStringGet(index);
    case kConsStringTag | kOneByteStringTag:
    case kConsStringTag | kTwoByteStringTag:
      return ConsString::cast(this)->ConsStringGet(index);
    case kExternalStringTag | kOneByteStringTag:
      return ExternalAsciiString::cast(this)->ExternalAsciiStringGet(index);
    case kExternalStringTag | kTwoByteStringTag:
      return ExternalTwoByteString::cast(this)->ExternalTwoByteStringGet(index);
    case kSlicedStringTag | kOneByteStringTag:
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
  ASSERT(this->IsFlat());
  ASSERT(StringShape(this).IsIndirect());
  STATIC_ASSERT(ConsString::kFirstOffset == SlicedString::kParentOffset);
  const int kUnderlyingOffset = SlicedString::kParentOffset;
  return String::cast(READ_FIELD(this, kUnderlyingOffset));
}


template<class Visitor>
ConsString* String::VisitFlat(Visitor* visitor,
                              String* string,
                              const int offset) {
  int slice_offset = offset;
  const int length = string->length();
  ASSERT(offset <= length);
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
            ExternalAsciiString::cast(string)->GetChars() + slice_offset,
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

      default:
        UNREACHABLE();
        return NULL;
    }
  }
}


uint16_t SeqOneByteString::SeqOneByteStringGet(int index) {
  ASSERT(index >= 0 && index < length());
  return READ_BYTE_FIELD(this, kHeaderSize + index * kCharSize);
}


void SeqOneByteString::SeqOneByteStringSet(int index, uint16_t value) {
  ASSERT(index >= 0 && index < length() && value <= kMaxOneByteCharCode);
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


int SeqOneByteString::SeqOneByteStringSize(InstanceType instance_type) {
  return SizeFor(length());
}


String* SlicedString::parent() {
  return String::cast(READ_FIELD(this, kParentOffset));
}


void SlicedString::set_parent(String* parent, WriteBarrierMode mode) {
  ASSERT(parent->IsSeqString() || parent->IsExternalString());
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


bool ExternalString::is_short() {
  InstanceType type = map()->instance_type();
  return (type & kShortExternalStringMask) == kShortExternalStringTag;
}


const ExternalAsciiString::Resource* ExternalAsciiString::resource() {
  return *reinterpret_cast<Resource**>(FIELD_ADDR(this, kResourceOffset));
}


void ExternalAsciiString::update_data_cache() {
  if (is_short()) return;
  const char** data_field =
      reinterpret_cast<const char**>(FIELD_ADDR(this, kResourceDataOffset));
  *data_field = resource()->data();
}


void ExternalAsciiString::set_resource(
    const ExternalAsciiString::Resource* resource) {
  ASSERT(IsAligned(reinterpret_cast<intptr_t>(resource), kPointerSize));
  *reinterpret_cast<const Resource**>(
      FIELD_ADDR(this, kResourceOffset)) = resource;
  if (resource != NULL) update_data_cache();
}


const uint8_t* ExternalAsciiString::GetChars() {
  return reinterpret_cast<const uint8_t*>(resource()->data());
}


uint16_t ExternalAsciiString::ExternalAsciiStringGet(int index) {
  ASSERT(index >= 0 && index < length());
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
  ASSERT(index >= 0 && index < length());
  return GetChars()[index];
}


const uint16_t* ExternalTwoByteString::ExternalTwoByteStringGetData(
      unsigned start) {
  return GetChars() + start;
}


int ConsStringIteratorOp::OffsetForDepth(int depth) {
  return depth & kDepthMask;
}


void ConsStringIteratorOp::PushLeft(ConsString* string) {
  frames_[depth_++ & kDepthMask] = string;
}


void ConsStringIteratorOp::PushRight(ConsString* string) {
  // Inplace update.
  frames_[(depth_-1) & kDepthMask] = string;
}


void ConsStringIteratorOp::AdjustMaximumDepth() {
  if (depth_ > maximum_depth_) maximum_depth_ = depth_;
}


void ConsStringIteratorOp::Pop() {
  ASSERT(depth_ > 0);
  ASSERT(depth_ <= maximum_depth_);
  depth_--;
}


uint16_t StringCharacterStream::GetNext() {
  ASSERT(buffer8_ != NULL && end_ != NULL);
  // Advance cursor if needed.
  if (buffer8_ == end_) HasMore();
  ASSERT(buffer8_ < end_);
  return is_one_byte_ ? *buffer8_++ : *buffer16_++;
}


StringCharacterStream::StringCharacterStream(String* string,
                                             ConsStringIteratorOp* op,
                                             int offset)
  : is_one_byte_(false),
    op_(op) {
  Reset(string, offset);
}


void StringCharacterStream::Reset(String* string, int offset) {
  buffer8_ = NULL;
  end_ = NULL;
  ConsString* cons_string = String::VisitFlat(this, string, offset);
  op_->Reset(cons_string, offset);
  if (cons_string != NULL) {
    string = op_->Next(&offset);
    if (string != NULL) String::VisitFlat(this, string, offset);
  }
}


bool StringCharacterStream::HasMore() {
  if (buffer8_ != end_) return true;
  int offset;
  String* string = op_->Next(&offset);
  ASSERT_EQ(offset, 0);
  if (string == NULL) return false;
  String::VisitFlat(this, string);
  ASSERT(buffer8_ != end_);
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


void JSFunctionResultCache::MakeZeroSize() {
  set_finger_index(kEntriesIndex);
  set_size(kEntriesIndex);
}


void JSFunctionResultCache::Clear() {
  int cache_size = size();
  Object** entries_start = RawFieldOfElementAt(kEntriesIndex);
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


uint8_t* ExternalUint8ClampedArray::external_uint8_clamped_pointer() {
  return reinterpret_cast<uint8_t*>(external_pointer());
}


uint8_t ExternalUint8ClampedArray::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = external_uint8_clamped_pointer();
  return ptr[index];
}


Handle<Object> ExternalUint8ClampedArray::get(
    Handle<ExternalUint8ClampedArray> array,
    int index) {
  return Handle<Smi>(Smi::FromInt(array->get_scalar(index)),
                     array->GetIsolate());
}


void ExternalUint8ClampedArray::set(int index, uint8_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = external_uint8_clamped_pointer();
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


int8_t ExternalInt8Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  int8_t* ptr = static_cast<int8_t*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalInt8Array::get(Handle<ExternalInt8Array> array,
                                      int index) {
  return Handle<Smi>(Smi::FromInt(array->get_scalar(index)),
                     array->GetIsolate());
}


void ExternalInt8Array::set(int index, int8_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  int8_t* ptr = static_cast<int8_t*>(external_pointer());
  ptr[index] = value;
}


uint8_t ExternalUint8Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = static_cast<uint8_t*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalUint8Array::get(Handle<ExternalUint8Array> array,
                                       int index) {
  return Handle<Smi>(Smi::FromInt(array->get_scalar(index)),
                     array->GetIsolate());
}


void ExternalUint8Array::set(int index, uint8_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint8_t* ptr = static_cast<uint8_t*>(external_pointer());
  ptr[index] = value;
}


int16_t ExternalInt16Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  int16_t* ptr = static_cast<int16_t*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalInt16Array::get(Handle<ExternalInt16Array> array,
                                       int index) {
  return Handle<Smi>(Smi::FromInt(array->get_scalar(index)),
                     array->GetIsolate());
}


void ExternalInt16Array::set(int index, int16_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  int16_t* ptr = static_cast<int16_t*>(external_pointer());
  ptr[index] = value;
}


uint16_t ExternalUint16Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint16_t* ptr = static_cast<uint16_t*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalUint16Array::get(Handle<ExternalUint16Array> array,
                                        int index) {
  return Handle<Smi>(Smi::FromInt(array->get_scalar(index)),
                     array->GetIsolate());
}


void ExternalUint16Array::set(int index, uint16_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint16_t* ptr = static_cast<uint16_t*>(external_pointer());
  ptr[index] = value;
}


int32_t ExternalInt32Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  int32_t* ptr = static_cast<int32_t*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalInt32Array::get(Handle<ExternalInt32Array> array,
                                       int index) {
  return array->GetIsolate()->factory()->
      NewNumberFromInt(array->get_scalar(index));
}


void ExternalInt32Array::set(int index, int32_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  int32_t* ptr = static_cast<int32_t*>(external_pointer());
  ptr[index] = value;
}


uint32_t ExternalUint32Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  uint32_t* ptr = static_cast<uint32_t*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalUint32Array::get(Handle<ExternalUint32Array> array,
                                        int index) {
  return array->GetIsolate()->factory()->
      NewNumberFromUint(array->get_scalar(index));
}


void ExternalUint32Array::set(int index, uint32_t value) {
  ASSERT((index >= 0) && (index < this->length()));
  uint32_t* ptr = static_cast<uint32_t*>(external_pointer());
  ptr[index] = value;
}


float ExternalFloat32Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  float* ptr = static_cast<float*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalFloat32Array::get(Handle<ExternalFloat32Array> array,
                                         int index) {
  return array->GetIsolate()->factory()->NewNumber(array->get_scalar(index));
}


void ExternalFloat32Array::set(int index, float value) {
  ASSERT((index >= 0) && (index < this->length()));
  float* ptr = static_cast<float*>(external_pointer());
  ptr[index] = value;
}


double ExternalFloat64Array::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  double* ptr = static_cast<double*>(external_pointer());
  return ptr[index];
}


Handle<Object> ExternalFloat64Array::get(Handle<ExternalFloat64Array> array,
                                         int index) {
  return array->GetIsolate()->factory()->NewNumber(array->get_scalar(index));
}


void ExternalFloat64Array::set(int index, double value) {
  ASSERT((index >= 0) && (index < this->length()));
  double* ptr = static_cast<double*>(external_pointer());
  ptr[index] = value;
}


void* FixedTypedArrayBase::DataPtr() {
  return FIELD_ADDR(this, kDataOffset);
}


int FixedTypedArrayBase::DataSize() {
  InstanceType instance_type = map()->instance_type();
  int element_size;
  switch (instance_type) {
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
  return length() * element_size;
}


int FixedTypedArrayBase::size() {
  return OBJECT_POINTER_ALIGN(kDataOffset + DataSize());
}


uint8_t Uint8ArrayTraits::defaultValue() { return 0; }


uint8_t Uint8ClampedArrayTraits::defaultValue() { return 0; }


int8_t Int8ArrayTraits::defaultValue() { return 0; }


uint16_t Uint16ArrayTraits::defaultValue() { return 0; }


int16_t Int16ArrayTraits::defaultValue() { return 0; }


uint32_t Uint32ArrayTraits::defaultValue() { return 0; }


int32_t Int32ArrayTraits::defaultValue() { return 0; }


float Float32ArrayTraits::defaultValue() {
  return static_cast<float>(OS::nan_value());
}


double Float64ArrayTraits::defaultValue() { return OS::nan_value(); }


template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  ElementType* ptr = reinterpret_cast<ElementType*>(
      FIELD_ADDR(this, kDataOffset));
  return ptr[index];
}


template<> inline
FixedTypedArray<Float64ArrayTraits>::ElementType
    FixedTypedArray<Float64ArrayTraits>::get_scalar(int index) {
  ASSERT((index >= 0) && (index < this->length()));
  return READ_DOUBLE_FIELD(this, ElementOffset(index));
}


template <class Traits>
void FixedTypedArray<Traits>::set(int index, ElementType value) {
  ASSERT((index >= 0) && (index < this->length()));
  ElementType* ptr = reinterpret_cast<ElementType*>(
      FIELD_ADDR(this, kDataOffset));
  ptr[index] = value;
}


template<> inline
void FixedTypedArray<Float64ArrayTraits>::set(
    int index, Float64ArrayTraits::ElementType value) {
  ASSERT((index >= 0) && (index < this->length()));
  WRITE_DOUBLE_FIELD(this, ElementOffset(index), value);
}


template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::from_int(int value) {
  return static_cast<ElementType>(value);
}


template <> inline
uint8_t FixedTypedArray<Uint8ClampedArrayTraits>::from_int(int value) {
  if (value < 0) return 0;
  if (value > 0xFF) return 0xFF;
  return static_cast<uint8_t>(value);
}


template <class Traits>
typename Traits::ElementType FixedTypedArray<Traits>::from_double(
    double value) {
  return static_cast<ElementType>(DoubleToInt32(value));
}


template<> inline
uint8_t FixedTypedArray<Uint8ClampedArrayTraits>::from_double(double value) {
  if (value < 0) return 0;
  if (value > 0xFF) return 0xFF;
  return static_cast<uint8_t>(lrint(value));
}


template<> inline
float FixedTypedArray<Float32ArrayTraits>::from_double(double value) {
  return static_cast<float>(value);
}


template<> inline
double FixedTypedArray<Float64ArrayTraits>::from_double(double value) {
  return value;
}


template <class Traits>
Handle<Object> FixedTypedArray<Traits>::get(
    Handle<FixedTypedArray<Traits> > array,
    int index) {
  return Traits::ToHandle(array->GetIsolate(), array->get_scalar(index));
}


template <class Traits>
Handle<Object> FixedTypedArray<Traits>::SetValue(
    Handle<FixedTypedArray<Traits> > array,
    uint32_t index,
    Handle<Object> value) {
  ElementType cast_value = Traits::defaultValue();
  if (index < static_cast<uint32_t>(array->length())) {
    if (value->IsSmi()) {
      int int_value = Handle<Smi>::cast(value)->value();
      cast_value = from_int(int_value);
    } else if (value->IsHeapNumber()) {
      double double_value = Handle<HeapNumber>::cast(value)->value();
      cast_value = from_double(double_value);
    } else {
      // Clamp undefined to the default value. All other types have been
      // converted to a number type further up in the call chain.
      ASSERT(value->IsUndefined());
    }
    array->set(index, cast_value);
  }
  return Traits::ToHandle(array->GetIsolate(), cast_value);
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
  ASSERT(0 <= id && id < 256);
  WRITE_BYTE_FIELD(this, kVisitorIdOffset, static_cast<byte>(id));
}


int Map::instance_size() {
  return NOBARRIER_READ_BYTE_FIELD(
      this, kInstanceSizeOffset) << kPointerSizeLog2;
}


int Map::inobject_properties() {
  return READ_BYTE_FIELD(this, kInObjectPropertiesOffset);
}


int Map::pre_allocated_property_fields() {
  return READ_BYTE_FIELD(this, kPreAllocatedPropertyFieldsOffset);
}


int Map::GetInObjectPropertyOffset(int index) {
  // Adjust for the number of properties stored in the object.
  index -= inobject_properties();
  ASSERT(index < 0);
  return instance_size() + (index * kPointerSize);
}


int HeapObject::SizeFromMap(Map* map) {
  int instance_size = map->instance_size();
  if (instance_size != kVariableSizeSentinel) return instance_size;
  // Only inline the most frequent cases.
  int instance_type = static_cast<int>(map->instance_type());
  if (instance_type == FIXED_ARRAY_TYPE) {
    return FixedArray::BodyDescriptor::SizeOf(map, this);
  }
  if (instance_type == ASCII_STRING_TYPE ||
      instance_type == ASCII_INTERNALIZED_STRING_TYPE) {
    return SeqOneByteString::SizeFor(
        reinterpret_cast<SeqOneByteString*>(this)->length());
  }
  if (instance_type == BYTE_ARRAY_TYPE) {
    return reinterpret_cast<ByteArray*>(this)->ByteArraySize();
  }
  if (instance_type == FREE_SPACE_TYPE) {
    return reinterpret_cast<FreeSpace*>(this)->nobarrier_size();
  }
  if (instance_type == STRING_TYPE ||
      instance_type == INTERNALIZED_STRING_TYPE) {
    return SeqTwoByteString::SizeFor(
        reinterpret_cast<SeqTwoByteString*>(this)->length());
  }
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
    return FixedDoubleArray::SizeFor(
        reinterpret_cast<FixedDoubleArray*>(this)->length());
  }
  if (instance_type == CONSTANT_POOL_ARRAY_TYPE) {
    return ConstantPoolArray::SizeFor(
        reinterpret_cast<ConstantPoolArray*>(this)->count_of_int64_entries(),
        reinterpret_cast<ConstantPoolArray*>(this)->count_of_code_ptr_entries(),
        reinterpret_cast<ConstantPoolArray*>(this)->count_of_heap_ptr_entries(),
        reinterpret_cast<ConstantPoolArray*>(this)->count_of_int32_entries());
  }
  if (instance_type >= FIRST_FIXED_TYPED_ARRAY_TYPE &&
      instance_type <= LAST_FIXED_TYPED_ARRAY_TYPE) {
    return reinterpret_cast<FixedTypedArrayBase*>(this)->size();
  }
  ASSERT(instance_type == CODE_TYPE);
  return reinterpret_cast<Code*>(this)->CodeSize();
}


void Map::set_instance_size(int value) {
  ASSERT_EQ(0, value & (kPointerSize - 1));
  value >>= kPointerSizeLog2;
  ASSERT(0 <= value && value < 256);
  NOBARRIER_WRITE_BYTE_FIELD(
      this, kInstanceSizeOffset, static_cast<byte>(value));
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
  set_bit_field3(FunctionWithPrototype::update(bit_field3(), value));
}


bool Map::function_with_prototype() {
  return FunctionWithPrototype::decode(bit_field3());
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
  set_bit_field3(IsShared::update(bit_field3(), value));
}


bool Map::is_shared() {
  return IsShared::decode(bit_field3()); }


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


void Map::set_owns_descriptors(bool is_shared) {
  set_bit_field3(OwnsDescriptors::update(bit_field3(), is_shared));
}


bool Map::owns_descriptors() {
  return OwnsDescriptors::decode(bit_field3());
}


void Map::set_has_instance_call_handler() {
  set_bit_field3(HasInstanceCallHandler::update(bit_field3(), true));
}


bool Map::has_instance_call_handler() {
  return HasInstanceCallHandler::decode(bit_field3());
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


void Map::freeze() {
  set_bit_field3(IsFrozen::update(bit_field3(), true));
}


bool Map::is_frozen() {
  return IsFrozen::decode(bit_field3());
}


void Map::mark_unstable() {
  set_bit_field3(IsUnstable::update(bit_field3(), true));
}


bool Map::is_stable() {
  return !IsUnstable::decode(bit_field3());
}


bool Map::has_code_cache() {
  return code_cache() != GetIsolate()->heap()->empty_fixed_array();
}


bool Map::CanBeDeprecated() {
  int descriptor = LastAdded();
  for (int i = 0; i <= descriptor; i++) {
    PropertyDetails details = instance_descriptors()->GetDetails(i);
    if (details.representation().IsNone()) return true;
    if (details.representation().IsSmi()) return true;
    if (details.representation().IsDouble()) return true;
    if (details.representation().IsHeapObject()) return true;
    if (details.type() == CONSTANT) return true;
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


bool Map::CanOmitMapChecks() {
  return is_stable() && FLAG_omit_map_checks_for_leaf_maps;
}


int DependentCode::number_of_entries(DependencyGroup group) {
  if (length() == 0) return 0;
  return Smi::cast(get(group))->value();
}


void DependentCode::set_number_of_entries(DependencyGroup group, int value) {
  set(group, Smi::FromInt(value));
}


bool DependentCode::is_code_at(int i) {
  return get(kCodesStartIndex + i)->IsCode();
}

Code* DependentCode::code_at(int i) {
  return Code::cast(get(kCodesStartIndex + i));
}


CompilationInfo* DependentCode::compilation_info_at(int i) {
  return reinterpret_cast<CompilationInfo*>(
      Foreign::cast(get(kCodesStartIndex + i))->foreign_address());
}


void DependentCode::set_object_at(int i, Object* object) {
  set(kCodesStartIndex + i, object);
}


Object* DependentCode::object_at(int i) {
  return get(kCodesStartIndex + i);
}


Object** DependentCode::slot_at(int i) {
  return RawFieldOfElementAt(kCodesStartIndex + i);
}


void DependentCode::clear_at(int i) {
  set_undefined(kCodesStartIndex + i);
}


void DependentCode::copy(int from, int to) {
  set(kCodesStartIndex + to, get(kCodesStartIndex + from));
}


void DependentCode::ExtendGroup(DependencyGroup group) {
  GroupStartIndexes starts(this);
  for (int g = kGroupCount - 1; g > group; g--) {
    if (starts.at(g) < starts.at(g + 1)) {
      copy(starts.at(g), starts.at(g + 1));
    }
  }
}


void Code::set_flags(Code::Flags flags) {
  STATIC_ASSERT(Code::NUMBER_OF_KINDS <= KindField::kMax + 1);
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
         result == DEBUG_STUB);
  return result;
}


ExtraICState Code::extra_ic_state() {
  ASSERT(is_inline_cache_stub() || ic_state() == DEBUG_STUB);
  return ExtractExtraICStateFromFlags(flags());
}


Code::StubType Code::type() {
  return ExtractTypeFromFlags(flags());
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


inline void Code::set_is_crankshafted(bool value) {
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = IsCrankshaftedField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


int Code::major_key() {
  ASSERT(has_major_key());
  return StubMajorKeyField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset));
}


void Code::set_major_key(int major) {
  ASSERT(has_major_key());
  ASSERT(0 <= major && major < 256);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = StubMajorKeyField::update(previous, major);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


bool Code::has_major_key() {
  return kind() == STUB ||
      kind() == HANDLER ||
      kind() == BINARY_OP_IC ||
      kind() == COMPARE_IC ||
      kind() == COMPARE_NIL_IC ||
      kind() == LOAD_IC ||
      kind() == KEYED_LOAD_IC ||
      kind() == STORE_IC ||
      kind() == CALL_IC ||
      kind() == KEYED_STORE_IC ||
      kind() == TO_BOOLEAN_IC;
}


bool Code::optimizable() {
  ASSERT_EQ(FUNCTION, kind());
  return READ_BYTE_FIELD(this, kOptimizableOffset) == 1;
}


void Code::set_optimizable(bool value) {
  ASSERT_EQ(FUNCTION, kind());
  WRITE_BYTE_FIELD(this, kOptimizableOffset, value ? 1 : 0);
}


bool Code::has_deoptimization_support() {
  ASSERT_EQ(FUNCTION, kind());
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasDeoptimizationSupportField::decode(flags);
}


void Code::set_has_deoptimization_support(bool value) {
  ASSERT_EQ(FUNCTION, kind());
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasDeoptimizationSupportField::update(flags, value);
  WRITE_BYTE_FIELD(this, kFullCodeFlags, flags);
}


bool Code::has_debug_break_slots() {
  ASSERT_EQ(FUNCTION, kind());
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsHasDebugBreakSlotsField::decode(flags);
}


void Code::set_has_debug_break_slots(bool value) {
  ASSERT_EQ(FUNCTION, kind());
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsHasDebugBreakSlotsField::update(flags, value);
  WRITE_BYTE_FIELD(this, kFullCodeFlags, flags);
}


bool Code::is_compiled_optimizable() {
  ASSERT_EQ(FUNCTION, kind());
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  return FullCodeFlagsIsCompiledOptimizable::decode(flags);
}


void Code::set_compiled_optimizable(bool value) {
  ASSERT_EQ(FUNCTION, kind());
  byte flags = READ_BYTE_FIELD(this, kFullCodeFlags);
  flags = FullCodeFlagsIsCompiledOptimizable::update(flags, value);
  WRITE_BYTE_FIELD(this, kFullCodeFlags, flags);
}


int Code::allow_osr_at_loop_nesting_level() {
  ASSERT_EQ(FUNCTION, kind());
  return READ_BYTE_FIELD(this, kAllowOSRAtLoopNestingLevelOffset);
}


void Code::set_allow_osr_at_loop_nesting_level(int level) {
  ASSERT_EQ(FUNCTION, kind());
  ASSERT(level >= 0 && level <= kMaxLoopNestingMarker);
  WRITE_BYTE_FIELD(this, kAllowOSRAtLoopNestingLevelOffset, level);
}


int Code::profiler_ticks() {
  ASSERT_EQ(FUNCTION, kind());
  return READ_BYTE_FIELD(this, kProfilerTicksOffset);
}


void Code::set_profiler_ticks(int ticks) {
  ASSERT_EQ(FUNCTION, kind());
  ASSERT(ticks < 256);
  WRITE_BYTE_FIELD(this, kProfilerTicksOffset, ticks);
}


unsigned Code::stack_slots() {
  ASSERT(is_crankshafted());
  return StackSlotsField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::set_stack_slots(unsigned slots) {
  CHECK(slots <= (1 << kStackSlotsBitCount));
  ASSERT(is_crankshafted());
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = StackSlotsField::update(previous, slots);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}


unsigned Code::safepoint_table_offset() {
  ASSERT(is_crankshafted());
  return SafepointTableOffsetField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset));
}


void Code::set_safepoint_table_offset(unsigned offset) {
  CHECK(offset <= (1 << kSafepointTableOffsetBitCount));
  ASSERT(is_crankshafted());
  ASSERT(IsAligned(offset, static_cast<unsigned>(kIntSize)));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = SafepointTableOffsetField::update(previous, offset);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


unsigned Code::back_edge_table_offset() {
  ASSERT_EQ(FUNCTION, kind());
  return BackEdgeTableOffsetField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset));
}


void Code::set_back_edge_table_offset(unsigned offset) {
  ASSERT_EQ(FUNCTION, kind());
  ASSERT(IsAligned(offset, static_cast<unsigned>(kIntSize)));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = BackEdgeTableOffsetField::update(previous, offset);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}


bool Code::back_edges_patched_for_osr() {
  ASSERT_EQ(FUNCTION, kind());
  return BackEdgesPatchedForOSRField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags2Offset));
}


void Code::set_back_edges_patched_for_osr(bool value) {
  ASSERT_EQ(FUNCTION, kind());
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = BackEdgesPatchedForOSRField::update(previous, value);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags2Offset, updated);
}



byte Code::to_boolean_state() {
  return extra_ic_state();
}


bool Code::has_function_cache() {
  ASSERT(kind() == STUB);
  return HasFunctionCacheField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::set_has_function_cache(bool flag) {
  ASSERT(kind() == STUB);
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = HasFunctionCacheField::update(previous, flag);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}


bool Code::marked_for_deoptimization() {
  ASSERT(kind() == OPTIMIZED_FUNCTION);
  return MarkedForDeoptimizationField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::set_marked_for_deoptimization(bool flag) {
  ASSERT(kind() == OPTIMIZED_FUNCTION);
  ASSERT(!flag || AllowDeoptimization::IsAllowed(GetIsolate()));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = MarkedForDeoptimizationField::update(previous, flag);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}


bool Code::is_weak_stub() {
  return CanBeWeakStub() && WeakStubField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::mark_as_weak_stub() {
  ASSERT(CanBeWeakStub());
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = WeakStubField::update(previous, true);
  WRITE_UINT32_FIELD(this, kKindSpecificFlags1Offset, updated);
}


bool Code::is_invalidated_weak_stub() {
  return is_weak_stub() && InvalidatedWeakStubField::decode(
      READ_UINT32_FIELD(this, kKindSpecificFlags1Offset));
}


void Code::mark_as_invalidated_weak_stub() {
  ASSERT(is_inline_cache_stub());
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = InvalidatedWeakStubField::update(previous, true);
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


bool Code::is_keyed_stub() {
  return is_keyed_load_stub() || is_keyed_store_stub();
}


bool Code::is_debug_stub() {
  return ic_state() == DEBUG_STUB;
}


ConstantPoolArray* Code::constant_pool() {
  return ConstantPoolArray::cast(READ_FIELD(this, kConstantPoolOffset));
}


void Code::set_constant_pool(Object* value) {
  ASSERT(value->IsConstantPoolArray());
  WRITE_FIELD(this, kConstantPoolOffset, value);
  WRITE_BARRIER(GetHeap(), this, kConstantPoolOffset, value);
}


Code::Flags Code::ComputeFlags(Kind kind,
                               InlineCacheState ic_state,
                               ExtraICState extra_ic_state,
                               StubType type,
                               InlineCacheHolderFlag holder) {
  // Compute the bit mask.
  unsigned int bits = KindField::encode(kind)
      | ICStateField::encode(ic_state)
      | TypeField::encode(type)
      | ExtraICStateField::encode(extra_ic_state)
      | CacheHolderField::encode(holder);
  return static_cast<Flags>(bits);
}


Code::Flags Code::ComputeMonomorphicFlags(Kind kind,
                                          ExtraICState extra_ic_state,
                                          InlineCacheHolderFlag holder,
                                          StubType type) {
  return ComputeFlags(kind, MONOMORPHIC, extra_ic_state, type, holder);
}


Code::Flags Code::ComputeHandlerFlags(Kind handler_kind,
                                      StubType type,
                                      InlineCacheHolderFlag holder) {
  return ComputeFlags(Code::HANDLER, MONOMORPHIC, handler_kind, type, holder);
}


Code::Kind Code::ExtractKindFromFlags(Flags flags) {
  return KindField::decode(flags);
}


InlineCacheState Code::ExtractICStateFromFlags(Flags flags) {
  return ICStateField::decode(flags);
}


ExtraICState Code::ExtractExtraICStateFromFlags(Flags flags) {
  return ExtraICStateField::decode(flags);
}


Code::StubType Code::ExtractTypeFromFlags(Flags flags) {
  return TypeField::decode(flags);
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


Object* Code::GetObjectFromEntryAddress(Address location_of_address) {
  return HeapObject::
      FromAddress(Memory::Address_at(location_of_address) - Code::kHeaderSize);
}


bool Code::IsWeakObjectInOptimizedCode(Object* object) {
  if (!FLAG_collect_maps) return false;
  if (object->IsMap()) {
    return Map::cast(object)->CanTransition() &&
           FLAG_weak_embedded_maps_in_optimized_code;
  }
  if (object->IsJSObject() ||
      (object->IsCell() && Cell::cast(object)->value()->IsJSObject())) {
    return FLAG_weak_embedded_objects_in_optimized_code;
  }
  return false;
}


class Code::FindAndReplacePattern {
 public:
  FindAndReplacePattern() : count_(0) { }
  void Add(Handle<Map> map_to_find, Handle<Object> obj_to_replace) {
    ASSERT(count_ < kMaxCount);
    find_[count_] = map_to_find;
    replace_[count_] = obj_to_replace;
    ++count_;
  }
 private:
  static const int kMaxCount = 4;
  int count_;
  Handle<Map> find_[kMaxCount];
  Handle<Object> replace_[kMaxCount];
  friend class Code;
};


bool Code::IsWeakObjectInIC(Object* object) {
  return object->IsMap() && Map::cast(object)->CanTransition() &&
         FLAG_collect_maps &&
         FLAG_weak_embedded_maps_in_ic;
}


Object* Map::prototype() {
  return READ_FIELD(this, kPrototypeOffset);
}


void Map::set_prototype(Object* value, WriteBarrierMode mode) {
  ASSERT(value->IsNull() || value->IsJSReceiver());
  WRITE_FIELD(this, kPrototypeOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kPrototypeOffset, value, mode);
}


// If the descriptor is using the empty transition array, install a new empty
// transition array that will have place for an element transition.
static void EnsureHasTransitionArray(Handle<Map> map) {
  Handle<TransitionArray> transitions;
  if (!map->HasTransitionArray()) {
    transitions = TransitionArray::Allocate(map->GetIsolate(), 0);
    transitions->set_back_pointer_storage(map->GetBackPointer());
  } else if (!map->transitions()->IsFullTransitionArray()) {
    transitions = TransitionArray::ExtendToFullTransitionArray(map);
  } else {
    return;
  }
  map->set_transitions(*transitions);
}


void Map::InitializeDescriptors(DescriptorArray* descriptors) {
  int len = descriptors->number_of_descriptors();
  set_instance_descriptors(descriptors);
  SetNumberOfOwnDescriptors(len);
}


ACCESSORS(Map, instance_descriptors, DescriptorArray, kDescriptorsOffset)


void Map::set_bit_field3(uint32_t bits) {
  // Ensure the upper 2 bits have the same value by sign extending it. This is
  // necessary to be able to use the 31st bit.
  int value = bits << 1;
  WRITE_FIELD(this, kBitField3Offset, Smi::FromInt(value >> 1));
}


uint32_t Map::bit_field3() {
  Object* value = READ_FIELD(this, kBitField3Offset);
  return Smi::cast(value)->value();
}


void Map::AppendDescriptor(Descriptor* desc) {
  DescriptorArray* descriptors = instance_descriptors();
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  ASSERT(descriptors->number_of_descriptors() == number_of_own_descriptors);
  descriptors->Append(desc);
  SetNumberOfOwnDescriptors(number_of_own_descriptors + 1);
}


Object* Map::GetBackPointer() {
  Object* object = READ_FIELD(this, kTransitionsOrBackPointerOffset);
  if (object->IsDescriptorArray()) {
    return TransitionArray::cast(object)->back_pointer_storage();
  } else {
    ASSERT(object->IsMap() || object->IsUndefined());
    return object;
  }
}


bool Map::HasElementsTransition() {
  return HasTransitionArray() && transitions()->HasElementsTransition();
}


bool Map::HasTransitionArray() {
  Object* object = READ_FIELD(this, kTransitionsOrBackPointerOffset);
  return object->IsTransitionArray();
}


Map* Map::elements_transition_map() {
  int index = transitions()->Search(GetHeap()->elements_transition_symbol());
  return transitions()->GetTarget(index);
}


bool Map::CanHaveMoreTransitions() {
  if (!HasTransitionArray()) return true;
  return FixedArray::SizeFor(transitions()->length() +
                             TransitionArray::kTransitionSize)
      <= Page::kMaxRegularHeapObjectSize;
}


Map* Map::GetTransition(int transition_index) {
  return transitions()->GetTarget(transition_index);
}


int Map::SearchTransition(Name* name) {
  if (HasTransitionArray()) return transitions()->Search(name);
  return TransitionArray::kNotFound;
}


FixedArray* Map::GetPrototypeTransitions() {
  if (!HasTransitionArray()) return GetHeap()->empty_fixed_array();
  if (!transitions()->HasPrototypeTransitions()) {
    return GetHeap()->empty_fixed_array();
  }
  return transitions()->GetPrototypeTransitions();
}


void Map::SetPrototypeTransitions(
    Handle<Map> map, Handle<FixedArray> proto_transitions) {
  EnsureHasTransitionArray(map);
  int old_number_of_transitions = map->NumberOfProtoTransitions();
#ifdef DEBUG
  if (map->HasPrototypeTransitions()) {
    ASSERT(map->GetPrototypeTransitions() != *proto_transitions);
    map->ZapPrototypeTransitions();
  }
#endif
  map->transitions()->SetPrototypeTransitions(*proto_transitions);
  map->SetNumberOfProtoTransitions(old_number_of_transitions);
}


bool Map::HasPrototypeTransitions() {
  return HasTransitionArray() && transitions()->HasPrototypeTransitions();
}


TransitionArray* Map::transitions() {
  ASSERT(HasTransitionArray());
  Object* object = READ_FIELD(this, kTransitionsOrBackPointerOffset);
  return TransitionArray::cast(object);
}


void Map::set_transitions(TransitionArray* transition_array,
                          WriteBarrierMode mode) {
  // Transition arrays are not shared. When one is replaced, it should not
  // keep referenced objects alive, so we zap it.
  // When there is another reference to the array somewhere (e.g. a handle),
  // not zapping turns from a waste of memory into a source of crashes.
  if (HasTransitionArray()) {
#ifdef DEBUG
    for (int i = 0; i < transitions()->number_of_transitions(); i++) {
      Map* target = transitions()->GetTarget(i);
      if (target->instance_descriptors() == instance_descriptors()) {
        Name* key = transitions()->GetKey(i);
        int new_target_index = transition_array->Search(key);
        ASSERT(new_target_index != TransitionArray::kNotFound);
        ASSERT(transition_array->GetTarget(new_target_index) == target);
      }
    }
#endif
    ASSERT(transitions() != transition_array);
    ZapTransitions();
  }

  WRITE_FIELD(this, kTransitionsOrBackPointerOffset, transition_array);
  CONDITIONAL_WRITE_BARRIER(
      GetHeap(), this, kTransitionsOrBackPointerOffset, transition_array, mode);
}


void Map::init_back_pointer(Object* undefined) {
  ASSERT(undefined->IsUndefined());
  WRITE_FIELD(this, kTransitionsOrBackPointerOffset, undefined);
}


void Map::SetBackPointer(Object* value, WriteBarrierMode mode) {
  ASSERT(instance_type() >= FIRST_JS_RECEIVER_TYPE);
  ASSERT((value->IsUndefined() && GetBackPointer()->IsMap()) ||
         (value->IsMap() && GetBackPointer()->IsUndefined()));
  Object* object = READ_FIELD(this, kTransitionsOrBackPointerOffset);
  if (object->IsTransitionArray()) {
    TransitionArray::cast(object)->set_back_pointer_storage(value);
  } else {
    WRITE_FIELD(this, kTransitionsOrBackPointerOffset, value);
    CONDITIONAL_WRITE_BARRIER(
        GetHeap(), this, kTransitionsOrBackPointerOffset, value, mode);
  }
}


ACCESSORS(Map, code_cache, Object, kCodeCacheOffset)
ACCESSORS(Map, dependent_code, DependentCode, kDependentCodeOffset)
ACCESSORS(Map, constructor, Object, kConstructorOffset)

ACCESSORS(JSFunction, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
ACCESSORS(JSFunction, literals_or_bindings, FixedArray, kLiteralsOffset)
ACCESSORS(JSFunction, next_function_link, Object, kNextFunctionLinkOffset)

ACCESSORS(GlobalObject, builtins, JSBuiltinsObject, kBuiltinsOffset)
ACCESSORS(GlobalObject, native_context, Context, kNativeContextOffset)
ACCESSORS(GlobalObject, global_context, Context, kGlobalContextOffset)
ACCESSORS(GlobalObject, global_receiver, JSObject, kGlobalReceiverOffset)

ACCESSORS(JSGlobalProxy, native_context, Object, kNativeContextOffset)
ACCESSORS(JSGlobalProxy, hash, Object, kHashOffset)

ACCESSORS(AccessorInfo, name, Object, kNameOffset)
ACCESSORS_TO_SMI(AccessorInfo, flag, kFlagOffset)
ACCESSORS(AccessorInfo, expected_receiver_type, Object,
          kExpectedReceiverTypeOffset)

ACCESSORS(DeclaredAccessorDescriptor, serialized_data, ByteArray,
          kSerializedDataOffset)

ACCESSORS(DeclaredAccessorInfo, descriptor, DeclaredAccessorDescriptor,
          kDescriptorOffset)

ACCESSORS(ExecutableAccessorInfo, getter, Object, kGetterOffset)
ACCESSORS(ExecutableAccessorInfo, setter, Object, kSetterOffset)
ACCESSORS(ExecutableAccessorInfo, data, Object, kDataOffset)

ACCESSORS(Box, value, Object, kValueOffset)

ACCESSORS(AccessorPair, getter, Object, kGetterOffset)
ACCESSORS(AccessorPair, setter, Object, kSetterOffset)
ACCESSORS_TO_SMI(AccessorPair, access_flags, kAccessFlagsOffset)

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
ACCESSORS(TemplateInfo, property_accessors, Object, kPropertyAccessorsOffset)

ACCESSORS(FunctionTemplateInfo, serial_number, Object, kSerialNumberOffset)
ACCESSORS(FunctionTemplateInfo, call_code, Object, kCallCodeOffset)
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
ACCESSORS_TO_SMI(FunctionTemplateInfo, flag, kFlagOffset)

ACCESSORS(ObjectTemplateInfo, constructor, Object, kConstructorOffset)
ACCESSORS(ObjectTemplateInfo, internal_field_count, Object,
          kInternalFieldCountOffset)

ACCESSORS(SignatureInfo, receiver, Object, kReceiverOffset)
ACCESSORS(SignatureInfo, args, Object, kArgsOffset)

ACCESSORS(TypeSwitchInfo, types, Object, kTypesOffset)

ACCESSORS(AllocationSite, transition_info, Object, kTransitionInfoOffset)
ACCESSORS(AllocationSite, nested_site, Object, kNestedSiteOffset)
ACCESSORS_TO_SMI(AllocationSite, pretenure_data, kPretenureDataOffset)
ACCESSORS_TO_SMI(AllocationSite, pretenure_create_count,
                 kPretenureCreateCountOffset)
ACCESSORS(AllocationSite, dependent_code, DependentCode,
          kDependentCodeOffset)
ACCESSORS(AllocationSite, weak_next, Object, kWeakNextOffset)
ACCESSORS(AllocationMemento, allocation_site, Object, kAllocationSiteOffset)

ACCESSORS(Script, source, Object, kSourceOffset)
ACCESSORS(Script, name, Object, kNameOffset)
ACCESSORS(Script, id, Smi, kIdOffset)
ACCESSORS_TO_SMI(Script, line_offset, kLineOffsetOffset)
ACCESSORS_TO_SMI(Script, column_offset, kColumnOffsetOffset)
ACCESSORS(Script, context_data, Object, kContextOffset)
ACCESSORS(Script, wrapper, Foreign, kWrapperOffset)
ACCESSORS_TO_SMI(Script, type, kTypeOffset)
ACCESSORS(Script, line_ends, Object, kLineEndsOffset)
ACCESSORS(Script, eval_from_shared, Object, kEvalFromSharedOffset)
ACCESSORS_TO_SMI(Script, eval_from_instructions_offset,
                 kEvalFrominstructionsOffsetOffset)
ACCESSORS_TO_SMI(Script, flags, kFlagsOffset)
BOOL_ACCESSORS(Script, flags, is_shared_cross_origin, kIsSharedCrossOriginBit)

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


ACCESSORS(DebugInfo, shared, SharedFunctionInfo, kSharedFunctionInfoIndex)
ACCESSORS(DebugInfo, original_code, Code, kOriginalCodeIndex)
ACCESSORS(DebugInfo, code, Code, kPatchedCodeIndex)
ACCESSORS(DebugInfo, break_points, FixedArray, kBreakPointsStateIndex)

ACCESSORS_TO_SMI(BreakPointInfo, code_position, kCodePositionIndex)
ACCESSORS_TO_SMI(BreakPointInfo, source_position, kSourcePositionIndex)
ACCESSORS_TO_SMI(BreakPointInfo, statement_position, kStatementPositionIndex)
ACCESSORS(BreakPointInfo, break_point_objects, Object, kBreakPointObjectsIndex)

ACCESSORS(SharedFunctionInfo, name, Object, kNameOffset)
ACCESSORS(SharedFunctionInfo, optimized_code_map, Object,
                 kOptimizedCodeMapOffset)
ACCESSORS(SharedFunctionInfo, construct_stub, Code, kConstructStubOffset)
ACCESSORS(SharedFunctionInfo, feedback_vector, FixedArray,
          kFeedbackVectorOffset)
ACCESSORS(SharedFunctionInfo, initial_map, Object, kInitialMapOffset)
ACCESSORS(SharedFunctionInfo, instance_class_name, Object,
          kInstanceClassNameOffset)
ACCESSORS(SharedFunctionInfo, function_data, Object, kFunctionDataOffset)
ACCESSORS(SharedFunctionInfo, script, Object, kScriptOffset)
ACCESSORS(SharedFunctionInfo, debug_info, Object, kDebugInfoOffset)
ACCESSORS(SharedFunctionInfo, inferred_name, String, kInferredNameOffset)


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
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_expression,
               kIsExpressionBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_toplevel,
               kIsTopLevelBit)

BOOL_ACCESSORS(SharedFunctionInfo,
               compiler_hints,
               allows_lazy_compilation,
               kAllowLazyCompilation)
BOOL_ACCESSORS(SharedFunctionInfo,
               compiler_hints,
               allows_lazy_compilation_without_context,
               kAllowLazyCompilationWithoutContext)
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
SMI_ACCESSORS(SharedFunctionInfo, opt_count_and_bailout_reason,
              kOptCountAndBailoutReasonOffset)
SMI_ACCESSORS(SharedFunctionInfo, counters, kCountersOffset)
SMI_ACCESSORS(SharedFunctionInfo, ast_node_count, kAstNodeCountOffset)
SMI_ACCESSORS(SharedFunctionInfo, profiler_ticks, kProfilerTicksOffset)

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
  return initial_map() != GetHeap()->undefined_value();
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


StrictMode SharedFunctionInfo::strict_mode() {
  return BooleanBit::get(compiler_hints(), kStrictModeFunction)
      ? STRICT : SLOPPY;
}


void SharedFunctionInfo::set_strict_mode(StrictMode strict_mode) {
  // We only allow mode transitions from sloppy to strict.
  ASSERT(this->strict_mode() == SLOPPY || this->strict_mode() == strict_mode);
  int hints = compiler_hints();
  hints = BooleanBit::set(hints, kStrictModeFunction, strict_mode == STRICT);
  set_compiler_hints(hints);
}


BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, native, kNative)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, inline_builtin,
               kInlineBuiltin)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints,
               name_should_print_as_anonymous,
               kNameShouldPrintAsAnonymous)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, bound, kBoundFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_anonymous, kIsAnonymous)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_function, kIsFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_optimize,
               kDontOptimize)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_inline, kDontInline)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_cache, kDontCache)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_flush, kDontFlush)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_generator, kIsGenerator)

void SharedFunctionInfo::BeforeVisitingPointers() {
  if (IsInobjectSlackTrackingInProgress()) DetachInitialMap();
}


ACCESSORS(CodeCache, default_cache, FixedArray, kDefaultCacheOffset)
ACCESSORS(CodeCache, normal_type_cache, Object, kNormalTypeCacheOffset)

ACCESSORS(PolymorphicCodeCache, cache, Object, kCacheOffset)

bool Script::HasValidSource() {
  Object* src = this->source();
  if (!src->IsString()) return true;
  String* src_str = String::cast(src);
  if (!StringShape(src_str).IsExternal()) return true;
  if (src_str->IsOneByteRepresentation()) {
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


void SharedFunctionInfo::set_code(Code* value, WriteBarrierMode mode) {
  ASSERT(value->kind() != Code::OPTIMIZED_FUNCTION);
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

  ASSERT(code()->gc_metadata() == NULL && value->gc_metadata() == NULL);

  set_code(value);
}


ScopeInfo* SharedFunctionInfo::scope_info() {
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


bool SharedFunctionInfo::is_compiled() {
  return code() !=
      GetIsolate()->builtins()->builtin(Builtins::kCompileUnoptimized);
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
  deopt_count = (deopt_count + 1) & DeoptCountBits::kMax;
  set_counters(DeoptCountBits::update(value, deopt_count));
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


BailoutReason SharedFunctionInfo::DisableOptimizationReason() {
  BailoutReason reason = static_cast<BailoutReason>(
      DisabledOptimizationReasonBits::decode(opt_count_and_bailout_reason()));
  return reason;
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
    set_opt_count(0);
    set_deopt_count(0);
    code()->set_optimizable(true);
  }
}


bool JSFunction::IsBuiltin() {
  return context()->global_object()->IsJSBuiltinsObject();
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


Code* JSFunction::code() {
  return Code::cast(
      Code::GetObjectFromEntryAddress(FIELD_ADDR(this, kCodeEntryOffset)));
}


void JSFunction::set_code(Code* value) {
  ASSERT(!GetHeap()->InNewSpace(value));
  Address entry = value->entry();
  WRITE_INTPTR_FIELD(this, kCodeEntryOffset, reinterpret_cast<intptr_t>(entry));
  GetHeap()->incremental_marking()->RecordWriteOfCodeEntry(
      this,
      HeapObject::RawField(this, kCodeEntryOffset),
      value);
}


void JSFunction::set_code_no_write_barrier(Code* value) {
  ASSERT(!GetHeap()->InNewSpace(value));
  Address entry = value->entry();
  WRITE_INTPTR_FIELD(this, kCodeEntryOffset, reinterpret_cast<intptr_t>(entry));
}


void JSFunction::ReplaceCode(Code* code) {
  bool was_optimized = IsOptimized();
  bool is_optimized = code->kind() == Code::OPTIMIZED_FUNCTION;

  if (was_optimized && is_optimized) {
    shared()->EvictFromOptimizedCodeMap(this->code(),
        "Replacing with another optimized code");
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


Context* JSFunction::context() {
  return Context::cast(READ_FIELD(this, kContextOffset));
}


void JSFunction::set_context(Object* value) {
  ASSERT(value->IsUndefined() || value->IsContext());
  WRITE_FIELD(this, kContextOffset, value);
  WRITE_BARRIER(GetHeap(), this, kContextOffset, value);
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
  return code() !=
      GetIsolate()->builtins()->builtin(Builtins::kCompileUnoptimized);
}


FixedArray* JSFunction::literals() {
  ASSERT(!shared()->bound());
  return literals_or_bindings();
}


void JSFunction::set_literals(FixedArray* literals) {
  ASSERT(!shared()->bound());
  set_literals_or_bindings(literals);
}


FixedArray* JSFunction::function_bindings() {
  ASSERT(shared()->bound());
  return literals_or_bindings();
}


void JSFunction::set_function_bindings(FixedArray* bindings) {
  ASSERT(shared()->bound());
  // Bound function literal may be initialized to the empty fixed array
  // before the bindings are set.
  ASSERT(bindings == GetHeap()->empty_fixed_array() ||
         bindings->map() == GetHeap()->fixed_cow_array_map());
  set_literals_or_bindings(bindings);
}


int JSFunction::NumberOfLiterals() {
  ASSERT(!shared()->bound());
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
  WRITE_BARRIER(GetHeap(), this, OffsetOfFunctionWithId(id), value);
}


Code* JSBuiltinsObject::javascript_builtin_code(Builtins::JavaScript id) {
  ASSERT(id < kJSBuiltinsCount);  // id is unsigned.
  return Code::cast(READ_FIELD(this, OffsetOfCodeWithId(id)));
}


void JSBuiltinsObject::set_javascript_builtin_code(Builtins::JavaScript id,
                                                   Code* value) {
  ASSERT(id < kJSBuiltinsCount);  // id is unsigned.
  WRITE_FIELD(this, OffsetOfCodeWithId(id), value);
  ASSERT(!GetHeap()->InNewSpace(value));
}


ACCESSORS(JSProxy, handler, Object, kHandlerOffset)
ACCESSORS(JSProxy, hash, Object, kHashOffset)
ACCESSORS(JSFunctionProxy, call_trap, Object, kCallTrapOffset)
ACCESSORS(JSFunctionProxy, construct_trap, Object, kConstructTrapOffset)


void JSProxy::InitializeBody(int object_size, Object* value) {
  ASSERT(!value->IsHeapObject() || !GetHeap()->InNewSpace(value));
  for (int offset = kHeaderSize; offset < object_size; offset += kPointerSize) {
    WRITE_FIELD(this, offset, value);
  }
}


ACCESSORS(JSSet, table, Object, kTableOffset)
ACCESSORS(JSMap, table, Object, kTableOffset)


#define ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(name, type, offset)    \
  template<class Derived, class TableType>                           \
  type* OrderedHashTableIterator<Derived, TableType>::name() {       \
    return type::cast(READ_FIELD(this, offset));                     \
  }                                                                  \
  template<class Derived, class TableType>                           \
  void OrderedHashTableIterator<Derived, TableType>::set_##name(     \
      type* value, WriteBarrierMode mode) {                          \
    WRITE_FIELD(this, offset, value);                                \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode); \
  }

ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(table, Object, kTableOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(index, Smi, kIndexOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(count, Smi, kCountOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(kind, Smi, kKindOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(next_iterator, Object,
                                      kNextIteratorOffset)
ORDERED_HASH_TABLE_ITERATOR_ACCESSORS(previous_iterator, Object,
                                      kPreviousIteratorOffset)

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
SMI_ACCESSORS(JSGeneratorObject, continuation, kContinuationOffset)
ACCESSORS(JSGeneratorObject, operand_stack, FixedArray, kOperandStackOffset)
SMI_ACCESSORS(JSGeneratorObject, stack_handler_index, kStackHandlerIndexOffset)

bool JSGeneratorObject::is_suspended() {
  ASSERT_LT(kGeneratorExecuting, kGeneratorClosed);
  ASSERT_EQ(kGeneratorClosed, 0);
  return continuation() > 0;
}

JSGeneratorObject* JSGeneratorObject::cast(Object* obj) {
  ASSERT(obj->IsJSGeneratorObject());
  ASSERT(HeapObject::cast(obj)->Size() == JSGeneratorObject::kSize);
  return reinterpret_cast<JSGeneratorObject*>(obj);
}


ACCESSORS(JSModule, context, Object, kContextOffset)
ACCESSORS(JSModule, scope_info, ScopeInfo, kScopeInfoOffset)


JSModule* JSModule::cast(Object* obj) {
  ASSERT(obj->IsJSModule());
  ASSERT(HeapObject::cast(obj)->Size() == JSModule::kSize);
  return reinterpret_cast<JSModule*>(obj);
}


ACCESSORS(JSValue, value, Object, kValueOffset)


JSValue* JSValue::cast(Object* obj) {
  ASSERT(obj->IsJSValue());
  ASSERT(HeapObject::cast(obj)->Size() == JSValue::kSize);
  return reinterpret_cast<JSValue*>(obj);
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


JSDate* JSDate::cast(Object* obj) {
  ASSERT(obj->IsJSDate());
  ASSERT(HeapObject::cast(obj)->Size() == JSDate::kSize);
  return reinterpret_cast<JSDate*>(obj);
}


ACCESSORS(JSMessageObject, type, String, kTypeOffset)
ACCESSORS(JSMessageObject, arguments, JSArray, kArgumentsOffset)
ACCESSORS(JSMessageObject, script, Object, kScriptOffset)
ACCESSORS(JSMessageObject, stack_frames, Object, kStackFramesOffset)
SMI_ACCESSORS(JSMessageObject, start_position, kStartPositionOffset)
SMI_ACCESSORS(JSMessageObject, end_position, kEndPositionOffset)


JSMessageObject* JSMessageObject::cast(Object* obj) {
  ASSERT(obj->IsJSMessageObject());
  ASSERT(HeapObject::cast(obj)->Size() == JSMessageObject::kSize);
  return reinterpret_cast<JSMessageObject*>(obj);
}


INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, prologue_offset, kPrologueOffset)
ACCESSORS(Code, relocation_info, ByteArray, kRelocationInfoOffset)
ACCESSORS(Code, handler_table, FixedArray, kHandlerTableOffset)
ACCESSORS(Code, deoptimization_data, FixedArray, kDeoptimizationDataOffset)
ACCESSORS(Code, raw_type_feedback_info, Object, kTypeFeedbackInfoOffset)
ACCESSORS(Code, next_code_link, Object, kNextCodeLinkOffset)


void Code::WipeOutHeader() {
  WRITE_FIELD(this, kRelocationInfoOffset, NULL);
  WRITE_FIELD(this, kHandlerTableOffset, NULL);
  WRITE_FIELD(this, kDeoptimizationDataOffset, NULL);
  WRITE_FIELD(this, kConstantPoolOffset, NULL);
  // Do not wipe out e.g. a minor key.
  if (!READ_FIELD(this, kTypeFeedbackInfoOffset)->IsSmi()) {
    WRITE_FIELD(this, kTypeFeedbackInfoOffset, NULL);
  }
}


Object* Code::type_feedback_info() {
  ASSERT(kind() == FUNCTION);
  return raw_type_feedback_info();
}


void Code::set_type_feedback_info(Object* value, WriteBarrierMode mode) {
  ASSERT(kind() == FUNCTION);
  set_raw_type_feedback_info(value, mode);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kTypeFeedbackInfoOffset,
                            value, mode);
}


int Code::stub_info() {
  ASSERT(kind() == COMPARE_IC || kind() == COMPARE_NIL_IC ||
         kind() == BINARY_OP_IC || kind() == LOAD_IC || kind() == CALL_IC);
  return Smi::cast(raw_type_feedback_info())->value();
}


void Code::set_stub_info(int value) {
  ASSERT(kind() == COMPARE_IC ||
         kind() == COMPARE_NIL_IC ||
         kind() == BINARY_OP_IC ||
         kind() == STUB ||
         kind() == LOAD_IC ||
         kind() == CALL_IC ||
         kind() == KEYED_LOAD_IC ||
         kind() == STORE_IC ||
         kind() == KEYED_STORE_IC);
  set_raw_type_feedback_info(Smi::FromInt(value));
}


ACCESSORS(Code, gc_metadata, Object, kGCMetadataOffset)
INT_ACCESSORS(Code, ic_age, kICAgeOffset)


byte* Code::instruction_start()  {
  return FIELD_ADDR(this, kHeaderSize);
}


byte* Code::instruction_end()  {
  return instruction_start() + instruction_size();
}


int Code::body_size() {
  return RoundUp(instruction_size(), kObjectAlignment);
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


ACCESSORS(JSArray, length, Object, kLengthOffset)


void* JSArrayBuffer::backing_store() {
  intptr_t ptr = READ_INTPTR_FIELD(this, kBackingStoreOffset);
  return reinterpret_cast<void*>(ptr);
}


void JSArrayBuffer::set_backing_store(void* value, WriteBarrierMode mode) {
  intptr_t ptr = reinterpret_cast<intptr_t>(value);
  WRITE_INTPTR_FIELD(this, kBackingStoreOffset, ptr);
}


ACCESSORS(JSArrayBuffer, byte_length, Object, kByteLengthOffset)
ACCESSORS_TO_SMI(JSArrayBuffer, flag, kFlagOffset)


bool JSArrayBuffer::is_external() {
  return BooleanBit::get(flag(), kIsExternalBit);
}


void JSArrayBuffer::set_is_external(bool value) {
  set_flag(BooleanBit::set(flag(), kIsExternalBit, value));
}


bool JSArrayBuffer::should_be_freed() {
  return BooleanBit::get(flag(), kShouldBeFreed);
}


void JSArrayBuffer::set_should_be_freed(bool value) {
  set_flag(BooleanBit::set(flag(), kShouldBeFreed, value));
}


ACCESSORS(JSArrayBuffer, weak_next, Object, kWeakNextOffset)
ACCESSORS(JSArrayBuffer, weak_first_view, Object, kWeakFirstViewOffset)


ACCESSORS(JSArrayBufferView, buffer, Object, kBufferOffset)
ACCESSORS(JSArrayBufferView, byte_offset, Object, kByteOffsetOffset)
ACCESSORS(JSArrayBufferView, byte_length, Object, kByteLengthOffset)
ACCESSORS(JSArrayBufferView, weak_next, Object, kWeakNextOffset)
ACCESSORS(JSTypedArray, length, Object, kLengthOffset)

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


ElementsKind JSObject::GetElementsKind() {
  ElementsKind kind = map()->elements_kind();
#if DEBUG
  FixedArrayBase* fixed_array =
      reinterpret_cast<FixedArrayBase*>(READ_FIELD(this, kElementsOffset));

  // If a GC was caused while constructing this object, the elements
  // pointer may point to a one pointer filler map.
  if (ElementsAreSafeToExamine()) {
    Map* map = fixed_array->map();
    ASSERT((IsFastSmiOrObjectElementsKind(kind) &&
            (map == GetHeap()->fixed_array_map() ||
             map == GetHeap()->fixed_cow_array_map())) ||
           (IsFastDoubleElementsKind(kind) &&
            (fixed_array->IsFixedDoubleArray() ||
             fixed_array == GetHeap()->empty_fixed_array())) ||
           (kind == DICTIONARY_ELEMENTS &&
            fixed_array->IsFixedArray() &&
            fixed_array->IsDictionary()) ||
           (kind > DICTIONARY_ELEMENTS));
    ASSERT((kind != SLOPPY_ARGUMENTS_ELEMENTS) ||
           (elements()->IsFixedArray() && elements()->length() >= 2));
  }
#endif
  return kind;
}


ElementsAccessor* JSObject::GetElementsAccessor() {
  return ElementsAccessor::ForKind(GetElementsKind());
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


bool JSObject::HasSloppyArgumentsElements() {
  return GetElementsKind() == SLOPPY_ARGUMENTS_ELEMENTS;
}


bool JSObject::HasExternalArrayElements() {
  HeapObject* array = elements();
  ASSERT(array != NULL);
  return array->IsExternalArray();
}


#define EXTERNAL_ELEMENTS_CHECK(Type, type, TYPE, ctype, size)          \
bool JSObject::HasExternal##Type##Elements() {                          \
  HeapObject* array = elements();                                       \
  ASSERT(array != NULL);                                                \
  if (!array->IsHeapObject())                                           \
    return false;                                                       \
  return array->map()->instance_type() == EXTERNAL_##TYPE##_ARRAY_TYPE; \
}

TYPED_ARRAYS(EXTERNAL_ELEMENTS_CHECK)

#undef EXTERNAL_ELEMENTS_CHECK


bool JSObject::HasFixedTypedArrayElements() {
  HeapObject* array = elements();
  ASSERT(array != NULL);
  return array->IsFixedTypedArrayBase();
}


#define FIXED_TYPED_ELEMENTS_CHECK(Type, type, TYPE, ctype, size)         \
bool JSObject::HasFixed##Type##Elements() {                               \
  HeapObject* array = elements();                                         \
  ASSERT(array != NULL);                                                  \
  if (!array->IsHeapObject())                                             \
    return false;                                                         \
  return array->map()->instance_type() == FIXED_##TYPE##_ARRAY_TYPE;      \
}

TYPED_ARRAYS(FIXED_TYPED_ELEMENTS_CHECK)

#undef FIXED_TYPED_ELEMENTS_CHECK


bool JSObject::HasNamedInterceptor() {
  return map()->has_named_interceptor();
}


bool JSObject::HasIndexedInterceptor() {
  return map()->has_indexed_interceptor();
}


NameDictionary* JSObject::property_dictionary() {
  ASSERT(!HasFastProperties());
  return NameDictionary::cast(properties());
}


SeededNumberDictionary* JSObject::element_dictionary() {
  ASSERT(HasDictionaryElements());
  return SeededNumberDictionary::cast(elements());
}


Handle<JSSetIterator> JSSetIterator::Create(
    Handle<OrderedHashSet> table,
    int kind) {
  return CreateInternal(table->GetIsolate()->set_iterator_map(), table, kind);
}


Handle<JSMapIterator> JSMapIterator::Create(
    Handle<OrderedHashMap> table,
    int kind) {
  return CreateInternal(table->GetIsolate()->map_iterator_map(), table, kind);
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


StringHasher::StringHasher(int length, uint32_t seed)
  : length_(length),
    raw_running_hash_(seed),
    array_index_(0),
    is_array_index_(0 < length_ && length_ <= String::kMaxArrayIndexSize),
    is_first_char_(true) {
  ASSERT(FLAG_randomize_hashes || raw_running_hash_ == 0);
}


bool StringHasher::has_trivial_hash() {
  return length_ > String::kMaxHashCalcLength;
}


uint32_t StringHasher::AddCharacterCore(uint32_t running_hash, uint16_t c) {
  running_hash += c;
  running_hash += (running_hash << 10);
  running_hash ^= (running_hash >> 6);
  return running_hash;
}


uint32_t StringHasher::GetHashCore(uint32_t running_hash) {
  running_hash += (running_hash << 3);
  running_hash ^= (running_hash >> 11);
  running_hash += (running_hash << 15);
  if ((running_hash & String::kHashBitMask) == 0) {
    return kZeroHash;
  }
  return running_hash;
}


void StringHasher::AddCharacter(uint16_t c) {
  // Use the Jenkins one-at-a-time hash function to update the hash
  // for the given character.
  raw_running_hash_ = AddCharacterCore(raw_running_hash_, c);
}


bool StringHasher::UpdateIndex(uint16_t c) {
  ASSERT(is_array_index_);
  if (c < '0' || c > '9') {
    is_array_index_ = false;
    return false;
  }
  int d = c - '0';
  if (is_first_char_) {
    is_first_char_ = false;
    if (c == '0' && length_ > 1) {
      is_array_index_ = false;
      return false;
    }
  }
  if (array_index_ > 429496729U - ((d + 2) >> 3)) {
    is_array_index_ = false;
    return false;
  }
  array_index_ = array_index_ * 10 + d;
  return true;
}


template<typename Char>
inline void StringHasher::AddCharacters(const Char* chars, int length) {
  ASSERT(sizeof(Char) == 1 || sizeof(Char) == 2);
  int i = 0;
  if (is_array_index_) {
    for (; i < length; i++) {
      AddCharacter(chars[i]);
      if (!UpdateIndex(chars[i])) {
        i++;
        break;
      }
    }
  }
  for (; i < length; i++) {
    ASSERT(!is_array_index_);
    AddCharacter(chars[i]);
  }
}


template <typename schar>
uint32_t StringHasher::HashSequentialString(const schar* chars,
                                            int length,
                                            uint32_t seed) {
  StringHasher hasher(length, seed);
  if (!hasher.has_trivial_hash()) hasher.AddCharacters(chars, length);
  return hasher.GetHashField();
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


Object* JSReceiver::GetPrototype() {
  return map()->prototype();
}


Object* JSReceiver::GetConstructor() {
  return map()->constructor();
}


bool JSReceiver::HasProperty(Handle<JSReceiver> object,
                             Handle<Name> name) {
  if (object->IsJSProxy()) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    return JSProxy::HasPropertyWithHandler(proxy, name);
  }
  return GetPropertyAttribute(object, name) != ABSENT;
}


bool JSReceiver::HasLocalProperty(Handle<JSReceiver> object,
                                  Handle<Name> name) {
  if (object->IsJSProxy()) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    return JSProxy::HasPropertyWithHandler(proxy, name);
  }
  return GetLocalPropertyAttribute(object, name) != ABSENT;
}


PropertyAttributes JSReceiver::GetPropertyAttribute(Handle<JSReceiver> object,
                                                    Handle<Name> key) {
  uint32_t index;
  if (object->IsJSObject() && key->AsArrayIndex(&index)) {
    return GetElementAttribute(object, index);
  }
  return GetPropertyAttributeWithReceiver(object, object, key);
}


PropertyAttributes JSReceiver::GetElementAttribute(Handle<JSReceiver> object,
                                                   uint32_t index) {
  if (object->IsJSProxy()) {
    return JSProxy::GetElementAttributeWithHandler(
        Handle<JSProxy>::cast(object), object, index);
  }
  return JSObject::GetElementAttributeWithReceiver(
      Handle<JSObject>::cast(object), object, index, true);
}


bool JSGlobalObject::IsDetached() {
  return JSGlobalProxy::cast(global_receiver())->IsDetachedFrom(this);
}


bool JSGlobalProxy::IsDetachedFrom(GlobalObject* global) {
  return GetPrototype() != global;
}


Handle<Object> JSReceiver::GetOrCreateIdentityHash(Handle<JSReceiver> object) {
  return object->IsJSProxy()
      ? JSProxy::GetOrCreateIdentityHash(Handle<JSProxy>::cast(object))
      : JSObject::GetOrCreateIdentityHash(Handle<JSObject>::cast(object));
}


Object* JSReceiver::GetIdentityHash() {
  return IsJSProxy()
      ? JSProxy::cast(this)->GetIdentityHash()
      : JSObject::cast(this)->GetIdentityHash();
}


bool JSReceiver::HasElement(Handle<JSReceiver> object, uint32_t index) {
  if (object->IsJSProxy()) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    return JSProxy::HasElementWithHandler(proxy, index);
  }
  return JSObject::GetElementAttributeWithReceiver(
      Handle<JSObject>::cast(object), object, index, true) != ABSENT;
}


bool JSReceiver::HasLocalElement(Handle<JSReceiver> object, uint32_t index) {
  if (object->IsJSProxy()) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    return JSProxy::HasElementWithHandler(proxy, index);
  }
  return JSObject::GetElementAttributeWithReceiver(
      Handle<JSObject>::cast(object), object, index, false) != ABSENT;
}


PropertyAttributes JSReceiver::GetLocalElementAttribute(
    Handle<JSReceiver> object, uint32_t index) {
  if (object->IsJSProxy()) {
    return JSProxy::GetElementAttributeWithHandler(
        Handle<JSProxy>::cast(object), object, index);
  }
  return JSObject::GetElementAttributeWithReceiver(
      Handle<JSObject>::cast(object), object, index, false);
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


bool AccessorInfo::IsCompatibleReceiver(Object* receiver) {
  Object* function_template = expected_receiver_type();
  if (!function_template->IsFunctionTemplateInfo()) return true;
  return FunctionTemplateInfo::cast(function_template)->IsTemplateFor(receiver);
}


void AccessorPair::set_access_flags(v8::AccessControl access_control) {
  int current = access_flags()->value();
  current = BooleanBit::set(current,
                            kProhibitsOverwritingBit,
                            access_control & PROHIBITS_OVERWRITING);
  current = BooleanBit::set(current,
                            kAllCanReadBit,
                            access_control & ALL_CAN_READ);
  current = BooleanBit::set(current,
                            kAllCanWriteBit,
                            access_control & ALL_CAN_WRITE);
  set_access_flags(Smi::FromInt(current));
}


bool AccessorPair::all_can_read() {
  return BooleanBit::get(access_flags(), kAllCanReadBit);
}


bool AccessorPair::all_can_write() {
  return BooleanBit::get(access_flags(), kAllCanWriteBit);
}


bool AccessorPair::prohibits_overwriting() {
  return BooleanBit::get(access_flags(), kProhibitsOverwritingBit);
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::SetEntry(int entry,
                                               Handle<Object> key,
                                               Handle<Object> value) {
  SetEntry(entry, key, value, PropertyDetails(Smi::FromInt(0)));
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::SetEntry(int entry,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               PropertyDetails details) {
  ASSERT(!key->IsName() ||
         details.IsDeleted() ||
         details.dictionary_index() > 0);
  int index = DerivedHashTable::EntryToIndex(entry);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = FixedArray::GetWriteBarrierMode(no_gc);
  FixedArray::set(index, *key, mode);
  FixedArray::set(index+1, *value, mode);
  FixedArray::set(index+2, details.AsSmi());
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


Handle<Object> NumberDictionaryShape::AsHandle(Isolate* isolate, uint32_t key) {
  return isolate->factory()->NewNumberFromUint(key);
}


bool NameDictionaryShape::IsMatch(Handle<Name> key, Object* other) {
  // We know that all entries in a hash table had their hash keys created.
  // Use that knowledge to have fast failure.
  if (key->Hash() != Name::cast(other)->Hash()) return false;
  return key->Equals(Name::cast(other));
}


uint32_t NameDictionaryShape::Hash(Handle<Name> key) {
  return key->Hash();
}


uint32_t NameDictionaryShape::HashForObject(Handle<Name> key, Object* other) {
  return Name::cast(other)->Hash();
}


Handle<Object> NameDictionaryShape::AsHandle(Isolate* isolate,
                                             Handle<Name> key) {
  ASSERT(key->IsUniqueName());
  return key;
}


void NameDictionary::DoGenerateNewEnumerationIndices(
    Handle<NameDictionary> dictionary) {
  DerivedDictionary::GenerateNewEnumerationIndices(dictionary);
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


template <int entrysize>
bool WeakHashTableShape<entrysize>::IsMatch(Handle<Object> key, Object* other) {
  return key->SameValue(other);
}


template <int entrysize>
uint32_t WeakHashTableShape<entrysize>::Hash(Handle<Object> key) {
  intptr_t hash = reinterpret_cast<intptr_t>(*key);
  return (uint32_t)(hash & 0xFFFFFFFF);
}


template <int entrysize>
uint32_t WeakHashTableShape<entrysize>::HashForObject(Handle<Object> key,
                                                      Object* other) {
  intptr_t hash = reinterpret_cast<intptr_t>(other);
  return (uint32_t)(hash & 0xFFFFFFFF);
}


template <int entrysize>
Handle<Object> WeakHashTableShape<entrysize>::AsHandle(Isolate* isolate,
                                                       Handle<Object> key) {
  return key;
}


void Map::ClearCodeCache(Heap* heap) {
  // No write barrier is needed since empty_fixed_array is not in new space.
  // Please note this function is used during marking:
  //  - MarkCompactCollector::MarkUnmarkedObject
  //  - IncrementalMarking::Step
  ASSERT(!heap->InNewSpace(heap->empty_fixed_array()));
  WRITE_FIELD(this, kCodeCacheOffset, heap->empty_fixed_array());
}


void JSArray::EnsureSize(Handle<JSArray> array, int required_size) {
  ASSERT(array->HasFastSmiOrObjectElements());
  Handle<FixedArray> elts = handle(FixedArray::cast(array->elements()));
  const int kArraySizeThatFitsComfortablyInNewSpace = 128;
  if (elts->length() < required_size) {
    // Doubling in size would be overkill, but leave some slack to avoid
    // constantly growing.
    Expand(array, required_size + (required_size >> 3));
    // It's a performance benefit to keep a frequently used array in new-space.
  } else if (!array->GetHeap()->new_space()->Contains(*elts) &&
             required_size < kArraySizeThatFitsComfortablyInNewSpace) {
    // Expand will allocate a new backing store in new space even if the size
    // we asked for isn't larger than what we had before.
    Expand(array, required_size);
  }
}


void JSArray::set_length(Smi* length) {
  // Don't need a write barrier for a Smi.
  set_length(static_cast<Object*>(length), SKIP_WRITE_BARRIER);
}


bool JSArray::AllowsSetElementsLength() {
  bool result = elements()->IsFixedArray() || elements()->IsFixedDoubleArray();
  ASSERT(result == !HasExternalArrayElements());
  return result;
}


void JSArray::SetContent(Handle<JSArray> array,
                         Handle<FixedArrayBase> storage) {
  EnsureCanContainElements(array, storage, storage->length(),
                           ALLOW_COPIED_DOUBLE_ELEMENTS);

  ASSERT((storage->map() == array->GetHeap()->fixed_double_array_map() &&
          IsFastDoubleElementsKind(array->GetElementsKind())) ||
         ((storage->map() != array->GetHeap()->fixed_double_array_map()) &&
          (IsFastObjectElementsKind(array->GetElementsKind()) ||
           (IsFastSmiElementsKind(array->GetElementsKind()) &&
            Handle<FixedArray>::cast(storage)->ContainsOnlySmisOrHoles()))));
  array->set_elements(*storage);
  array->set_length(Smi::FromInt(storage->length()));
}


Handle<Object> TypeFeedbackInfo::UninitializedSentinel(Isolate* isolate) {
  return isolate->factory()->uninitialized_symbol();
}


Handle<Object> TypeFeedbackInfo::MegamorphicSentinel(Isolate* isolate) {
  return isolate->factory()->megamorphic_symbol();
}


Handle<Object> TypeFeedbackInfo::MonomorphicArraySentinel(Isolate* isolate,
    ElementsKind elements_kind) {
  return Handle<Object>(Smi::FromInt(static_cast<int>(elements_kind)), isolate);
}


Object* TypeFeedbackInfo::RawUninitializedSentinel(Heap* heap) {
  return heap->uninitialized_symbol();
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


void TypeFeedbackInfo::initialize_storage() {
  WRITE_FIELD(this, kStorage1Offset, Smi::FromInt(0));
  WRITE_FIELD(this, kStorage2Offset, Smi::FromInt(0));
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
  ASSERT_EQ(isolate_->relocatable_top(), this);
  isolate_->set_relocatable_top(prev_);
}


int JSObject::BodyDescriptor::SizeOf(Map* map, HeapObject* object) {
  return map->instance_size();
}


void Foreign::ForeignIterateBody(ObjectVisitor* v) {
  v->VisitExternalReference(
      reinterpret_cast<Address*>(FIELD_ADDR(this, kForeignAddressOffset)));
}


template<typename StaticVisitor>
void Foreign::ForeignIterateBody() {
  StaticVisitor::VisitExternalReference(
      reinterpret_cast<Address*>(FIELD_ADDR(this, kForeignAddressOffset)));
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


template<int start_offset, int end_offset, int size>
void FixedBodyDescriptor<start_offset, end_offset, size>::IterateBody(
    HeapObject* obj,
    ObjectVisitor* v) {
    v->VisitPointers(HeapObject::RawField(obj, start_offset),
                     HeapObject::RawField(obj, end_offset));
}


template<int start_offset>
void FlexibleBodyDescriptor<start_offset>::IterateBody(HeapObject* obj,
                                                       int object_size,
                                                       ObjectVisitor* v) {
  v->VisitPointers(HeapObject::RawField(obj, start_offset),
                   HeapObject::RawField(obj, object_size));
}


#undef TYPE_CHECKER
#undef CAST_ACCESSOR
#undef INT_ACCESSORS
#undef ACCESSORS
#undef ACCESSORS_TO_SMI
#undef SMI_ACCESSORS
#undef SYNCHRONIZED_SMI_ACCESSORS
#undef NOBARRIER_SMI_ACCESSORS
#undef BOOL_GETTER
#undef BOOL_ACCESSORS
#undef FIELD_ADDR
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
#undef READ_UINT32_FIELD
#undef WRITE_UINT32_FIELD
#undef READ_SHORT_FIELD
#undef WRITE_SHORT_FIELD
#undef READ_BYTE_FIELD
#undef WRITE_BYTE_FIELD
#undef NOBARRIER_READ_BYTE_FIELD
#undef NOBARRIER_WRITE_BYTE_FIELD

} }  // namespace v8::internal

#endif  // V8_OBJECTS_INL_H_
