// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_INL_H_
#define V8_OBJECTS_STRING_INL_H_

#include "src/common/external-pointer-inl.h"
#include "src/common/external-pointer.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/numbers/conversions-inl.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/string-table-inl.h"
#include "src/objects/string.h"
#include "src/strings/string-hasher-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

int String::synchronized_length() const {
  return base::AsAtomic32::Acquire_Load(
      reinterpret_cast<const int32_t*>(FIELD_ADDR(*this, kLengthOffset)));
}

void String::synchronized_set_length(int value) {
  base::AsAtomic32::Release_Store(
      reinterpret_cast<int32_t*>(FIELD_ADDR(*this, kLengthOffset)), value);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(String)
TQ_OBJECT_CONSTRUCTORS_IMPL(SeqString)
TQ_OBJECT_CONSTRUCTORS_IMPL(SeqOneByteString)
TQ_OBJECT_CONSTRUCTORS_IMPL(SeqTwoByteString)
TQ_OBJECT_CONSTRUCTORS_IMPL(InternalizedString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ConsString)
TQ_OBJECT_CONSTRUCTORS_IMPL(ThinString)
TQ_OBJECT_CONSTRUCTORS_IMPL(SlicedString)
OBJECT_CONSTRUCTORS_IMPL(ExternalString, String)
OBJECT_CONSTRUCTORS_IMPL(ExternalOneByteString, ExternalString)
OBJECT_CONSTRUCTORS_IMPL(ExternalTwoByteString, ExternalString)

CAST_ACCESSOR(ExternalOneByteString)
CAST_ACCESSOR(ExternalString)
CAST_ACCESSOR(ExternalTwoByteString)

StringShape::StringShape(const String str) : type_(str.map().instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(Map map) : type_(map.instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(InstanceType t) : type_(static_cast<uint32_t>(t)) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

bool StringShape::IsInternalized() {
  DCHECK(valid());
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return (type_ & (kIsNotStringMask | kIsNotInternalizedMask)) ==
         (kStringTag | kInternalizedTag);
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

template <typename TDispatcher, typename TResult, typename... TArgs>
inline TResult StringShape::DispatchToSpecificTypeWithoutCast(TArgs&&... args) {
  switch (full_representation_tag()) {
    case kSeqStringTag | kOneByteStringTag:
      return TDispatcher::HandleSeqOneByteString(std::forward<TArgs>(args)...);
    case kSeqStringTag | kTwoByteStringTag:
      return TDispatcher::HandleSeqTwoByteString(std::forward<TArgs>(args)...);
    case kConsStringTag | kOneByteStringTag:
    case kConsStringTag | kTwoByteStringTag:
      return TDispatcher::HandleConsString(std::forward<TArgs>(args)...);
    case kExternalStringTag | kOneByteStringTag:
      return TDispatcher::HandleExternalOneByteString(
          std::forward<TArgs>(args)...);
    case kExternalStringTag | kTwoByteStringTag:
      return TDispatcher::HandleExternalTwoByteString(
          std::forward<TArgs>(args)...);
    case kSlicedStringTag | kOneByteStringTag:
    case kSlicedStringTag | kTwoByteStringTag:
      return TDispatcher::HandleSlicedString(std::forward<TArgs>(args)...);
    case kThinStringTag | kOneByteStringTag:
    case kThinStringTag | kTwoByteStringTag:
      return TDispatcher::HandleThinString(std::forward<TArgs>(args)...);
    default:
      return TDispatcher::HandleInvalidString(std::forward<TArgs>(args)...);
  }
}

// All concrete subclasses of String (leaves of the inheritance tree).
#define STRING_CLASS_TYPES(V) \
  V(SeqOneByteString)         \
  V(SeqTwoByteString)         \
  V(ConsString)               \
  V(ExternalOneByteString)    \
  V(ExternalTwoByteString)    \
  V(SlicedString)             \
  V(ThinString)

template <typename TDispatcher, typename TResult, typename... TArgs>
inline TResult StringShape::DispatchToSpecificType(String str,
                                                   TArgs&&... args) {
  class CastingDispatcher : public AllStatic {
   public:
#define DEFINE_METHOD(Type)                                         \
  static inline TResult Handle##Type(String str, TArgs&&... args) { \
    return TDispatcher::Handle##Type(Type::cast(str),               \
                                     std::forward<TArgs>(args)...); \
  }
    STRING_CLASS_TYPES(DEFINE_METHOD)
#undef DEFINE_METHOD
    static inline TResult HandleInvalidString(String str, TArgs&&... args) {
      return TDispatcher::HandleInvalidString(str,
                                              std::forward<TArgs>(args)...);
    }
  };

  return DispatchToSpecificTypeWithoutCast<CastingDispatcher, TResult>(
      str, std::forward<TArgs>(args)...);
}

DEF_GETTER(String, IsOneByteRepresentation, bool) {
  uint32_t type = map(isolate).instance_type();
  return (type & kStringEncodingMask) == kOneByteStringTag;
}

DEF_GETTER(String, IsTwoByteRepresentation, bool) {
  uint32_t type = map(isolate).instance_type();
  return (type & kStringEncodingMask) == kTwoByteStringTag;
}

// static
bool String::IsOneByteRepresentationUnderneath(String string) {
  while (true) {
    uint32_t type = string.map().instance_type();
    STATIC_ASSERT(kIsIndirectStringTag != 0);
    STATIC_ASSERT((kIsIndirectStringMask & kStringEncodingMask) == 0);
    DCHECK(string.IsFlat());
    switch (type & (kIsIndirectStringMask | kStringEncodingMask)) {
      case kOneByteStringTag:
        return true;
      case kTwoByteStringTag:
        return false;
      default:  // Cons, sliced, thin, strings need to go deeper.
        string = string.GetUnderlying();
    }
  }
}

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

template <typename Char>
class SequentialStringKey final : public StringTableKey {
 public:
  SequentialStringKey(const Vector<const Char>& chars, uint64_t seed,
                      bool convert = false)
      : SequentialStringKey(StringHasher::HashSequentialString<Char>(
                                chars.begin(), chars.length(), seed),
                            chars, convert) {}

  SequentialStringKey(int hash, const Vector<const Char>& chars,
                      bool convert = false)
      : StringTableKey(hash, chars.length()),
        chars_(chars),
        convert_(convert) {}

  bool IsMatch(String s) override {
    DisallowHeapAllocation no_gc;
    if (s.IsOneByteRepresentation()) {
      const uint8_t* chars = s.GetChars<uint8_t>(no_gc);
      return CompareChars(chars, chars_.begin(), chars_.length()) == 0;
    }
    const uint16_t* chars = s.GetChars<uint16_t>(no_gc);
    return CompareChars(chars, chars_.begin(), chars_.length()) == 0;
  }

  Handle<String> AsHandle(Isolate* isolate) override {
    if (sizeof(Char) == 1) {
      return isolate->factory()->NewOneByteInternalizedString(
          Vector<const uint8_t>::cast(chars_), hash_field());
    }
    return isolate->factory()->NewTwoByteInternalizedString(
        Vector<const uint16_t>::cast(chars_), hash_field());
  }

 private:
  Vector<const Char> chars_;
  bool convert_;
};

using OneByteStringKey = SequentialStringKey<uint8_t>;
using TwoByteStringKey = SequentialStringKey<uint16_t>;

template <typename SeqString>
class SeqSubStringKey final : public StringTableKey {
 public:
  using Char = typename SeqString::Char;
// VS 2017 on official builds gives this spurious warning:
// warning C4789: buffer 'key' of size 16 bytes will be overrun; 4 bytes will
// be written starting at offset 16
// https://bugs.chromium.org/p/v8/issues/detail?id=6068
#if defined(V8_CC_MSVC)
#pragma warning(push)
#pragma warning(disable : 4789)
#endif
  SeqSubStringKey(Isolate* isolate, Handle<SeqString> string, int from, int len,
                  bool convert = false)
      : StringTableKey(0, len),
        string_(string),
        from_(from),
        convert_(convert) {
    // We have to set the hash later.
    DisallowHeapAllocation no_gc;
    uint32_t hash = StringHasher::HashSequentialString(
        string->GetChars(no_gc) + from, len, HashSeed(isolate));
    set_hash_field(hash);

    DCHECK_LE(0, length());
    DCHECK_LE(from_ + length(), string_->length());
    DCHECK_EQ(string_->IsSeqOneByteString(), sizeof(Char) == 1);
    DCHECK_EQ(string_->IsSeqTwoByteString(), sizeof(Char) == 2);
  }
#if defined(V8_CC_MSVC)
#pragma warning(pop)
#endif

  bool IsMatch(String string) override {
    DisallowHeapAllocation no_gc;
    if (string.IsOneByteRepresentation()) {
      const uint8_t* data = string.GetChars<uint8_t>(no_gc);
      return CompareChars(string_->GetChars(no_gc) + from_, data, length()) ==
             0;
    }
    const uint16_t* data = string.GetChars<uint16_t>(no_gc);
    return CompareChars(string_->GetChars(no_gc) + from_, data, length()) == 0;
  }

  Handle<String> AsHandle(Isolate* isolate) override {
    if (sizeof(Char) == 1 || (sizeof(Char) == 2 && convert_)) {
      Handle<SeqOneByteString> result =
          isolate->factory()->AllocateRawOneByteInternalizedString(
              length(), hash_field());
      DisallowHeapAllocation no_gc;
      CopyChars(result->GetChars(no_gc), string_->GetChars(no_gc) + from_,
                length());
      return result;
    }
    Handle<SeqTwoByteString> result =
        isolate->factory()->AllocateRawTwoByteInternalizedString(length(),
                                                                 hash_field());
    DisallowHeapAllocation no_gc;
    CopyChars(result->GetChars(no_gc), string_->GetChars(no_gc) + from_,
              length());
    return result;
  }

 private:
  Handle<typename CharTraits<Char>::String> string_;
  int from_;
  bool convert_;
};

using SeqOneByteSubStringKey = SeqSubStringKey<SeqOneByteString>;
using SeqTwoByteSubStringKey = SeqSubStringKey<SeqTwoByteString>;

bool String::Equals(String other) {
  if (other == *this) return true;
  if (this->IsInternalizedString() && other.IsInternalizedString()) {
    return false;
  }
  return SlowEquals(other);
}

bool String::Equals(Isolate* isolate, Handle<String> one, Handle<String> two) {
  if (one.is_identical_to(two)) return true;
  if (one->IsInternalizedString() && two->IsInternalizedString()) {
    return false;
  }
  return SlowEquals(isolate, one, two);
}

template <typename Char>
const Char* String::GetChars(const DisallowHeapAllocation& no_gc) {
  return StringShape(*this).IsExternal()
             ? CharTraits<Char>::ExternalString::cast(*this).GetChars()
             : CharTraits<Char>::String::cast(*this).GetChars(no_gc);
}

Handle<String> String::Flatten(Isolate* isolate, Handle<String> string,
                               AllocationType allocation) {
  if (string->IsConsString()) {
    Handle<ConsString> cons = Handle<ConsString>::cast(string);
    if (cons->IsFlat()) {
      string = handle(cons->first(), isolate);
    } else {
      return SlowFlatten(isolate, cons, allocation);
    }
  }
  if (string->IsThinString()) {
    string = handle(Handle<ThinString>::cast(string)->actual(), isolate);
    DCHECK(!string->IsConsString());
  }
  return string;
}

Handle<String> String::Flatten(OffThreadIsolate* isolate, Handle<String> string,
                               AllocationType allocation) {
  // We should never pass non-flat strings to String::Flatten when off-thread.
  DCHECK(string->IsFlat());
  return string;
}

uint16_t String::Get(int index) {
  DCHECK(index >= 0 && index < length());

  class StringGetDispatcher : public AllStatic {
   public:
#define DEFINE_METHOD(Type)                                  \
  static inline uint16_t Handle##Type(Type str, int index) { \
    return str.Get(index);                                   \
  }
    STRING_CLASS_TYPES(DEFINE_METHOD)
#undef DEFINE_METHOD
    static inline uint16_t HandleInvalidString(String str, int index) {
      UNREACHABLE();
    }
  };

  return StringShape(*this)
      .DispatchToSpecificType<StringGetDispatcher, uint16_t>(*this, index);
}

void String::Set(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  DCHECK(StringShape(*this).IsSequential());

  return this->IsOneByteRepresentation()
             ? SeqOneByteString::cast(*this).SeqOneByteStringSet(index, value)
             : SeqTwoByteString::cast(*this).SeqTwoByteStringSet(index, value);
}

bool String::IsFlat() {
  if (!StringShape(*this).IsCons()) return true;
  return ConsString::cast(*this).second().length() == 0;
}

String String::GetUnderlying() {
  // Giving direct access to underlying string only makes sense if the
  // wrapping string is already flattened.
  DCHECK(this->IsFlat());
  DCHECK(StringShape(*this).IsIndirect());
  STATIC_ASSERT(static_cast<int>(ConsString::kFirstOffset) ==
                static_cast<int>(SlicedString::kParentOffset));
  STATIC_ASSERT(static_cast<int>(ConsString::kFirstOffset) ==
                static_cast<int>(ThinString::kActualOffset));
  const int kUnderlyingOffset = SlicedString::kParentOffset;
  return TaggedField<String, kUnderlyingOffset>::load(*this);
}

template <class Visitor>
ConsString String::VisitFlat(Visitor* visitor, String string,
                             const int offset) {
  DisallowHeapAllocation no_gc;
  int slice_offset = offset;
  const int length = string.length();
  DCHECK(offset <= length);
  while (true) {
    int32_t type = string.map().instance_type();
    switch (type & (kStringRepresentationMask | kStringEncodingMask)) {
      case kSeqStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            SeqOneByteString::cast(string).GetChars(no_gc) + slice_offset,
            length - offset);
        return ConsString();

      case kSeqStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            SeqTwoByteString::cast(string).GetChars(no_gc) + slice_offset,
            length - offset);
        return ConsString();

      case kExternalStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            ExternalOneByteString::cast(string).GetChars() + slice_offset,
            length - offset);
        return ConsString();

      case kExternalStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            ExternalTwoByteString::cast(string).GetChars() + slice_offset,
            length - offset);
        return ConsString();

      case kSlicedStringTag | kOneByteStringTag:
      case kSlicedStringTag | kTwoByteStringTag: {
        SlicedString slicedString = SlicedString::cast(string);
        slice_offset += slicedString.offset();
        string = slicedString.parent();
        continue;
      }

      case kConsStringTag | kOneByteStringTag:
      case kConsStringTag | kTwoByteStringTag:
        return ConsString::cast(string);

      case kThinStringTag | kOneByteStringTag:
      case kThinStringTag | kTwoByteStringTag:
        string = ThinString::cast(string).actual();
        continue;

      default:
        UNREACHABLE();
    }
  }
}

