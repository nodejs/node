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
#include "src/contexts-inl.h"
#include "src/conversions-inl.h"
#include "src/factory.h"
#include "src/field-index-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/layout-descriptor-inl.h"
#include "src/lookup.h"
#include "src/objects.h"
#include "src/property.h"
#include "src/prototype.h"
#include "src/transitions-inl.h"
#include "src/type-feedback-vector-inl.h"
#include "src/types-inl.h"
#include "src/v8memory.h"

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


#define TYPE_CHECKER(type, instancetype)                                \
  bool Object::Is##type() const {                                       \
  return Object::IsHeapObject() &&                                      \
      HeapObject::cast(this)->map()->instance_type() == instancetype;   \
  }


#define CAST_ACCESSOR(type)                       \
  type* type::cast(Object* object) {              \
    SLOW_DCHECK(object->Is##type());              \
    return reinterpret_cast<type*>(object);       \
  }                                               \
  const type* type::cast(const Object* object) {  \
    SLOW_DCHECK(object->Is##type());              \
    return reinterpret_cast<const type*>(object); \
  }


#define INT_ACCESSORS(holder, name, offset)                                   \
  int holder::name() const { return READ_INT_FIELD(this, offset); }           \
  void holder::set_##name(int value) { WRITE_INT_FIELD(this, offset, value); }


#define ACCESSORS(holder, name, type, offset)                                 \
  type* holder::name() const { return type::cast(READ_FIELD(this, offset)); } \
  void holder::set_##name(type* value, WriteBarrierMode mode) {               \
    WRITE_FIELD(this, offset, value);                                         \
    CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);          \
  }


// Getter that returns a Smi as an int and writes an int as a Smi.
#define SMI_ACCESSORS(holder, name, offset)             \
  int holder::name() const {                            \
    Object* value = READ_FIELD(this, offset);           \
    return Smi::cast(value)->value();                   \
  }                                                     \
  void holder::set_##name(int value) {                  \
    WRITE_FIELD(this, offset, Smi::FromInt(value));     \
  }

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


bool Object::IsFixedArrayBase() const {
  return IsFixedArray() || IsFixedDoubleArray() || IsFixedTypedArrayBase();
}


bool Object::IsFixedArray() const {
  if (!IsHeapObject()) return false;
  InstanceType instance_type = HeapObject::cast(this)->map()->instance_type();
  return instance_type == FIXED_ARRAY_TYPE ||
         instance_type == TRANSITION_ARRAY_TYPE;
}


// External objects are not extensible, so the map check is enough.
bool Object::IsExternal() const {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->external_map();
}


bool Object::IsAccessorInfo() const { return IsExecutableAccessorInfo(); }


TYPE_CHECKER(HeapNumber, HEAP_NUMBER_TYPE)
TYPE_CHECKER(MutableHeapNumber, MUTABLE_HEAP_NUMBER_TYPE)
TYPE_CHECKER(Symbol, SYMBOL_TYPE)
TYPE_CHECKER(Simd128Value, SIMD128_VALUE_TYPE)


#define SIMD128_TYPE_CHECKER(TYPE, Type, type, lane_count, lane_type) \
  bool Object::Is##Type() const {                                     \
    return Object::IsHeapObject() &&                                  \
           HeapObject::cast(this)->map() ==                           \
               HeapObject::cast(this)->GetHeap()->type##_map();       \
  }
SIMD128_TYPES(SIMD128_TYPE_CHECKER)
#undef SIMD128_TYPE_CHECKER


bool Object::IsString() const {
  return Object::IsHeapObject()
    && HeapObject::cast(this)->map()->instance_type() < FIRST_NONSTRING_TYPE;
}


bool Object::IsName() const {
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  return Object::IsHeapObject() &&
         HeapObject::cast(this)->map()->instance_type() <= LAST_NAME_TYPE;
}


bool Object::IsUniqueName() const {
  return IsInternalizedString() || IsSymbol();
}


bool Object::IsFunction() const {
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  return Object::IsHeapObject() &&
         HeapObject::cast(this)->map()->instance_type() >= FIRST_FUNCTION_TYPE;
}


bool Object::IsCallable() const {
  return Object::IsHeapObject() && HeapObject::cast(this)->map()->is_callable();
}


bool Object::IsConstructor() const {
  return Object::IsHeapObject() &&
         HeapObject::cast(this)->map()->is_constructor();
}


bool Object::IsTemplateInfo() const {
  return IsObjectTemplateInfo() || IsFunctionTemplateInfo();
}


bool Object::IsInternalizedString() const {
  if (!this->IsHeapObject()) return false;
  uint32_t type = HeapObject::cast(this)->map()->instance_type();
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (type & (kIsNotStringMask | kIsNotInternalizedMask)) ==
      (kStringTag | kInternalizedTag);
}


bool Object::IsConsString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsCons();
}


bool Object::IsSlicedString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSliced();
}


bool Object::IsSeqString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential();
}


bool Object::IsSeqOneByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsOneByteRepresentation();
}


bool Object::IsSeqTwoByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsSequential() &&
         String::cast(this)->IsTwoByteRepresentation();
}


bool Object::IsExternalString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal();
}


bool Object::IsExternalOneByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsOneByteRepresentation();
}


bool Object::IsExternalTwoByteString() const {
  if (!IsString()) return false;
  return StringShape(String::cast(this)).IsExternal() &&
         String::cast(this)->IsTwoByteRepresentation();
}


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


Handle<Object> Object::NewStorageFor(Isolate* isolate,
                                     Handle<Object> object,
                                     Representation representation) {
  if (representation.IsSmi() && object->IsUninitialized()) {
    return handle(Smi::FromInt(0), isolate);
  }
  if (!representation.IsDouble()) return object;
  double value;
  if (object->IsUninitialized()) {
    value = 0;
  } else if (object->IsMutableHeapNumber()) {
    value = HeapNumber::cast(*object)->value();
  } else {
    value = object->Number();
  }
  return isolate->factory()->NewHeapNumber(value, MUTABLE);
}