template <>
inline Vector<const uint8_t> String::GetCharVector(
    const DisallowHeapAllocation& no_gc) {
  String::FlatContent flat = GetFlatContent(no_gc);
  DCHECK(flat.IsOneByte());
  return flat.ToOneByteVector();
}

template <>
inline Vector<const uc16> String::GetCharVector(
    const DisallowHeapAllocation& no_gc) {
  String::FlatContent flat = GetFlatContent(no_gc);
  DCHECK(flat.IsTwoByte());
  return flat.ToUC16Vector();
}

uint32_t String::ToValidIndex(Object number) {
  uint32_t index = PositiveNumberToUint32(number);
  uint32_t length_value = static_cast<uint32_t>(length());
  if (index > length_value) return length_value;
  return index;
}

uint8_t SeqOneByteString::Get(int index) {
  DCHECK(index >= 0 && index < length());
  return ReadField<byte>(kHeaderSize + index * kCharSize);
}

void SeqOneByteString::SeqOneByteStringSet(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length() && value <= kMaxOneByteCharCode);
  WriteField<byte>(kHeaderSize + index * kCharSize, static_cast<byte>(value));
}

Address SeqOneByteString::GetCharsAddress() {
  return FIELD_ADDR(*this, kHeaderSize);
}

uint8_t* SeqOneByteString::GetChars(const DisallowHeapAllocation& no_gc) {
  USE(no_gc);
  return reinterpret_cast<uint8_t*>(GetCharsAddress());
}