Handle<Object> Object::WrapForRead(Isolate* isolate,
                                   Handle<Object> object,
                                   Representation representation) {
  DCHECK(!object->IsUninitialized());
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


StringShape::StringShape(Map* map)
  : type_(map->instance_type()) {
  set_valid();
  DCHECK((type_ & kIsNotStringMask) == kStringTag);
}


StringShape::StringShape(InstanceType t)
  : type_(static_cast<uint32_t>(t)) {
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

  uint32_t Hash() override {
    hash_field_ = StringHasher::HashSequentialString<Char>(string_.start(),
                                                           string_.length(),
                                                           seed_);

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
      : SequentialStringKey<uint8_t>(str, seed) { }

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
      : SequentialStringKey<uc16>(str, seed) { }

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsTwoByteEqualTo(string_);
  }

  Handle<Object> AsHandle(Isolate* isolate) override;
};


// Utf8StringKey carries a vector of chars as key.
class Utf8StringKey : public HashTableKey {
 public:
  explicit Utf8StringKey(Vector<const char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

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
    return isolate->factory()->NewInternalizedStringFromUtf8(
        string_, chars_, hash_field_);
  }

  Vector<const char> string_;
  uint32_t hash_field_;
  int chars_;  // Caches the number of characters when computing the hash code.
  uint32_t seed_;
};


bool Object::IsNumber() const {
  return IsSmi() || IsHeapNumber();
}


TYPE_CHECKER(ByteArray, BYTE_ARRAY_TYPE)
TYPE_CHECKER(BytecodeArray, BYTECODE_ARRAY_TYPE)
TYPE_CHECKER(FreeSpace, FREE_SPACE_TYPE)


bool Object::IsFiller() const {
  if (!Object::IsHeapObject()) return false;
  InstanceType instance_type = HeapObject::cast(this)->map()->instance_type();
  return instance_type == FREE_SPACE_TYPE || instance_type == FILLER_TYPE;
}



#define TYPED_ARRAY_TYPE_CHECKER(Type, type, TYPE, ctype, size)               \
  TYPE_CHECKER(Fixed##Type##Array, FIXED_##TYPE##_ARRAY_TYPE)

TYPED_ARRAYS(TYPED_ARRAY_TYPE_CHECKER)
#undef TYPED_ARRAY_TYPE_CHECKER


bool Object::IsFixedTypedArrayBase() const {
  if (!Object::IsHeapObject()) return false;

  InstanceType instance_type =
      HeapObject::cast(this)->map()->instance_type();
  return (instance_type >= FIRST_FIXED_TYPED_ARRAY_TYPE &&
          instance_type <= LAST_FIXED_TYPED_ARRAY_TYPE);
}


bool Object::IsJSReceiver() const {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return IsHeapObject() &&
      HeapObject::cast(this)->map()->instance_type() >= FIRST_JS_RECEIVER_TYPE;
}


bool Object::IsJSObject() const {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return IsHeapObject() && HeapObject::cast(this)->map()->IsJSObjectMap();
}


bool Object::IsJSProxy() const {
  if (!Object::IsHeapObject()) return false;
  return  HeapObject::cast(this)->map()->IsJSProxyMap();
}


TYPE_CHECKER(JSSet, JS_SET_TYPE)
TYPE_CHECKER(JSMap, JS_MAP_TYPE)
TYPE_CHECKER(JSSetIterator, JS_SET_ITERATOR_TYPE)
TYPE_CHECKER(JSMapIterator, JS_MAP_ITERATOR_TYPE)
TYPE_CHECKER(JSIteratorResult, JS_ITERATOR_RESULT_TYPE)
TYPE_CHECKER(JSWeakMap, JS_WEAK_MAP_TYPE)
TYPE_CHECKER(JSWeakSet, JS_WEAK_SET_TYPE)
TYPE_CHECKER(JSContextExtensionObject, JS_CONTEXT_EXTENSION_OBJECT_TYPE)
TYPE_CHECKER(Map, MAP_TYPE)
TYPE_CHECKER(FixedDoubleArray, FIXED_DOUBLE_ARRAY_TYPE)
TYPE_CHECKER(WeakFixedArray, FIXED_ARRAY_TYPE)
TYPE_CHECKER(TransitionArray, TRANSITION_ARRAY_TYPE)


bool Object::IsJSWeakCollection() const {
  return IsJSWeakMap() || IsJSWeakSet();
}


bool Object::IsDescriptorArray() const {
  return IsFixedArray();
}


bool Object::IsArrayList() const { return IsFixedArray(); }


bool Object::IsLayoutDescriptor() const {
  return IsSmi() || IsFixedTypedArrayBase();
}


bool Object::IsTypeFeedbackVector() const { return IsFixedArray(); }


bool Object::IsTypeFeedbackMetadata() const { return IsFixedArray(); }


bool Object::IsLiteralsArray() const { return IsFixedArray(); }


bool Object::IsDeoptimizationInputData() const {
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


bool Object::IsDeoptimizationOutputData() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a deoptimization data array.  Since this is used for asserts we can check
  // that the length is plausible though.
  if (FixedArray::cast(this)->length() % 2 != 0) return false;
  return true;
}


bool Object::IsHandlerTable() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a handler table array.
  return true;
}


bool Object::IsDependentCode() const {
  if (!IsFixedArray()) return false;
  // There's actually no way to see the difference between a fixed array and
  // a dependent codes array.
  return true;
}


bool Object::IsContext() const {
  if (!Object::IsHeapObject()) return false;
  Map* map = HeapObject::cast(this)->map();
  Heap* heap = map->GetHeap();
  return (map == heap->function_context_map() ||
      map == heap->catch_context_map() ||
      map == heap->with_context_map() ||
      map == heap->native_context_map() ||
      map == heap->block_context_map() ||
      map == heap->module_context_map() ||
      map == heap->script_context_map());
}


bool Object::IsNativeContext() const {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->native_context_map();
}


bool Object::IsScriptContextTable() const {
  if (!Object::IsHeapObject()) return false;
  Map* map = HeapObject::cast(this)->map();
  Heap* heap = map->GetHeap();
  return map == heap->script_context_table_map();
}


bool Object::IsScopeInfo() const {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->scope_info_map();
}


TYPE_CHECKER(JSBoundFunction, JS_BOUND_FUNCTION_TYPE)
TYPE_CHECKER(JSFunction, JS_FUNCTION_TYPE)


template <> inline bool Is<JSFunction>(Object* obj) {
  return obj->IsJSFunction();
}


TYPE_CHECKER(Code, CODE_TYPE)
TYPE_CHECKER(Oddball, ODDBALL_TYPE)
TYPE_CHECKER(Cell, CELL_TYPE)
TYPE_CHECKER(PropertyCell, PROPERTY_CELL_TYPE)
TYPE_CHECKER(WeakCell, WEAK_CELL_TYPE)
TYPE_CHECKER(SharedFunctionInfo, SHARED_FUNCTION_INFO_TYPE)
TYPE_CHECKER(JSGeneratorObject, JS_GENERATOR_OBJECT_TYPE)
TYPE_CHECKER(JSModule, JS_MODULE_TYPE)
TYPE_CHECKER(JSValue, JS_VALUE_TYPE)
TYPE_CHECKER(JSDate, JS_DATE_TYPE)
TYPE_CHECKER(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)


bool Object::IsStringWrapper() const {
  return IsJSValue() && JSValue::cast(this)->value()->IsString();
}


TYPE_CHECKER(Foreign, FOREIGN_TYPE)


bool Object::IsBoolean() const {
  return IsOddball() &&
      ((Oddball::cast(this)->kind() & Oddball::kNotBooleanMask) == 0);
}


TYPE_CHECKER(JSArray, JS_ARRAY_TYPE)
TYPE_CHECKER(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)
TYPE_CHECKER(JSTypedArray, JS_TYPED_ARRAY_TYPE)
TYPE_CHECKER(JSDataView, JS_DATA_VIEW_TYPE)


bool Object::IsJSArrayBufferView() const {
  return IsJSDataView() || IsJSTypedArray();
}


TYPE_CHECKER(JSRegExp, JS_REGEXP_TYPE)


template <> inline bool Is<JSArray>(Object* obj) {
  return obj->IsJSArray();
}


bool Object::IsHashTable() const {
  return Object::IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->hash_table_map();
}


bool Object::IsWeakHashTable() const {
  return IsHashTable();
}


bool Object::IsDictionary() const {
  return IsHashTable() &&
      this != HeapObject::cast(this)->GetHeap()->string_table();
}


bool Object::IsNameDictionary() const {
  return IsDictionary();
}


bool Object::IsGlobalDictionary() const { return IsDictionary(); }


bool Object::IsSeededNumberDictionary() const {
  return IsDictionary();
}


bool Object::IsUnseededNumberDictionary() const {
  return IsDictionary();
}


bool Object::IsStringTable() const {
  return IsHashTable();
}


bool Object::IsNormalizedMapCache() const {
  return NormalizedMapCache::IsNormalizedMapCache(this);
}


int NormalizedMapCache::GetIndex(Handle<Map> map) {
  return map->Hash() % NormalizedMapCache::kEntries;
}


bool NormalizedMapCache::IsNormalizedMapCache(const Object* obj) {
  if (!obj->IsFixedArray()) return false;
  if (FixedArray::cast(obj)->length() != NormalizedMapCache::kEntries) {
    return false;
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    reinterpret_cast<NormalizedMapCache*>(const_cast<Object*>(obj))->
        NormalizedMapCacheVerify();
  }
#endif
  return true;
}


bool Object::IsCompilationCacheTable() const {
  return IsHashTable();
}


bool Object::IsCodeCacheHashTable() const {
  return IsHashTable();
}


bool Object::IsPolymorphicCodeCacheHashTable() const {
  return IsHashTable();
}


bool Object::IsMapCache() const {
  return IsHashTable();
}


bool Object::IsObjectHashTable() const {
  return IsHashTable();
}


bool Object::IsOrderedHashTable() const {
  return IsHeapObject() &&
      HeapObject::cast(this)->map() ==
      HeapObject::cast(this)->GetHeap()->ordered_hash_table_map();
}


bool Object::IsOrderedHashSet() const {
  return IsOrderedHashTable();
}


bool Object::IsOrderedHashMap() const {
  return IsOrderedHashTable();
}


bool Object::IsPrimitive() const {
  return IsSmi() || HeapObject::cast(this)->map()->IsPrimitiveMap();
}


bool Object::IsJSGlobalProxy() const {
  bool result = IsHeapObject() &&
                (HeapObject::cast(this)->map()->instance_type() ==
                 JS_GLOBAL_PROXY_TYPE);
  DCHECK(!result ||
         HeapObject::cast(this)->map()->is_access_check_needed());
  return result;
}


TYPE_CHECKER(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)


bool Object::IsUndetectableObject() const {
  return IsHeapObject()
    && HeapObject::cast(this)->map()->is_undetectable();
}


bool Object::IsAccessCheckNeeded() const {
  if (!IsHeapObject()) return false;
  if (IsJSGlobalProxy()) {
    const JSGlobalProxy* proxy = JSGlobalProxy::cast(this);
    JSGlobalObject* global = proxy->GetIsolate()->context()->global_object();
    return proxy->IsDetachedFrom(global);
  }
  return HeapObject::cast(this)->map()->is_access_check_needed();
}


bool Object::IsStruct() const {
  if (!IsHeapObject()) return false;
  switch (HeapObject::cast(this)->map()->instance_type()) {
#define MAKE_STRUCT_CASE(NAME, Name, name) case NAME##_TYPE: return true;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    default: return false;
  }
}


#define MAKE_STRUCT_PREDICATE(NAME, Name, name)                         \
  bool Object::Is##Name() const {                                       \
    return Object::IsHeapObject()                                       \
      && HeapObject::cast(this)->map()->instance_type() == NAME##_TYPE; \
  }
  STRUCT_LIST(MAKE_STRUCT_PREDICATE)
#undef MAKE_STRUCT_PREDICATE


bool Object::IsUndefined() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kUndefined;
}


bool Object::IsNull() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kNull;
}


bool Object::IsTheHole() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kTheHole;
}


bool Object::IsException() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kException;
}


bool Object::IsUninitialized() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kUninitialized;
}


bool Object::IsTrue() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kTrue;
}


bool Object::IsFalse() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kFalse;
}


bool Object::IsArgumentsMarker() const {
  return IsOddball() && Oddball::cast(this)->kind() == Oddball::kArgumentMarker;
}


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
  if (IsSmi()) return FAST_SMI_ELEMENTS;
  if (IsNumber()) return FAST_DOUBLE_ELEMENTS;
  return FAST_ELEMENTS;
}


bool Object::FitsRepresentation(Representation representation) {
  if (FLAG_track_fields && representation.IsNone()) {
    return false;
  } else if (FLAG_track_fields && representation.IsSmi()) {
    return IsSmi();
  } else if (FLAG_track_double_fields && representation.IsDouble()) {
    return IsMutableHeapNumber() || IsNumber();
  } else if (FLAG_track_heap_object_fields && representation.IsHeapObject()) {
    return IsHeapObject();
  }
  return true;
}


// static
MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object) {
  return ToObject(
      isolate, object, handle(isolate->context()->native_context(), isolate));
}


// static
MaybeHandle<Object> Object::ToPrimitive(Handle<Object> input,
                                        ToPrimitiveHint hint) {
  if (input->IsPrimitive()) return input;
  return JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input), hint);
}


bool Object::HasSpecificClassOf(String* name) {
  return this->IsJSObject() && (JSObject::cast(this)->class_name() == name);
}


MaybeHandle<Object> Object::GetProperty(Handle<Object> object,
                                        Handle<Name> name,
                                        LanguageMode language_mode) {
  LookupIterator it(object, name);
  return GetProperty(&it, language_mode);
}


MaybeHandle<Object> Object::GetElement(Isolate* isolate, Handle<Object> object,
                                       uint32_t index,
                                       LanguageMode language_mode) {
  LookupIterator it(isolate, object, index);
  return GetProperty(&it, language_mode);
}


MaybeHandle<Object> Object::SetElement(Isolate* isolate, Handle<Object> object,
                                       uint32_t index, Handle<Object> value,
                                       LanguageMode language_mode) {
  LookupIterator it(isolate, object, index);
  MAYBE_RETURN_NULL(
      SetProperty(&it, value, language_mode, MAY_BE_STORE_FROM_KEYED));
  return value;
}


MaybeHandle<Object> Object::GetPrototype(Isolate* isolate,
                                         Handle<Object> receiver) {
  // We don't expect access checks to be needed on JSProxy objects.
  DCHECK(!receiver->IsAccessCheckNeeded() || receiver->IsJSObject());
  PrototypeIterator iter(isolate, receiver,
                         PrototypeIterator::START_AT_RECEIVER);
  do {
    if (!iter.AdvanceFollowingProxies()) return MaybeHandle<Object>();
  } while (!iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN));
  return PrototypeIterator::GetCurrent(iter);
}


MaybeHandle<Object> Object::GetProperty(Isolate* isolate, Handle<Object> object,
                                        const char* name,
                                        LanguageMode language_mode) {
  Handle<String> str = isolate->factory()->InternalizeUtf8String(name);
  return GetProperty(object, str, language_mode);
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

#define WRITE_FIELD(p, offset, value) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)) = value)

#define RELEASE_WRITE_FIELD(p, offset, value)                     \
  base::Release_Store(                                            \
      reinterpret_cast<base::AtomicWord*>(FIELD_ADDR(p, offset)), \
      reinterpret_cast<base::AtomicWord>(value));

#define NOBARRIER_WRITE_FIELD(p, offset, value)                   \
  base::NoBarrier_Store(                                          \
      reinterpret_cast<base::AtomicWord*>(FIELD_ADDR(p, offset)), \
      reinterpret_cast<base::AtomicWord>(value));

#define WRITE_BARRIER(heap, object, offset, value)                      \
  heap->incremental_marking()->RecordWrite(                             \
      object, HeapObject::RawField(object, offset), value);             \
  if (heap->InNewSpace(value)) {                                        \
    heap->RecordWrite(object->address(), offset);                       \
  }

#define CONDITIONAL_WRITE_BARRIER(heap, object, offset, value, mode) \
  if (mode != SKIP_WRITE_BARRIER) {                                  \
    if (mode == UPDATE_WRITE_BARRIER) {                              \
      heap->incremental_marking()->RecordWrite(                      \
          object, HeapObject::RawField(object, offset), value);      \
    }                                                                \
    if (heap->InNewSpace(value)) {                                   \
      heap->RecordWrite(object->address(), offset);                  \
    }                                                                \
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


bool MapWord::IsForwardingAddress() {
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
  Heap* heap =
      MemoryChunk::FromAddress(reinterpret_cast<const byte*>(this))->heap();
  SLOW_DCHECK(heap != NULL);
  return heap;
}


Isolate* HeapObject::GetIsolate() const {
  return GetHeap()->isolate();
}


Map* HeapObject::map() const {
#ifdef DEBUG
  // Clear mark potentially added by PathTracer.
  uintptr_t raw_value =
      map_word().ToRawValue() & ~static_cast<uintptr_t>(PathTracer::kMarkTag);
  return MapWord::FromRawValue(raw_value).ToMap();
#else
  return map_word().ToMap();
#endif
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


int HeapNumber::get_exponent() {
  return ((READ_INT_FIELD(this, kExponentOffset) & kExponentMask) >>
          kExponentShift) - kExponentBias;
}


int HeapNumber::get_sign() {
  return READ_INT_FIELD(this, kExponentOffset) & kSignMask;
}


bool Simd128Value::Equals(Simd128Value* that) {
#define SIMD128_VALUE(TYPE, Type, type, lane_count, lane_type) \
  if (this->Is##Type()) {                                      \
    if (!that->Is##Type()) return false;                       \
    return Type::cast(this)->Equals(Type::cast(that));         \
  }
  SIMD128_TYPES(SIMD128_VALUE)
#undef SIMD128_VALUE
  return false;
}


// static
bool Simd128Value::Equals(Handle<Simd128Value> one, Handle<Simd128Value> two) {
  return one->Equals(*two);
}


#define SIMD128_VALUE_EQUALS(TYPE, Type, type, lane_count, lane_type) \
  bool Type::Equals(Type* that) {                                     \
    for (int lane = 0; lane < lane_count; ++lane) {                   \
      if (this->get_lane(lane) != that->get_lane(lane)) return false; \
    }                                                                 \
    return true;                                                      \
  }
SIMD128_TYPES(SIMD128_VALUE_EQUALS)
#undef SIMD128_VALUE_EQUALS


#if defined(V8_TARGET_LITTLE_ENDIAN)
#define SIMD128_READ_LANE(lane_type, lane_count, field_type, field_size) \
  lane_type value =                                                      \
      READ_##field_type##_FIELD(this, kValueOffset + lane * field_size);
#elif defined(V8_TARGET_BIG_ENDIAN)
#define SIMD128_READ_LANE(lane_type, lane_count, field_type, field_size) \
  lane_type value = READ_##field_type##_FIELD(                           \
      this, kValueOffset + (lane_count - lane - 1) * field_size);
#else
#error Unknown byte ordering
#endif

#if defined(V8_TARGET_LITTLE_ENDIAN)
#define SIMD128_WRITE_LANE(lane_count, field_type, field_size, value) \
  WRITE_##field_type##_FIELD(this, kValueOffset + lane * field_size, value);
#elif defined(V8_TARGET_BIG_ENDIAN)
#define SIMD128_WRITE_LANE(lane_count, field_type, field_size, value) \
  WRITE_##field_type##_FIELD(                                         \
      this, kValueOffset + (lane_count - lane - 1) * field_size, value);
#else
#error Unknown byte ordering
#endif

#define SIMD128_NUMERIC_LANE_FNS(type, lane_type, lane_count, field_type, \
                                 field_size)                              \
  lane_type type::get_lane(int lane) const {                              \
    DCHECK(lane < lane_count && lane >= 0);                               \
    SIMD128_READ_LANE(lane_type, lane_count, field_type, field_size)      \
    return value;                                                         \
  }                                                                       \
                                                                          \
  void type::set_lane(int lane, lane_type value) {                        \
    DCHECK(lane < lane_count && lane >= 0);                               \
    SIMD128_WRITE_LANE(lane_count, field_type, field_size, value)         \
  }

SIMD128_NUMERIC_LANE_FNS(Float32x4, float, 4, FLOAT, kFloatSize)
SIMD128_NUMERIC_LANE_FNS(Int32x4, int32_t, 4, INT32, kInt32Size)
SIMD128_NUMERIC_LANE_FNS(Uint32x4, uint32_t, 4, UINT32, kInt32Size)
SIMD128_NUMERIC_LANE_FNS(Int16x8, int16_t, 8, INT16, kShortSize)
SIMD128_NUMERIC_LANE_FNS(Uint16x8, uint16_t, 8, UINT16, kShortSize)
SIMD128_NUMERIC_LANE_FNS(Int8x16, int8_t, 16, INT8, kCharSize)
SIMD128_NUMERIC_LANE_FNS(Uint8x16, uint8_t, 16, UINT8, kCharSize)
#undef SIMD128_NUMERIC_LANE_FNS


#define SIMD128_BOOLEAN_LANE_FNS(type, lane_type, lane_count, field_type, \
                                 field_size)                              \
  bool type::get_lane(int lane) const {                                   \
    DCHECK(lane < lane_count && lane >= 0);                               \
    SIMD128_READ_LANE(lane_type, lane_count, field_type, field_size)      \
    DCHECK(value == 0 || value == -1);                                    \
    return value != 0;                                                    \
  }                                                                       \
                                                                          \
  void type::set_lane(int lane, bool value) {                             \
    DCHECK(lane < lane_count && lane >= 0);                               \
    int32_t int_val = value ? -1 : 0;                                     \
    SIMD128_WRITE_LANE(lane_count, field_type, field_size, int_val)       \
  }

SIMD128_BOOLEAN_LANE_FNS(Bool32x4, int32_t, 4, INT32, kInt32Size)
SIMD128_BOOLEAN_LANE_FNS(Bool16x8, int16_t, 8, INT16, kShortSize)
SIMD128_BOOLEAN_LANE_FNS(Bool8x16, int8_t, 16, INT8, kCharSize)
#undef SIMD128_BOOLEAN_LANE_FNS

#undef SIMD128_READ_LANE
#undef SIMD128_WRITE_LANE


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


void AllocationSite::Initialize() {
  set_transition_info(Smi::FromInt(0));
  SetElementsKind(GetInitialFastElementsKind());
  set_nested_site(Smi::FromInt(0));
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


AllocationSiteMode AllocationSite::GetMode(ElementsKind from,
                                           ElementsKind to) {
  if (IsFastSmiElementsKind(from) &&
      IsMoreGeneralElementsKindTransition(from, to)) {
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
                 this, create_count, found_count, ratio,
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
    DCHECK(mode != ALLOW_COPIED_DOUBLE_ELEMENTS);
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
          (*value == object->GetHeap()->empty_fixed_array())) ==
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
  DCHECK(map()->has_indexed_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->GetConstructor());
  DCHECK(constructor->shared()->IsApiFunction());
  Object* result =
      constructor->shared()->get_api_func_data()->indexed_property_handler();
  return InterceptorInfo::cast(result);
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
  WRITE_FIELD(this, kValueOffset, Smi::FromInt(0));
}


void WeakCell::initialize(HeapObject* val) {
  WRITE_FIELD(this, kValueOffset, val);
  Heap* heap = GetHeap();
  // We just have to execute the generational barrier here because we never
  // mark through a weak cell and collect evacuation candidates when we process
  // all weak cells.
  if (heap->InNewSpace(val)) {
    heap->RecordWrite(address(), kValueOffset);
  }
}


bool WeakCell::cleared() const { return value() == Smi::FromInt(0); }


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


bool WeakCell::next_cleared() { return next()->IsTheHole(); }


int JSObject::GetHeaderSize() { return GetHeaderSize(map()->instance_type()); }


int JSObject::GetHeaderSize(InstanceType type) {
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
    case JS_ITERATOR_RESULT_TYPE:
      return JSIteratorResult::kSize;
    case JS_WEAK_MAP_TYPE:
      return JSWeakMap::kSize;
    case JS_WEAK_SET_TYPE:
      return JSWeakSet::kSize;
    case JS_PROMISE_TYPE:
      return JSObject::kHeaderSize;
    case JS_REGEXP_TYPE:
      return JSRegExp::kSize;
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
      return JSObject::kHeaderSize;
    case JS_MESSAGE_OBJECT_TYPE:
      return JSMessageObject::kSize;
    default:
      UNREACHABLE();
      return 0;
  }
}


int JSObject::GetInternalFieldCount(Map* map) {
  int instance_size = map->instance_size();
  if (instance_size == kVariableSizeSentinel) return 0;
  InstanceType instance_type = map->instance_type();
  return ((instance_size - GetHeaderSize(instance_type)) >> kPointerSizeLog2) -
         map->GetInObjectProperties();
}


int JSObject::GetInternalFieldCount() { return GetInternalFieldCount(map()); }


int JSObject::GetInternalFieldOffset(int index) {
  DCHECK(index < GetInternalFieldCount() && index >= 0);
  return GetHeaderSize() + (kPointerSize * index);
}