Address SeqTwoByteString::GetCharsAddress() {
  return FIELD_ADDR(*this, kHeaderSize);
}

uc16* SeqTwoByteString::GetChars(const DisallowHeapAllocation& no_gc) {
  USE(no_gc);
  return reinterpret_cast<uc16*>(FIELD_ADDR(*this, kHeaderSize));
}

uint16_t SeqTwoByteString::Get(int index) {
  DCHECK(index >= 0 && index < length());
  return ReadField<uint16_t>(kHeaderSize + index * kShortSize);
}

void SeqTwoByteString::SeqTwoByteStringSet(int index, uint16_t value) {
  DCHECK(index >= 0 && index < length());
  WriteField<uint16_t>(kHeaderSize + index * kShortSize, value);
}

// Due to ThinString rewriting, concurrent visitors need to read the length with
// acquire semantics.
inline int SeqOneByteString::AllocatedSize() {
  return SizeFor(synchronized_length());
}
inline int SeqTwoByteString::AllocatedSize() {
  return SizeFor(synchronized_length());
}

void SlicedString::set_parent(String parent, WriteBarrierMode mode) {
  DCHECK(parent.IsSeqString() || parent.IsExternalString());
  TorqueGeneratedSlicedString<SlicedString, Super>::set_parent(parent, mode);
}

Object ConsString::unchecked_first() {
  return TaggedField<Object, kFirstOffset>::load(*this);
}

Object ConsString::unchecked_second() {
  return RELAXED_READ_FIELD(*this, kSecondOffset);
}

DEF_GETTER(ThinString, unchecked_actual, HeapObject) {
  return TaggedField<HeapObject, kActualOffset>::load(isolate, *this);
}

bool ExternalString::is_uncached() const {
  InstanceType type = map().instance_type();
  return (type & kUncachedExternalStringMask) == kUncachedExternalStringTag;
}

DEF_GETTER(ExternalString, resource_as_address, Address) {
  ExternalPointer_t encoded_address =
      ReadField<ExternalPointer_t>(kResourceOffset);
  return DecodeExternalPointer(isolate, encoded_address);
}

void ExternalString::set_address_as_resource(Isolate* isolate,
                                             Address address) {
  const ExternalPointer_t encoded_address =
      EncodeExternalPointer(isolate, address);
  WriteField<ExternalPointer_t>(kResourceOffset, encoded_address);
  if (IsExternalOneByteString()) {
    ExternalOneByteString::cast(*this).update_data_cache(isolate);
  } else {
    ExternalTwoByteString::cast(*this).update_data_cache(isolate);
  }
}

uint32_t ExternalString::resource_as_uint32() {
  ExternalPointer_t encoded_address =
      ReadField<ExternalPointer_t>(kResourceOffset);
  return static_cast<uint32_t>(encoded_address);
}