Object* JSObject::GetInternalField(int index) {
  DCHECK(index < GetInternalFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  return READ_FIELD(this, GetHeaderSize() + (kPointerSize * index));
}


void JSObject::SetInternalField(int index, Object* value) {
  DCHECK(index < GetInternalFieldCount() && index >= 0);
  // Internal objects do follow immediately after the header, whereas in-object
  // properties are at the end of the object. Therefore there is no need
  // to adjust the index here.
  int offset = GetHeaderSize() + (kPointerSize * index);
  WRITE_FIELD(this, offset, value);
  WRITE_BARRIER(GetHeap(), this, offset, value);
}


void JSObject::SetInternalField(int index, Smi* value) {
  DCHECK(index < GetInternalFieldCount() && index >= 0);
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


void JSObject::RawFastPropertyAtPut(FieldIndex index, Object* value) {
  if (index.is_inobject()) {
    int offset = index.offset();
    WRITE_FIELD(this, offset, value);
    WRITE_BARRIER(GetHeap(), this, offset, value);
  } else {
    properties()->set(index.outobject_array_index(), value);
  }
}


void JSObject::RawFastDoublePropertyAtPut(FieldIndex index, double value) {
  WRITE_DOUBLE_FIELD(this, index.offset(), value);
}


void JSObject::FastPropertyAtPut(FieldIndex index, Object* value) {
  if (IsUnboxedDoubleField(index)) {
    DCHECK(value->IsMutableHeapNumber());
    RawFastDoublePropertyAtPut(index, HeapNumber::cast(value)->value());
  } else {
    RawFastPropertyAtPut(index, value);
  }
}


void JSObject::WriteToField(int descriptor, Object* value) {
  DisallowHeapAllocation no_gc;

  DescriptorArray* desc = map()->instance_descriptors();
  PropertyDetails details = desc->GetDetails(descriptor);

  DCHECK(details.type() == DATA);

  FieldIndex index = FieldIndex::ForDescriptor(map(), descriptor);
  if (details.representation().IsDouble()) {
    // Nothing more to be done.
    if (value->IsUninitialized()) return;
    if (IsUnboxedDoubleField(index)) {
      RawFastDoublePropertyAtPut(index, value->Number());
    } else {
      HeapNumber* box = HeapNumber::cast(RawFastPropertyAt(index));
      DCHECK(box->IsMutableHeapNumber());
      box->set_value(value->Number());
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


bool Object::IsStringObjectWithCharacterAt(uint32_t index) {
  if (!this->IsJSValue()) return false;

  JSValue* js_value = JSValue::cast(this);
  if (!js_value->value()->IsString()) return false;

  String* str = String::cast(js_value->value());
  if (index >= static_cast<uint32_t>(str->length())) return false;

  return true;
}


void Object::VerifyApiCallResultType() {
#if DEBUG
  if (!(IsSmi() || IsString() || IsSymbol() || IsJSReceiver() ||
        IsHeapNumber() || IsSimd128Value() || IsUndefined() || IsTrue() ||
        IsFalse() || IsNull())) {
    FATAL("API call returned invalid object");
  }
#endif  // DEBUG
}


Object* FixedArray::get(int index) const {
  SLOW_DCHECK(index >= 0 && index < this->length());
  return READ_FIELD(this, kHeaderSize + index * kPointerSize);
}


Handle<Object> FixedArray::get(Handle<FixedArray> array, int index) {
  return handle(array->get(index), array->GetIsolate());
}


bool FixedArray::is_the_hole(int index) {
  return get(index) == GetHeap()->the_hole_value();
}


void FixedArray::set(int index, Smi* value) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map());
  DCHECK(index >= 0 && index < this->length());
  DCHECK(reinterpret_cast<Object*>(value)->IsSmi());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
}


void FixedArray::set(int index, Object* value) {
  DCHECK_NE(GetHeap()->fixed_cow_array_map(), map());
  DCHECK(IsFixedArray());
  DCHECK(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
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


Handle<Object> FixedDoubleArray::get(Handle<FixedDoubleArray> array,
                                     int index) {
  if (array->is_the_hole(index)) {
    return array->GetIsolate()->factory()->the_hole_value();
  } else {
    return array->GetIsolate()->factory()->NewNumber(array->get_scalar(index));
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


void FixedDoubleArray::set_the_hole(int index) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map() &&
         map() != GetHeap()->fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  WRITE_UINT64_FIELD(this, offset, kHoleNanInt64);
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
  FixedArray::cast(this)->set(index + kFirstIndex, Smi::FromInt(0));
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


int ArrayList::Length() {
  if (FixedArray::cast(this)->length() == 0) return 0;
  return Smi::cast(FixedArray::cast(this)->get(kLengthIndex))->value();
}


void ArrayList::SetLength(int length) {
  return FixedArray::cast(this)->set(kLengthIndex, Smi::FromInt(length));
}


Object* ArrayList::Get(int index) {
  return FixedArray::cast(this)->get(kFirstIndex + index);
}


Object** ArrayList::Slot(int index) {
  return data_start() + kFirstIndex + index;
}


void ArrayList::Set(int index, Object* obj) {
  FixedArray::cast(this)->set(kFirstIndex + index, obj);
}


void ArrayList::Clear(int index, Object* undefined) {
  DCHECK(undefined->IsUndefined());
  FixedArray::cast(this)
      ->set(kFirstIndex + index, undefined, SKIP_WRITE_BARRIER);
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
  if (IsSimd128Value()) return kSimd128Unaligned;
#endif  // V8_HOST_ARCH_32_BIT
  return kWordAligned;
}


void FixedArray::set(int index,
                     Object* value,
                     WriteBarrierMode mode) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map());
  DCHECK(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kPointerSize;
  WRITE_FIELD(this, offset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, offset, value, mode);
}


void FixedArray::NoWriteBarrierSet(FixedArray* array,
                                   int index,
                                   Object* value) {
  DCHECK(array->map() != array->GetHeap()->fixed_cow_array_map());
  DCHECK(index >= 0 && index < array->length());
  DCHECK(!array->GetHeap()->InNewSpace(value));
  WRITE_FIELD(array, kHeaderSize + index * kPointerSize, value);
}


void FixedArray::set_undefined(int index) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map());
  DCHECK(index >= 0 && index < this->length());
  DCHECK(!GetHeap()->InNewSpace(GetHeap()->undefined_value()));
  WRITE_FIELD(this,
              kHeaderSize + index * kPointerSize,
              GetHeap()->undefined_value());
}


void FixedArray::set_null(int index) {
  DCHECK(index >= 0 && index < this->length());
  DCHECK(!GetHeap()->InNewSpace(GetHeap()->null_value()));
  WRITE_FIELD(this,
              kHeaderSize + index * kPointerSize,
              GetHeap()->null_value());
}


void FixedArray::set_the_hole(int index) {
  DCHECK(map() != GetHeap()->fixed_cow_array_map());
  DCHECK(index >= 0 && index < this->length());
  DCHECK(!GetHeap()->InNewSpace(GetHeap()->the_hole_value()));
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
  return len == 0 ? 0 : (len - kFirstIndex) / kDescriptorSize;
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
  return !IsEmpty() && !get(kEnumCacheIndex)->IsSmi();
}


void DescriptorArray::CopyEnumCacheFrom(DescriptorArray* array) {
  set(kEnumCacheIndex, array->get(kEnumCacheIndex));
}


FixedArray* DescriptorArray::GetEnumCache() {
  DCHECK(HasEnumCache());
  FixedArray* bridge = FixedArray::cast(get(kEnumCacheIndex));
  return FixedArray::cast(bridge->get(kEnumCacheBridgeCacheIndex));
}


bool DescriptorArray::HasEnumIndicesCache() {
  if (IsEmpty()) return false;
  Object* object = get(kEnumCacheIndex);
  if (object->IsSmi()) return false;
  FixedArray* bridge = FixedArray::cast(object);
  return !bridge->get(kEnumCacheBridgeIndicesCacheIndex)->IsSmi();
}


FixedArray* DescriptorArray::GetEnumIndicesCache() {
  DCHECK(HasEnumIndicesCache());
  FixedArray* bridge = FixedArray::cast(get(kEnumCacheIndex));
  return FixedArray::cast(bridge->get(kEnumCacheBridgeIndicesCacheIndex));
}


Object** DescriptorArray::GetEnumCacheSlot() {
  DCHECK(HasEnumCache());
  return HeapObject::RawField(reinterpret_cast<HeapObject*>(this),
                              kEnumCacheOffset);
}


// Perform a binary search in a fixed array. Low and high are entry indices. If
// there are three entries in this array it should be called with low=0 and
// high=2.
template <SearchMode search_mode, typename T>
int BinarySearch(T* array, Name* name, int low, int high, int valid_entries,
                 int* out_insertion_index) {
  DCHECK(search_mode == ALL_ENTRIES || out_insertion_index == NULL);
  uint32_t hash = name->Hash();
  int limit = high;

  DCHECK(low <= high);

  while (low != high) {
    int mid = low + (high - low) / 2;
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
    uint32_t current_hash = entry->Hash();
    if (current_hash != hash) {
      if (out_insertion_index != NULL) {
        *out_insertion_index = sort_index + (current_hash > hash ? 0 : 1);
      }
      return T::kNotFound;
    }
    if (entry->Equals(name)) {
      if (search_mode == ALL_ENTRIES || sort_index < valid_entries) {
        return sort_index;
      }
      return T::kNotFound;
    }
  }

  if (out_insertion_index != NULL) *out_insertion_index = limit + 1;
  return T::kNotFound;
}


// Perform a linear search in this fixed array. len is the number of entry
// indices that are valid.
template <SearchMode search_mode, typename T>
int LinearSearch(T* array, Name* name, int len, int valid_entries,
                 int* out_insertion_index) {
  uint32_t hash = name->Hash();
  if (search_mode == ALL_ENTRIES) {
    for (int number = 0; number < len; number++) {
      int sorted_index = array->GetSortedKeyIndex(number);
      Name* entry = array->GetKey(sorted_index);
      uint32_t current_hash = entry->Hash();
      if (current_hash > hash) {
        if (out_insertion_index != NULL) *out_insertion_index = sorted_index;
        return T::kNotFound;
      }
      if (current_hash == hash && entry->Equals(name)) return sorted_index;
    }
    if (out_insertion_index != NULL) *out_insertion_index = len;
    return T::kNotFound;
  } else {
    DCHECK(len >= valid_entries);
    DCHECK_NULL(out_insertion_index);  // Not supported here.
    for (int number = 0; number < valid_entries; number++) {
      Name* entry = array->GetKey(number);
      uint32_t current_hash = entry->Hash();
      if (current_hash == hash && entry->Equals(name)) return number;
    }
    return T::kNotFound;
  }
}


template <SearchMode search_mode, typename T>
int Search(T* array, Name* name, int valid_entries, int* out_insertion_index) {
  if (search_mode == VALID_ENTRIES) {
    SLOW_DCHECK(array->IsSortedNoDuplicates(valid_entries));
  } else {
    SLOW_DCHECK(array->IsSortedNoDuplicates());
  }

  int nof = array->number_of_entries();
  if (nof == 0) {
    if (out_insertion_index != NULL) *out_insertion_index = 0;
    return T::kNotFound;
  }

  // Fast case: do linear search for small arrays.
  const int kMaxElementsForLinearSearch = 8;
  if ((search_mode == ALL_ENTRIES &&
       nof <= kMaxElementsForLinearSearch) ||
      (search_mode == VALID_ENTRIES &&
       valid_entries <= (kMaxElementsForLinearSearch * 3))) {
    return LinearSearch<search_mode>(array, name, nof, valid_entries,
                                     out_insertion_index);
  }

  // Slow case: perform binary search.
  return BinarySearch<search_mode>(array, name, 0, nof - 1, valid_entries,
                                   out_insertion_index);
}


int DescriptorArray::Search(Name* name, int valid_descriptors) {
  return internal::Search<VALID_ENTRIES>(this, name, valid_descriptors, NULL);
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
  if (has_fast_smi_or_object_elements() ||
      has_fast_double_elements()) {
    DCHECK(!GetHeap()->InNewSpace(GetHeap()->empty_fixed_array()));
    return GetHeap()->empty_fixed_array();
  } else if (has_fixed_typed_array_elements()) {
    FixedTypedArrayBase* empty_array =
        GetHeap()->EmptyFixedTypedArrayForMap(this);
    DCHECK(!GetHeap()->InNewSpace(empty_array));
    return empty_array;
  } else {
    UNREACHABLE();
  }
  return NULL;
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


void DescriptorArray::SetRepresentation(int descriptor_index,
                                        Representation representation) {
  DCHECK(!representation.IsNone());
  PropertyDetails details = GetDetails(descriptor_index);
  set(ToDetailsIndex(descriptor_index),
      details.CopyWithRepresentation(representation).AsSmi());
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


PropertyType DescriptorArray::GetType(int descriptor_number) {
  return GetDetails(descriptor_number).type();
}


int DescriptorArray::GetFieldIndex(int descriptor_number) {
  DCHECK(GetDetails(descriptor_number).location() == kField);
  return GetDetails(descriptor_number).field_index();
}


HeapType* DescriptorArray::GetFieldType(int descriptor_number) {
  DCHECK(GetDetails(descriptor_number).location() == kField);
  Object* value = GetValue(descriptor_number);
  if (value->IsWeakCell()) {
    if (WeakCell::cast(value)->cleared()) return HeapType::None();
    value = WeakCell::cast(value)->value();
  }
  return HeapType::cast(value);
}


Object* DescriptorArray::GetConstant(int descriptor_number) {
  return GetValue(descriptor_number);
}


Object* DescriptorArray::GetCallbacksObject(int descriptor_number) {
  DCHECK(GetType(descriptor_number) == ACCESSOR_CONSTANT);
  return GetValue(descriptor_number);
}


AccessorDescriptor* DescriptorArray::GetCallbacks(int descriptor_number) {
  DCHECK(GetType(descriptor_number) == ACCESSOR_CONSTANT);
  Foreign* p = Foreign::cast(GetCallbacksObject(descriptor_number));
  return reinterpret_cast<AccessorDescriptor*>(p->foreign_address());
}


void DescriptorArray::Get(int descriptor_number, Descriptor* desc) {
  desc->Init(handle(GetKey(descriptor_number), GetIsolate()),
             handle(GetValue(descriptor_number), GetIsolate()),
             GetDetails(descriptor_number));
}


void DescriptorArray::SetDescriptor(int descriptor_number, Descriptor* desc) {
  // Range check.
  DCHECK(descriptor_number < number_of_descriptors());
  set(ToKeyIndex(descriptor_number), *desc->GetKey());
  set(ToValueIndex(descriptor_number), *desc->GetValue());
  set(ToDetailsIndex(descriptor_number), desc->GetDetails().AsSmi());
}


void DescriptorArray::Set(int descriptor_number, Descriptor* desc) {
  // Range check.
  DCHECK(descriptor_number < number_of_descriptors());

  set(ToKeyIndex(descriptor_number), *desc->GetKey());
  set(ToValueIndex(descriptor_number), *desc->GetValue());
  set(ToDetailsIndex(descriptor_number), desc->GetDetails().AsSmi());
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


PropertyType DescriptorArray::Entry::type() { return descs_->GetType(index_); }


Object* DescriptorArray::Entry::GetCallbackObject() {
  return descs_->GetValue(index_);
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
  const int kMinCapacity = 4;
  int capacity = base::bits::RoundUpToPowerOfTwo32(at_least_space_for * 2);
  return Max(capacity, kMinCapacity);
}


bool HashTableBase::IsKey(Object* k) {
  return !k->IsTheHole() && !k->IsUndefined();
}


void HashTableBase::SetNumberOfElements(int nof) {
  set(kNumberOfElementsIndex, Smi::FromInt(nof));
}


void HashTableBase::SetNumberOfDeletedElements(int nod) {
  set(kNumberOfDeletedElementsIndex, Smi::FromInt(nod));
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
  while (true) {
    Object* element = KeyAt(entry);
    // Empty entry. Uses raw unchecked accessors because it is called by the
    // string table during bootstrapping.
    if (element == isolate->heap()->root(Heap::kUndefinedValueRootIndex)) break;
    if (element != isolate->heap()->root(Heap::kTheHoleValueRootIndex) &&
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
  DCHECK(!requires_slow_elements());
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


CAST_ACCESSOR(AccessorInfo)
CAST_ACCESSOR(ArrayList)
CAST_ACCESSOR(Bool16x8)
CAST_ACCESSOR(Bool32x4)
CAST_ACCESSOR(Bool8x16)
CAST_ACCESSOR(ByteArray)
CAST_ACCESSOR(BytecodeArray)
CAST_ACCESSOR(Cell)
CAST_ACCESSOR(Code)
CAST_ACCESSOR(CodeCacheHashTable)
CAST_ACCESSOR(CompilationCacheTable)
CAST_ACCESSOR(ConsString)
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
CAST_ACCESSOR(Float32x4)
CAST_ACCESSOR(Foreign)
CAST_ACCESSOR(GlobalDictionary)
CAST_ACCESSOR(HandlerTable)
CAST_ACCESSOR(HeapObject)
CAST_ACCESSOR(Int16x8)
CAST_ACCESSOR(Int32x4)
CAST_ACCESSOR(Int8x16)
CAST_ACCESSOR(JSArray)
CAST_ACCESSOR(JSArrayBuffer)
CAST_ACCESSOR(JSArrayBufferView)
CAST_ACCESSOR(JSBoundFunction)
CAST_ACCESSOR(JSDataView)
CAST_ACCESSOR(JSDate)
CAST_ACCESSOR(JSFunction)
CAST_ACCESSOR(JSGeneratorObject)
CAST_ACCESSOR(JSGlobalObject)
CAST_ACCESSOR(JSGlobalProxy)
CAST_ACCESSOR(JSMap)
CAST_ACCESSOR(JSMapIterator)
CAST_ACCESSOR(JSMessageObject)
CAST_ACCESSOR(JSModule)
CAST_ACCESSOR(JSObject)
CAST_ACCESSOR(JSProxy)
CAST_ACCESSOR(JSReceiver)
CAST_ACCESSOR(JSRegExp)
CAST_ACCESSOR(JSSet)
CAST_ACCESSOR(JSSetIterator)
CAST_ACCESSOR(JSIteratorResult)
CAST_ACCESSOR(JSTypedArray)
CAST_ACCESSOR(JSValue)
CAST_ACCESSOR(JSWeakMap)
CAST_ACCESSOR(JSWeakSet)
CAST_ACCESSOR(LayoutDescriptor)
CAST_ACCESSOR(Map)
CAST_ACCESSOR(Name)
CAST_ACCESSOR(NameDictionary)
CAST_ACCESSOR(NormalizedMapCache)
CAST_ACCESSOR(Object)
CAST_ACCESSOR(ObjectHashTable)
CAST_ACCESSOR(Oddball)
CAST_ACCESSOR(OrderedHashMap)
CAST_ACCESSOR(OrderedHashSet)
CAST_ACCESSOR(PolymorphicCodeCacheHashTable)
CAST_ACCESSOR(PropertyCell)
CAST_ACCESSOR(ScopeInfo)
CAST_ACCESSOR(SeededNumberDictionary)
CAST_ACCESSOR(SeqOneByteString)
CAST_ACCESSOR(SeqString)
CAST_ACCESSOR(SeqTwoByteString)
CAST_ACCESSOR(SharedFunctionInfo)
CAST_ACCESSOR(Simd128Value)
CAST_ACCESSOR(SlicedString)
CAST_ACCESSOR(Smi)
CAST_ACCESSOR(String)
CAST_ACCESSOR(StringTable)
CAST_ACCESSOR(Struct)
CAST_ACCESSOR(Symbol)
CAST_ACCESSOR(Uint16x8)
CAST_ACCESSOR(Uint32x4)
CAST_ACCESSOR(Uint8x16)
CAST_ACCESSOR(UnseededNumberDictionary)
CAST_ACCESSOR(WeakCell)
CAST_ACCESSOR(WeakFixedArray)
CAST_ACCESSOR(WeakHashTable)


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


Object* LiteralsArray::get(int index) const { return FixedArray::get(index); }


void LiteralsArray::set(int index, Object* value) {
  FixedArray::set(index, value);
}


void LiteralsArray::set(int index, Smi* value) {
  FixedArray::set(index, value);
}


void LiteralsArray::set(int index, Object* value, WriteBarrierMode mode) {
  FixedArray::set(index, value, mode);
}


LiteralsArray* LiteralsArray::cast(Object* object) {
  SLOW_DCHECK(object->IsLiteralsArray());
  return reinterpret_cast<LiteralsArray*>(object);
}


TypeFeedbackVector* LiteralsArray::feedback_vector() const {
  return TypeFeedbackVector::cast(get(kVectorIndex));
}


void LiteralsArray::set_feedback_vector(TypeFeedbackVector* vector) {
  set(kVectorIndex, vector);
}


Object* LiteralsArray::literal(int literal_index) const {
  return get(kFirstLiteralIndex + literal_index);
}


void LiteralsArray::set_literal(int literal_index, Object* literal) {
  set(kFirstLiteralIndex + literal_index, literal);
}


int LiteralsArray::literals_count() const {
  return length() - kFirstLiteralIndex;
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


void HandlerTable::SetRangeDepth(int index, int value) {
  set(index * kRangeEntrySize + kRangeDepthIndex, Smi::FromInt(value));
}


void HandlerTable::SetReturnOffset(int index, int value) {
  set(index * kReturnEntrySize + kReturnOffsetIndex, Smi::FromInt(value));
}


void HandlerTable::SetReturnHandler(int index, int offset,
                                    CatchPrediction prediction) {
  int value = HandlerOffsetField::encode(offset) |
              HandlerPredictionField::encode(prediction);
  set(index * kReturnEntrySize + kReturnHandlerIndex, Smi::FromInt(value));
}


#define MAKE_STRUCT_CAST(NAME, Name, name) CAST_ACCESSOR(Name)
  STRUCT_LIST(MAKE_STRUCT_CAST)
#undef MAKE_STRUCT_CAST


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


Handle<Name> Name::Flatten(Handle<Name> name, PretenureFlag pretenure) {
  if (name->IsSymbol()) return name;
  return String::Flatten(Handle<String>::cast(name));
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


int ByteArray::get_int(int index) {
  DCHECK(index >= 0 && (index * kIntSize) < this->length());
  return READ_INT_FIELD(this, kHeaderSize + index * kIntSize);
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


int BytecodeArray::parameter_count() const {
  // Parameter count is stored as the size on stack of the parameters to allow
  // it to be used directly by generated code.
  return READ_INT_FIELD(this, kParameterSizeOffset) >> kPointerSizeLog2;
}


ACCESSORS(BytecodeArray, constant_pool, FixedArray, kConstantPoolOffset)


Address BytecodeArray::GetFirstBytecodeAddress() {
  return reinterpret_cast<Address>(this) - kHeapObjectTag + kHeaderSize;
}


int BytecodeArray::BytecodeArraySize() { return SizeFor(this->length()); }


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
  if (base_pointer() == Smi::FromInt(0)) return 0;
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
  DCHECK((index >= 0) && (index < this->length()));
  ElementType* ptr = reinterpret_cast<ElementType*>(DataPtr());
  ptr[index] = value;
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
  // Handle NaNs and less than zero values which clamp to zero.
  if (!(value > 0)) return 0;
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
void FixedTypedArray<Traits>::SetValue(uint32_t index, Object* value) {
  ElementType cast_value = Traits::defaultValue();
  if (value->IsSmi()) {
    int int_value = Smi::cast(value)->value();
    cast_value = from_int(int_value);
  } else if (value->IsHeapNumber()) {
    double double_value = HeapNumber::cast(value)->value();
    cast_value = from_double(double_value);
  } else {
    // Clamp undefined to the default value. All other types have been
    // converted to a number type further up in the call chain.
    DCHECK(value->IsUndefined());
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


void Map::set_is_constructor() {
  set_bit_field(bit_field() | (1 << kIsConstructor));
}


bool Map::is_constructor() const {
  return ((1 << kIsConstructor) & bit_field()) != 0;
}


void Map::set_is_hidden_prototype() {
  set_bit_field3(IsHiddenPrototype::update(bit_field3(), true));
}


bool Map::is_hidden_prototype() const {
  return IsHiddenPrototype::decode(bit_field3());
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


void Map::set_is_observed() { set_bit_field(bit_field() | (1 << kIsObserved)); }

bool Map::is_observed() { return ((1 << kIsObserved) & bit_field()) != 0; }


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
  return IsSloppyArgumentsElements(elements_kind());
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


void Map::set_is_strong() {
  set_bit_field3(IsStrong::update(bit_field3(), true));
}


bool Map::is_strong() {
  return IsStrong::decode(bit_field3());
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
    if (details.type() == DATA_CONSTANT) return true;
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
  return kind() == STUB || kind() == HANDLER || kind() == LOAD_IC ||
         kind() == KEYED_LOAD_IC || kind() == CALL_IC || kind() == STORE_IC ||
         kind() == KEYED_STORE_IC || kind() == BINARY_OP_IC ||
         kind() == COMPARE_IC || kind() == COMPARE_NIL_IC ||
         kind() == TO_BOOLEAN_IC;
}


bool Code::IsJavaScriptCode() {
  return kind() == FUNCTION || kind() == OPTIMIZED_FUNCTION ||
         is_interpreter_entry_trampoline();
}


InlineCacheState Code::ic_state() {
  InlineCacheState result = ExtractICStateFromFlags(flags());
  // Only allow uninitialized or debugger states for non-IC code
  // objects. This is used in the debugger to determine whether or not
  // a call to code object has been replaced with a debug break call.
  DCHECK(is_inline_cache_stub() ||
         result == UNINITIALIZED ||
         result == DEBUG_STUB);
  return result;
}


ExtraICState Code::extra_ic_state() {
  DCHECK(is_inline_cache_stub() || ic_state() == DEBUG_STUB);
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


inline bool Code::is_hydrogen_stub() {
  return is_crankshafted() && kind() != OPTIMIZED_FUNCTION;
}


inline bool Code::is_interpreter_entry_trampoline() {
  Handle<Code> interpreter_entry =
      GetIsolate()->builtins()->InterpreterEntryTrampoline();
  return interpreter_entry.location() != nullptr && *interpreter_entry == this;
}

inline void Code::set_is_crankshafted(bool value) {
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags2Offset);
  int updated = IsCrankshaftedField::update(previous, value);
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
  DCHECK(level >= 0 && level <= kMaxLoopNestingMarker);
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


int Code::builtin_index() {
  return READ_INT32_FIELD(this, kKindSpecificFlags1Offset);
}


void Code::set_builtin_index(int index) {
  WRITE_INT32_FIELD(this, kKindSpecificFlags1Offset, index);
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
  DCHECK(!flag || AllowDeoptimization::IsAllowed(GetIsolate()));
  int previous = READ_UINT32_FIELD(this, kKindSpecificFlags1Offset);
  int updated = MarkedForDeoptimizationField::update(previous, flag);
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


bool Code::is_debug_stub() { return ic_state() == DEBUG_STUB; }
bool Code::is_handler() { return kind() == HANDLER; }
bool Code::is_load_stub() { return kind() == LOAD_IC; }
bool Code::is_keyed_load_stub() { return kind() == KEYED_LOAD_IC; }
bool Code::is_store_stub() { return kind() == STORE_IC; }
bool Code::is_keyed_store_stub() { return kind() == KEYED_STORE_IC; }
bool Code::is_call_stub() { return kind() == CALL_IC; }
bool Code::is_binary_op_stub() { return kind() == BINARY_OP_IC; }
bool Code::is_compare_ic_stub() { return kind() == COMPARE_IC; }
bool Code::is_compare_nil_ic_stub() { return kind() == COMPARE_NIL_IC; }
bool Code::is_to_boolean_ic_stub() { return kind() == TO_BOOLEAN_IC; }
bool Code::is_optimized_code() { return kind() == OPTIMIZED_FUNCTION; }


bool Code::embeds_maps_weakly() {
  Kind k = kind();
  return (k == LOAD_IC || k == STORE_IC || k == KEYED_LOAD_IC ||
          k == KEYED_STORE_IC || k == COMPARE_NIL_IC) &&
         ic_state() == MONOMORPHIC;
}


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


Code::Flags Code::ComputeFlags(Kind kind, InlineCacheState ic_state,
                               ExtraICState extra_ic_state, StubType type,
                               CacheHolderFlag holder) {
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
                                          CacheHolderFlag holder,
                                          StubType type) {
  return ComputeFlags(kind, MONOMORPHIC, extra_ic_state, type, holder);
}


Code::Flags Code::ComputeHandlerFlags(Kind handler_kind, StubType type,
                                      CacheHolderFlag holder) {
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


CacheHolderFlag Code::ExtractCacheHolderFromFlags(Flags flags) {
  return CacheHolderField::decode(flags);
}


Code::Flags Code::RemoveTypeFromFlags(Flags flags) {
  int bits = flags & ~TypeField::kMask;
  return static_cast<Flags>(bits);
}


Code::Flags Code::RemoveTypeAndHolderFromFlags(Flags flags) {
  int bits = flags & ~TypeField::kMask & ~CacheHolderField::kMask;
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


bool Code::CanContainWeakObjects() {
  return is_optimized_code() && can_have_weak_objects();
}


bool Code::IsWeakObject(Object* object) {
  return (CanContainWeakObjects() && IsWeakObjectInOptimizedCode(object));
}


bool Code::IsWeakObjectInOptimizedCode(Object* object) {
  if (object->IsMap()) {
    return Map::cast(object)->CanTransition() &&
           FLAG_weak_embedded_maps_in_optimized_code;
  }
  if (object->IsCell()) {
    object = Cell::cast(object)->value();
  } else if (object->IsPropertyCell()) {
    object = PropertyCell::cast(object)->value();
  }
  if (object->IsJSReceiver()) {
    return FLAG_weak_embedded_objects_in_optimized_code;
  }
  if (object->IsContext()) {
    // Contexts of inlined functions are embedded in optimized code.
    return FLAG_weak_embedded_objects_in_optimized_code;
  }
  return false;
}


class Code::FindAndReplacePattern {
 public:
  FindAndReplacePattern() : count_(0) { }
  void Add(Handle<Map> map_to_find, Handle<Object> obj_to_replace) {
    DCHECK(count_ < kMaxCount);
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


Object* Map::prototype() const {
  return READ_FIELD(this, kPrototypeOffset);
}


void Map::set_prototype(Object* value, WriteBarrierMode mode) {
  DCHECK(value->IsNull() || value->IsJSReceiver());
  WRITE_FIELD(this, kPrototypeOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kPrototypeOffset, value, mode);
}


LayoutDescriptor* Map::layout_descriptor_gc_safe() {
  Object* layout_desc = READ_FIELD(this, kLayoutDecriptorOffset);
  return LayoutDescriptor::cast_gc_safe(layout_desc);
}


bool Map::HasFastPointerLayout() const {
  Object* layout_desc = READ_FIELD(this, kLayoutDecriptorOffset);
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
ACCESSORS(Map, layout_descriptor, LayoutDescriptor, kLayoutDecriptorOffset)


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
  CHECK(details.type() != DATA || !details.representation().IsDouble());
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
  DCHECK((value->IsMap() && GetBackPointer()->IsUndefined()));
  DCHECK(!value->IsMap() ||
         Map::cast(value)->GetConstructor() == constructor_or_backpointer());
  set_constructor_or_backpointer(value, mode);
}


ACCESSORS(Map, code_cache, Object, kCodeCacheOffset)
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


void Map::SetConstructor(Object* constructor, WriteBarrierMode mode) {
  // Never overwrite a back pointer with a constructor.
  DCHECK(!constructor_or_backpointer()->IsMap());
  set_constructor_or_backpointer(constructor, mode);
}


Handle<Map> Map::CopyInitialMap(Handle<Map> map) {
  return CopyInitialMap(map, map->instance_size(), map->GetInObjectProperties(),
                        map->unused_property_fields());
}


ACCESSORS(JSBoundFunction, length, Object, kLengthOffset)
ACCESSORS(JSBoundFunction, name, Object, kNameOffset)
ACCESSORS(JSBoundFunction, bound_target_function, JSReceiver,
          kBoundTargetFunctionOffset)
ACCESSORS(JSBoundFunction, bound_this, Object, kBoundThisOffset)
ACCESSORS(JSBoundFunction, bound_arguments, FixedArray, kBoundArgumentsOffset)
ACCESSORS(JSBoundFunction, creation_context, Context, kCreationContextOffset)

ACCESSORS(JSFunction, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
ACCESSORS(JSFunction, literals, LiteralsArray, kLiteralsOffset)
ACCESSORS(JSFunction, next_function_link, Object, kNextFunctionLinkOffset)

ACCESSORS(JSGlobalObject, native_context, Context, kNativeContextOffset)
ACCESSORS(JSGlobalObject, global_proxy, JSObject, kGlobalProxyOffset)

ACCESSORS(JSGlobalProxy, native_context, Object, kNativeContextOffset)
ACCESSORS(JSGlobalProxy, hash, Object, kHashOffset)

ACCESSORS(AccessorInfo, name, Object, kNameOffset)
SMI_ACCESSORS(AccessorInfo, flag, kFlagOffset)
ACCESSORS(AccessorInfo, expected_receiver_type, Object,
          kExpectedReceiverTypeOffset)

ACCESSORS(ExecutableAccessorInfo, getter, Object, kGetterOffset)
ACCESSORS(ExecutableAccessorInfo, setter, Object, kSetterOffset)
ACCESSORS(ExecutableAccessorInfo, data, Object, kDataOffset)

ACCESSORS(Box, value, Object, kValueOffset)

ACCESSORS(PrototypeInfo, prototype_users, Object, kPrototypeUsersOffset)
SMI_ACCESSORS(PrototypeInfo, registry_slot, kRegistrySlotOffset)
ACCESSORS(PrototypeInfo, validity_cell, Object, kValidityCellOffset)

ACCESSORS(SloppyBlockWithEvalContextExtension, scope_info, ScopeInfo,
          kScopeInfoOffset)
ACCESSORS(SloppyBlockWithEvalContextExtension, extension, JSObject,
          kExtensionOffset)

ACCESSORS(AccessorPair, getter, Object, kGetterOffset)
ACCESSORS(AccessorPair, setter, Object, kSetterOffset)

ACCESSORS(AccessCheckInfo, named_callback, Object, kNamedCallbackOffset)
ACCESSORS(AccessCheckInfo, indexed_callback, Object, kIndexedCallbackOffset)
ACCESSORS(AccessCheckInfo, callback, Object, kCallbackOffset)
ACCESSORS(AccessCheckInfo, data, Object, kDataOffset)

ACCESSORS(InterceptorInfo, getter, Object, kGetterOffset)
ACCESSORS(InterceptorInfo, setter, Object, kSetterOffset)
ACCESSORS(InterceptorInfo, query, Object, kQueryOffset)
ACCESSORS(InterceptorInfo, deleter, Object, kDeleterOffset)
ACCESSORS(InterceptorInfo, enumerator, Object, kEnumeratorOffset)
ACCESSORS(InterceptorInfo, data, Object, kDataOffset)
SMI_ACCESSORS(InterceptorInfo, flags, kFlagsOffset)
BOOL_ACCESSORS(InterceptorInfo, flags, can_intercept_symbols,
               kCanInterceptSymbolsBit)
BOOL_ACCESSORS(InterceptorInfo, flags, all_can_read, kAllCanReadBit)
BOOL_ACCESSORS(InterceptorInfo, flags, non_masking, kNonMasking)

ACCESSORS(CallHandlerInfo, callback, Object, kCallbackOffset)
ACCESSORS(CallHandlerInfo, data, Object, kDataOffset)
ACCESSORS(CallHandlerInfo, fast_handler, Object, kFastHandlerOffset)

ACCESSORS(TemplateInfo, tag, Object, kTagOffset)
SMI_ACCESSORS(TemplateInfo, number_of_properties, kNumberOfProperties)
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
SMI_ACCESSORS(FunctionTemplateInfo, flag, kFlagOffset)

ACCESSORS(ObjectTemplateInfo, constructor, Object, kConstructorOffset)
ACCESSORS(ObjectTemplateInfo, internal_field_count, Object,
          kInternalFieldCountOffset)

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
ACCESSORS(Script, eval_from_shared, Object, kEvalFromSharedOffset)
SMI_ACCESSORS(Script, eval_from_instructions_offset,
              kEvalFrominstructionsOffsetOffset)
ACCESSORS(Script, shared_function_infos, Object, kSharedFunctionInfosOffset)
SMI_ACCESSORS(Script, flags, kFlagsOffset)
ACCESSORS(Script, source_url, Object, kSourceUrlOffset)
ACCESSORS(Script, source_mapping_url, Object, kSourceMappingUrlOffset)

Script::CompilationType Script::compilation_type() {
  return BooleanBit::get(flags(), kCompilationTypeBit) ?
      COMPILATION_TYPE_EVAL : COMPILATION_TYPE_HOST;
}
void Script::set_compilation_type(CompilationType type) {
  set_flags(BooleanBit::set(flags(), kCompilationTypeBit,
      type == COMPILATION_TYPE_EVAL));
}
bool Script::hide_source() { return BooleanBit::get(flags(), kHideSourceBit); }
void Script::set_hide_source(bool value) {
  set_flags(BooleanBit::set(flags(), kHideSourceBit, value));
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
ACCESSORS(DebugInfo, code, Code, kCodeIndex)
ACCESSORS(DebugInfo, break_points, FixedArray, kBreakPointsStateIndex)

SMI_ACCESSORS(BreakPointInfo, code_position, kCodePositionIndex)
SMI_ACCESSORS(BreakPointInfo, source_position, kSourcePositionIndex)
SMI_ACCESSORS(BreakPointInfo, statement_position, kStatementPositionIndex)
ACCESSORS(BreakPointInfo, break_point_objects, Object, kBreakPointObjectsIndex)

ACCESSORS(SharedFunctionInfo, name, Object, kNameOffset)
ACCESSORS(SharedFunctionInfo, optimized_code_map, FixedArray,
          kOptimizedCodeMapOffset)
ACCESSORS(SharedFunctionInfo, construct_stub, Code, kConstructStubOffset)
ACCESSORS(SharedFunctionInfo, feedback_vector, TypeFeedbackVector,
          kFeedbackVectorOffset)
#if TRACE_MAPS
SMI_ACCESSORS(SharedFunctionInfo, unique_id, kUniqueIdOffset)
#endif
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
BOOL_ACCESSORS(FunctionTemplateInfo, flag, instantiated, kInstantiatedBit)
BOOL_ACCESSORS(FunctionTemplateInfo, flag, accept_any_receiver,
               kAcceptAnyReceiver)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_expression,
               kIsExpressionBit)
BOOL_ACCESSORS(SharedFunctionInfo, start_position_and_type, is_toplevel,
               kIsTopLevelBit)

BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, allows_lazy_compilation,
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
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, asm_function, kIsAsmFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, deserialized, kDeserialized)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, never_compiled,
               kNeverCompiled)


#if V8_HOST_ARCH_32_BIT
SMI_ACCESSORS(SharedFunctionInfo, length, kLengthOffset)
SMI_ACCESSORS(SharedFunctionInfo, internal_formal_parameter_count,
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


BOOL_GETTER(SharedFunctionInfo,
            compiler_hints,
            optimization_disabled,
            kOptimizationDisabled)


void SharedFunctionInfo::set_optimization_disabled(bool disable) {
  set_compiler_hints(BooleanBit::set(compiler_hints(),
                                     kOptimizationDisabled,
                                     disable));
}


LanguageMode SharedFunctionInfo::language_mode() {
  STATIC_ASSERT(LANGUAGE_END == 3);
  return construct_language_mode(
      BooleanBit::get(compiler_hints(), kStrictModeFunction),
      BooleanBit::get(compiler_hints(), kStrongModeFunction));
}


void SharedFunctionInfo::set_language_mode(LanguageMode language_mode) {
  STATIC_ASSERT(LANGUAGE_END == 3);
  // We only allow language mode transitions that set the same language mode
  // again or go up in the chain:
  DCHECK(is_sloppy(this->language_mode()) || is_strict(language_mode));
  int hints = compiler_hints();
  hints = BooleanBit::set(hints, kStrictModeFunction, is_strict(language_mode));
  hints = BooleanBit::set(hints, kStrongModeFunction, is_strong(language_mode));
  set_compiler_hints(hints);
}


FunctionKind SharedFunctionInfo::kind() {
  return FunctionKindBits::decode(compiler_hints());
}


void SharedFunctionInfo::set_kind(FunctionKind kind) {
  DCHECK(IsValidFunctionKind(kind));
  int hints = compiler_hints();
  hints = FunctionKindBits::update(hints, kind);
  set_compiler_hints(hints);
}


BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, needs_home_object,
               kNeedsHomeObject)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, native, kNative)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, force_inline, kForceInline)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints,
               name_should_print_as_anonymous,
               kNameShouldPrintAsAnonymous)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_anonymous, kIsAnonymous)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_function, kIsFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_crankshaft,
               kDontCrankshaft)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, dont_flush, kDontFlush)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_arrow, kIsArrow)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_generator, kIsGenerator)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_concise_method,
               kIsConciseMethod)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_accessor_function,
               kIsAccessorFunction)
BOOL_ACCESSORS(SharedFunctionInfo, compiler_hints, is_default_constructor,
               kIsDefaultConstructor)

ACCESSORS(CodeCache, default_cache, FixedArray, kDefaultCacheOffset)
ACCESSORS(CodeCache, normal_type_cache, Object, kNormalTypeCacheOffset)

ACCESSORS(PolymorphicCodeCache, cache, Object, kCacheOffset)

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
  DCHECK(code()->kind() == Code::BUILTIN);
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

  if (is_compiled()) set_never_compiled(false);
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


bool SharedFunctionInfo::is_compiled() {
  Builtins* builtins = GetIsolate()->builtins();
  DCHECK(code() != builtins->builtin(Builtins::kCompileOptimizedConcurrent));
  DCHECK(code() != builtins->builtin(Builtins::kCompileOptimized));
  return code() != builtins->builtin(Builtins::kCompileLazy);
}


bool SharedFunctionInfo::has_simple_parameters() {
  return scope_info()->HasSimpleParameters();
}


bool SharedFunctionInfo::HasDebugInfo() {
  bool has_debug_info = debug_info()->IsStruct();
  DCHECK(!has_debug_info || HasDebugCode());
  return has_debug_info;
}


DebugInfo* SharedFunctionInfo::GetDebugInfo() {
  DCHECK(HasDebugInfo());
  return DebugInfo::cast(debug_info());
}


bool SharedFunctionInfo::HasDebugCode() {
  return code()->kind() == Code::FUNCTION && code()->has_debug_break_slots();
}


bool SharedFunctionInfo::IsApiFunction() {
  return function_data()->IsFunctionTemplateInfo();
}


FunctionTemplateInfo* SharedFunctionInfo::get_api_func_data() {
  DCHECK(IsApiFunction());
  return FunctionTemplateInfo::cast(function_data());
}


bool SharedFunctionInfo::HasBuiltinFunctionId() {
  return function_data()->IsSmi();
}


BuiltinFunctionId SharedFunctionInfo::builtin_function_id() {
  DCHECK(HasBuiltinFunctionId());
  return static_cast<BuiltinFunctionId>(Smi::cast(function_data())->value());
}


bool SharedFunctionInfo::HasBytecodeArray() {
  return function_data()->IsBytecodeArray();
}


BytecodeArray* SharedFunctionInfo::bytecode_array() {
  DCHECK(HasBytecodeArray());
  return BytecodeArray::cast(function_data());
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
    set_opt_count(0);
    set_deopt_count(0);
  }
}


void SharedFunctionInfo::set_disable_optimization_reason(BailoutReason reason) {
  set_opt_count_and_bailout_reason(DisabledOptimizationReasonBits::update(
      opt_count_and_bailout_reason(), reason));
}


bool SharedFunctionInfo::IsBuiltin() {
  Object* script_obj = script();
  if (script_obj->IsUndefined()) return true;
  Script* script = Script::cast(script_obj);
  Script::Type type = static_cast<Script::Type>(script->type());
  return type != Script::TYPE_NORMAL;
}


bool SharedFunctionInfo::IsSubjectToDebugging() { return !IsBuiltin(); }


bool SharedFunctionInfo::OptimizedCodeMapIsCleared() const {
  return optimized_code_map() == GetHeap()->cleared_optimized_code_map();
}


// static
void SharedFunctionInfo::AddToOptimizedCodeMap(
    Handle<SharedFunctionInfo> shared, Handle<Context> native_context,
    Handle<Code> code, Handle<LiteralsArray> literals, BailoutId osr_ast_id) {
  AddToOptimizedCodeMapInternal(shared, native_context, code, literals,
                                osr_ast_id);
}


// static
void SharedFunctionInfo::AddLiteralsToOptimizedCodeMap(
    Handle<SharedFunctionInfo> shared, Handle<Context> native_context,
    Handle<LiteralsArray> literals) {
  Isolate* isolate = shared->GetIsolate();
  Handle<Oddball> undefined = isolate->factory()->undefined_value();
  AddToOptimizedCodeMapInternal(shared, native_context, undefined, literals,
                                BailoutId::None());
}


bool JSFunction::IsOptimized() {
  return code()->kind() == Code::OPTIMIZED_FUNCTION;
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


JSObject* JSFunction::global_proxy() {
  return context()->global_proxy();
}


Context* JSFunction::native_context() { return context()->native_context(); }


void JSFunction::set_context(Object* value) {
  DCHECK(value->IsUndefined() || value->IsContext());
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
  return has_initial_map() || !prototype_or_initial_map()->IsTheHole();
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


int JSFunction::NumberOfLiterals() {
  return literals()->length();
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
SMI_ACCESSORS(JSGeneratorObject, continuation, kContinuationOffset)
ACCESSORS(JSGeneratorObject, operand_stack, FixedArray, kOperandStackOffset)

bool JSGeneratorObject::is_suspended() {
  DCHECK_LT(kGeneratorExecuting, kGeneratorClosed);
  DCHECK_EQ(kGeneratorClosed, 0);
  return continuation() > 0;
}

bool JSGeneratorObject::is_closed() {
  return continuation() == kGeneratorClosed;
}

bool JSGeneratorObject::is_executing() {
  return continuation() == kGeneratorExecuting;
}

ACCESSORS(JSModule, context, Object, kContextOffset)
ACCESSORS(JSModule, scope_info, ScopeInfo, kScopeInfoOffset)


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


INT_ACCESSORS(Code, instruction_size, kInstructionSizeOffset)
INT_ACCESSORS(Code, prologue_offset, kPrologueOffset)
INT_ACCESSORS(Code, constant_pool_offset, kConstantPoolOffset)
ACCESSORS(Code, relocation_info, ByteArray, kRelocationInfoOffset)
ACCESSORS(Code, handler_table, FixedArray, kHandlerTableOffset)
ACCESSORS(Code, deoptimization_data, FixedArray, kDeoptimizationDataOffset)
ACCESSORS(Code, raw_type_feedback_info, Object, kTypeFeedbackInfoOffset)
ACCESSORS(Code, next_code_link, Object, kNextCodeLinkOffset)


void Code::WipeOutHeader() {
  WRITE_FIELD(this, kRelocationInfoOffset, NULL);
  WRITE_FIELD(this, kHandlerTableOffset, NULL);
  WRITE_FIELD(this, kDeoptimizationDataOffset, NULL);
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


Object* JSArrayBufferView::byte_offset() const {
  if (WasNeutered()) return Smi::FromInt(0);
  return Object::cast(READ_FIELD(this, kByteOffsetOffset));
}


void JSArrayBufferView::set_byte_offset(Object* value, WriteBarrierMode mode) {
  WRITE_FIELD(this, kByteOffsetOffset, value);
  CONDITIONAL_WRITE_BARRIER(GetHeap(), this, kByteOffsetOffset, value, mode);
}


Object* JSArrayBufferView::byte_length() const {
  if (WasNeutered()) return Smi::FromInt(0);
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
  if (WasNeutered()) return Smi::FromInt(0);
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


#ifdef VERIFY_HEAP
ACCESSORS(JSTypedArray, raw_length, Object, kLengthOffset)
#endif


ACCESSORS(JSRegExp, data, Object, kDataOffset)
ACCESSORS(JSRegExp, flags, Object, kFlagsOffset)
ACCESSORS(JSRegExp, source, Object, kSourceOffset)


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


Object* JSRegExp::DataAt(int index) {
  DCHECK(TypeTag() != NOT_COMPILED);
  return FixedArray::cast(data())->get(index);
}


void JSRegExp::SetDataAt(int index, Object* value) {
  DCHECK(TypeTag() != NOT_COMPILED);
  DCHECK(index >= kDataIndex);  // Only implementation data can be set this way.
  FixedArray::cast(data())->set(index, value);
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
    DCHECK((IsFastSmiOrObjectElementsKind(kind) &&
            (map == GetHeap()->fixed_array_map() ||
             map == GetHeap()->fixed_cow_array_map())) ||
           (IsFastDoubleElementsKind(kind) &&
            (fixed_array->IsFixedDoubleArray() ||
             fixed_array == GetHeap()->empty_fixed_array())) ||
           (kind == DICTIONARY_ELEMENTS &&
            fixed_array->IsFixedArray() &&
            fixed_array->IsDictionary()) ||
           (kind > DICTIONARY_ELEMENTS));
    DCHECK(!IsSloppyArgumentsElements(kind) ||
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
  return IsSloppyArgumentsElements(GetElementsKind());
}


bool JSObject::HasFixedTypedArrayElements() {
  HeapObject* array = elements();
  DCHECK(array != NULL);
  return array->IsFixedTypedArrayBase();
}


#define FIXED_TYPED_ELEMENTS_CHECK(Type, type, TYPE, ctype, size)         \
bool JSObject::HasFixed##Type##Elements() {                               \
  HeapObject* array = elements();                                         \
  DCHECK(array != NULL);                                                  \
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


GlobalDictionary* JSObject::global_dictionary() {
  DCHECK(!HasFastProperties());
  DCHECK(IsJSGlobalObject());
  return GlobalDictionary::cast(properties());
}


SeededNumberDictionary* JSObject::element_dictionary() {
  DCHECK(HasDictionaryElements());
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


StringHasher::StringHasher(int length, uint32_t seed)
  : length_(length),
    raw_running_hash_(seed),
    array_index_(0),
    is_array_index_(0 < length_ && length_ <= String::kMaxArrayIndexSize),
    is_first_char_(true) {
  DCHECK(FLAG_randomize_hashes || raw_running_hash_ == 0);
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


uint32_t StringHasher::ComputeRunningHash(uint32_t running_hash,
                                          const uc16* chars, int length) {
  DCHECK_NOT_NULL(chars);
  DCHECK(length >= 0);
  for (int i = 0; i < length; ++i) {
    running_hash = AddCharacterCore(running_hash, *chars++);
  }
  return running_hash;
}


uint32_t StringHasher::ComputeRunningHashOneByte(uint32_t running_hash,
                                                 const char* chars,
                                                 int length) {
  DCHECK_NOT_NULL(chars);
  DCHECK(length >= 0);
  for (int i = 0; i < length; ++i) {
    uint16_t c = static_cast<uint16_t>(*chars++);
    running_hash = AddCharacterCore(running_hash, c);
  }
  return running_hash;
}


void StringHasher::AddCharacter(uint16_t c) {
  // Use the Jenkins one-at-a-time hash function to update the hash
  // for the given character.
  raw_running_hash_ = AddCharacterCore(raw_running_hash_, c);
}


bool StringHasher::UpdateIndex(uint16_t c) {
  DCHECK(is_array_index_);
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
  if (array_index_ > 429496729U - ((d + 3) >> 3)) {
    is_array_index_ = false;
    return false;
  }
  array_index_ = array_index_ * 10 + d;
  return true;
}


template<typename Char>
inline void StringHasher::AddCharacters(const Char* chars, int length) {
  DCHECK(sizeof(Char) == 1 || sizeof(Char) == 2);
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
    DCHECK(!is_array_index_);
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


IteratingStringHasher::IteratingStringHasher(int len, uint32_t seed)
    : StringHasher(len, seed) {}


uint32_t IteratingStringHasher::Hash(String* string, uint32_t seed) {
  IteratingStringHasher hasher(string->length(), seed);
  // Nothing to do.
  if (hasher.has_trivial_hash()) return hasher.GetHashField();
  ConsString* cons_string = String::VisitFlat(&hasher, string);
  if (cons_string == nullptr) return hasher.GetHashField();
  hasher.VisitConsString(cons_string);
  return hasher.GetHashField();
}


void IteratingStringHasher::VisitOneByteString(const uint8_t* chars,
                                               int length) {
  AddCharacters(chars, length);
}


void IteratingStringHasher::VisitTwoByteString(const uint16_t* chars,
                                               int length) {
  AddCharacters(chars, length);
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
Maybe<bool> Object::GreaterThan(Handle<Object> x, Handle<Object> y,
                                Strength strength) {
  Maybe<ComparisonResult> result = Compare(x, y, strength);
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
Maybe<bool> Object::GreaterThanOrEqual(Handle<Object> x, Handle<Object> y,
                                       Strength strength) {
  Maybe<ComparisonResult> result = Compare(x, y, strength);
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
Maybe<bool> Object::LessThan(Handle<Object> x, Handle<Object> y,
                             Strength strength) {
  Maybe<ComparisonResult> result = Compare(x, y, strength);
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
Maybe<bool> Object::LessThanOrEqual(Handle<Object> x, Handle<Object> y,
                                    Strength strength) {
  Maybe<ComparisonResult> result = Compare(x, y, strength);
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
                                                 Handle<Name> name,
                                                 LanguageMode language_mode) {
  LookupIterator it =
      LookupIterator::PropertyOrElement(name->GetIsolate(), object, name);
  return GetProperty(&it, language_mode);
}


MaybeHandle<Object> Object::GetPropertyOrElement(Handle<JSReceiver> holder,
                                                 Handle<Name> name,
                                                 Handle<Object> receiver,
                                                 LanguageMode language_mode) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), receiver, name, holder);
  return GetProperty(&it, language_mode);
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
  DCHECK(properties()->IsDictionary() == map()->is_dictionary_map());
  return !properties()->IsDictionary();
}


NameDictionary* JSReceiver::property_dictionary() {
  DCHECK(!HasFastProperties());
  DCHECK(!IsJSGlobalObject());
  return NameDictionary::cast(properties());
}


Maybe<bool> JSReceiver::HasProperty(Handle<JSReceiver> object,
                                    Handle<Name> name) {
  LookupIterator it =
      LookupIterator::PropertyOrElement(object->GetIsolate(), object, name);
  return HasProperty(&it);
}


Maybe<bool> JSReceiver::HasOwnProperty(Handle<JSReceiver> object,
                                       Handle<Name> name) {
  if (object->IsJSObject()) {  // Shortcut
    LookupIterator it = LookupIterator::PropertyOrElement(
        object->GetIsolate(), object, name, LookupIterator::HIDDEN);
    return HasProperty(&it);
  }

  Maybe<PropertyAttributes> attributes =
      JSReceiver::GetOwnPropertyAttributes(object, name);
  MAYBE_RETURN(attributes, Nothing<bool>());
  return Just(attributes.FromJust() != ABSENT);
}


Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  LookupIterator it =
      LookupIterator::PropertyOrElement(name->GetIsolate(), object, name);
  return GetPropertyAttributes(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetOwnPropertyAttributes(
    Handle<JSReceiver> object, Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), object, name, LookupIterator::HIDDEN);
  return GetPropertyAttributes(&it);
}


Maybe<bool> JSReceiver::HasElement(Handle<JSReceiver> object, uint32_t index) {
  LookupIterator it(object->GetIsolate(), object, index);
  return HasProperty(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index);
  return GetPropertyAttributes(&it);
}


Maybe<PropertyAttributes> JSReceiver::GetOwnElementAttributes(
    Handle<JSReceiver> object, uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, LookupIterator::HIDDEN);
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


Handle<Smi> JSReceiver::GetOrCreateIdentityHash(Handle<JSReceiver> object) {
  return object->IsJSProxy()
      ? JSProxy::GetOrCreateIdentityHash(Handle<JSProxy>::cast(object))
      : JSObject::GetOrCreateIdentityHash(Handle<JSObject>::cast(object));
}


Object* JSReceiver::GetIdentityHash() {
  return IsJSProxy()
      ? JSProxy::cast(this)->GetIdentityHash()
      : JSObject::cast(this)->GetIdentityHash();
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


PropertyAttributes AccessorInfo::property_attributes() {
  return AttributesField::decode(static_cast<uint32_t>(flag()));
}


void AccessorInfo::set_property_attributes(PropertyAttributes attributes) {
  set_flag(AttributesField::update(flag(), attributes));
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
  if (!getter->IsNull()) set_getter(getter);
  if (!setter->IsNull()) set_setter(setter);
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
  return obj->IsCallable() || obj->IsUndefined();
}


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::SetEntry(int entry,
                                               Handle<Object> key,
                                               Handle<Object> value) {
  this->SetEntry(entry, key, value, PropertyDetails(Smi::FromInt(0)));
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
  STATIC_ASSERT(Dictionary::kEntrySize == 3);
  DCHECK(!key->IsName() || details.dictionary_index() > 0);
  int index = dict->EntryToIndex(entry);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = dict->GetWriteBarrierMode(no_gc);
  dict->set(index, *key, mode);
  dict->set(index + 1, *value, mode);
  dict->set(index + 2, details.AsSmi());
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
  dict->set(index, *key, mode);
  dict->set(index + 1, *value, mode);
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
  DCHECK(key->IsUniqueName());
  return key;
}


Handle<FixedArray> NameDictionary::DoGenerateNewEnumerationIndices(
    Handle<NameDictionary> dictionary) {
  return DerivedDictionary::GenerateNewEnumerationIndices(dictionary);
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
  return PropertyCell::cast(dict->ValueAt(entry))->value()->IsTheHole();
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


bool ScopeInfo::IsAsmModule() { return AsmModuleField::decode(Flags()); }


bool ScopeInfo::IsAsmFunction() { return AsmFunctionField::decode(Flags()); }


bool ScopeInfo::HasSimpleParameters() {
  return HasSimpleParametersField::decode(Flags());
}


#define SCOPE_INFO_FIELD_ACCESSORS(name)                                      \
  void ScopeInfo::Set##name(int value) { set(k##name, Smi::FromInt(value)); } \
  int ScopeInfo::name() {                                                     \
    if (length() > 0) {                                                       \
      return Smi::cast(get(k##name))->value();                                \
    } else {                                                                  \
      return 0;                                                               \
    }                                                                         \
  }
FOR_EACH_SCOPE_INFO_NUMERIC_FIELD(SCOPE_INFO_FIELD_ACCESSORS)
#undef SCOPE_INFO_FIELD_ACCESSORS


void Map::ClearCodeCache(Heap* heap) {
  // No write barrier is needed since empty_fixed_array is not in new space.
  // Please note this function is used during marking:
  //  - MarkCompactCollector::MarkUnmarkedObject
  //  - IncrementalMarking::Step
  DCHECK(!heap->InNewSpace(heap->empty_fixed_array()));
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
  // If the new array won't fit in a some non-trivial fraction of the max old
  // space size, then force it to go dictionary mode.
  uint32_t max_fast_array_size =
      static_cast<uint32_t>((heap->MaxOldGenerationSize() / kDoubleSize) / 4);
  return new_length >= max_fast_array_size;
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
  WRITE_FIELD(this, kStorage1Offset, Smi::FromInt(0));
  WRITE_FIELD(this, kStorage2Offset, Smi::FromInt(0));
  WRITE_FIELD(this, kStorage3Offset, Smi::FromInt(0));
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
  DCHECK(!key->IsTheHole());
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
  DCHECK(!value->IsTheHole());
  return value;
}


ACCESSORS(JSIteratorResult, done, Object, kDoneOffset)
ACCESSORS(JSIteratorResult, value, Object, kValueOffset)


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


#undef TYPE_CHECKER
#undef CAST_ACCESSOR
#undef INT_ACCESSORS
#undef ACCESSORS
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

#endif  // V8_OBJECTS_INL_H_