void ExternalString::set_uint32_as_resource(Isolate* isolate, uint32_t value) {
  WriteField<ExternalPointer_t>(kResourceOffset, value);
  if (is_uncached()) return;
  WriteField<ExternalPointer_t>(kResourceDataOffset,
                                EncodeExternalPointer(isolate, kNullAddress));
}

void ExternalString::DisposeResource(Isolate* isolate) {
  const ExternalPointer_t encoded_address =
      ReadField<ExternalPointer_t>(kResourceOffset);
  v8::String::ExternalStringResourceBase* resource =
      reinterpret_cast<v8::String::ExternalStringResourceBase*>(
          DecodeExternalPointer(isolate, encoded_address));

  // Dispose of the C++ object if it has not already been disposed.
  if (resource != nullptr) {
    resource->Dispose();
    const ExternalPointer_t encoded_address =
        EncodeExternalPointer(isolate, kNullAddress);
    WriteField<ExternalPointer_t>(kResourceOffset, encoded_address);
  }
}

DEF_GETTER(ExternalOneByteString, resource,
           const ExternalOneByteString::Resource*) {
  const ExternalPointer_t encoded_address =
      ReadField<ExternalPointer_t>(kResourceOffset);
  return reinterpret_cast<Resource*>(
      DecodeExternalPointer(isolate, encoded_address));
}

void ExternalOneByteString::update_data_cache(Isolate* isolate) {
  if (is_uncached()) return;
  const ExternalPointer_t encoded_resource_data = EncodeExternalPointer(
      isolate, reinterpret_cast<Address>(resource()->data()));
  WriteField<ExternalPointer_t>(kResourceDataOffset, encoded_resource_data);
}

void ExternalOneByteString::SetResource(
    Isolate* isolate, const ExternalOneByteString::Resource* resource) {
  set_resource(isolate, resource);
  size_t new_payload = resource == nullptr ? 0 : resource->length();
  if (new_payload > 0) {
    isolate->heap()->UpdateExternalString(*this, 0, new_payload);
  }
}

void ExternalOneByteString::set_resource(
    Isolate* isolate, const ExternalOneByteString::Resource* resource) {
  const ExternalPointer_t encoded_address =
      EncodeExternalPointer(isolate, reinterpret_cast<Address>(resource));
  WriteField<ExternalPointer_t>(kResourceOffset, encoded_address);
  if (resource != nullptr) update_data_cache(isolate);
}

const uint8_t* ExternalOneByteString::GetChars() {
  return reinterpret_cast<const uint8_t*>(resource()->data());
}

uint8_t ExternalOneByteString::Get(int index) {
  DCHECK(index >= 0 && index < length());
  return GetChars()[index];
}

DEF_GETTER(ExternalTwoByteString, resource,
           const ExternalTwoByteString::Resource*) {
  const ExternalPointer_t encoded_address =
      ReadField<ExternalPointer_t>(kResourceOffset);
  return reinterpret_cast<Resource*>(
      DecodeExternalPointer(isolate, encoded_address));
}

void ExternalTwoByteString::update_data_cache(Isolate* isolate) {
  if (is_uncached()) return;
  const ExternalPointer_t encoded_resource_data = EncodeExternalPointer(
      isolate, reinterpret_cast<Address>(resource()->data()));
  WriteField<ExternalPointer_t>(kResourceDataOffset, encoded_resource_data);
}

void ExternalTwoByteString::SetResource(
    Isolate* isolate, const ExternalTwoByteString::Resource* resource) {
  set_resource(isolate, resource);
  size_t new_payload = resource == nullptr ? 0 : resource->length() * 2;
  if (new_payload > 0) {
    isolate->heap()->UpdateExternalString(*this, 0, new_payload);
  }
}

void ExternalTwoByteString::set_resource(
    Isolate* isolate, const ExternalTwoByteString::Resource* resource) {
  const ExternalPointer_t encoded_address =
      EncodeExternalPointer(isolate, reinterpret_cast<Address>(resource));
  WriteField<ExternalPointer_t>(kResourceOffset, encoded_address);
  if (resource != nullptr) update_data_cache(isolate);
}

const uint16_t* ExternalTwoByteString::GetChars() { return resource()->data(); }

uint16_t ExternalTwoByteString::Get(int index) {
  DCHECK(index >= 0 && index < length());
  return GetChars()[index];
}

const uint16_t* ExternalTwoByteString::ExternalTwoByteStringGetData(
    unsigned start) {
  return GetChars() + start;
}

int ConsStringIterator::OffsetForDepth(int depth) { return depth & kDepthMask; }

void ConsStringIterator::PushLeft(ConsString string) {
  frames_[depth_++ & kDepthMask] = string;
}

void ConsStringIterator::PushRight(ConsString string) {
  // Inplace update.
  frames_[(depth_ - 1) & kDepthMask] = string;
}

void ConsStringIterator::AdjustMaximumDepth() {
  if (depth_ > maximum_depth_) maximum_depth_ = depth_;
}

void ConsStringIterator::Pop() {
  DCHECK_GT(depth_, 0);
  DCHECK(depth_ <= maximum_depth_);
  depth_--;
}

uint16_t StringCharacterStream::GetNext() {
  DCHECK(buffer8_ != nullptr && end_ != nullptr);
  // Advance cursor if needed.
  if (buffer8_ == end_) HasMore();
  DCHECK(buffer8_ < end_);
  return is_one_byte_ ? *buffer8_++ : *buffer16_++;
}

StringCharacterStream::StringCharacterStream(String string, int offset)
    : is_one_byte_(false) {
  Reset(string, offset);
}

void StringCharacterStream::Reset(String string, int offset) {
  buffer8_ = nullptr;
  end_ = nullptr;
  ConsString cons_string = String::VisitFlat(this, string, offset);
  iter_.Reset(cons_string, offset);
  if (!cons_string.is_null()) {
    string = iter_.Next(&offset);
    if (!string.is_null()) String::VisitFlat(this, string, offset);
  }
}

bool StringCharacterStream::HasMore() {
  if (buffer8_ != end_) return true;
  int offset;
  String string = iter_.Next(&offset);
  DCHECK_EQ(offset, 0);
  if (string.is_null()) return false;
  String::VisitFlat(this, string);
  DCHECK(buffer8_ != end_);
  return true;
}

void StringCharacterStream::VisitOneByteString(const uint8_t* chars,
                                               int length) {
  is_one_byte_ = true;
  buffer8_ = chars;
  end_ = chars + length;
}

void StringCharacterStream::VisitTwoByteString(const uint16_t* chars,
                                               int length) {
  is_one_byte_ = false;
  buffer16_ = chars;
  end_ = reinterpret_cast<const uint8_t*>(chars + length);
}

bool String::AsArrayIndex(uint32_t* index) {
  DisallowHeapAllocation no_gc;
  uint32_t field = hash_field();
  if (ContainsCachedArrayIndex(field)) {
    *index = ArrayIndexValueBits::decode(field);
    return true;
  }
  if (IsHashFieldComputed(field) && (field & kIsNotIntegerIndexMask)) {
    return false;
  }
  return SlowAsArrayIndex(index);
}

bool String::AsIntegerIndex(size_t* index) {
  uint32_t field = hash_field();
  if (ContainsCachedArrayIndex(field)) {
    *index = ArrayIndexValueBits::decode(field);
    return true;
  }
  if (IsHashFieldComputed(field) && (field & kIsNotIntegerIndexMask)) {
    return false;
  }
  return SlowAsIntegerIndex(index);
}

SubStringRange::SubStringRange(String string,
                               const DisallowHeapAllocation& no_gc, int first,
                               int length)
    : string_(string),
      first_(first),
      length_(length == -1 ? string.length() : length),
      no_gc_(no_gc) {}

class SubStringRange::iterator final {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = int;
  using value_type = uc16;
  using pointer = uc16*;
  using reference = uc16&;

  iterator(const iterator& other) = default;

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
  friend class SubStringRange;
  iterator(String from, int offset, const DisallowHeapAllocation& no_gc)
      : content_(from.GetFlatContent(no_gc)), offset_(offset) {}
  String::FlatContent content_;
  int offset_;
};

SubStringRange::iterator SubStringRange::begin() {
  return SubStringRange::iterator(string_, first_, no_gc_);
}

SubStringRange::iterator SubStringRange::end() {
  return SubStringRange::iterator(string_, first_ + length_, no_gc_);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_INL_H_
