// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STRING_INL_H_
#define V8_OBJECTS_STRING_INL_H_

#include "src/objects/string.h"

#include "src/conversions-inl.h"
#include "src/handles-inl.h"
#include "src/heap/factory.h"
#include "src/objects/name-inl.h"
#include "src/string-hasher-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

SMI_ACCESSORS(String, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(String, length, kLengthOffset)

CAST_ACCESSOR(ConsString)
CAST_ACCESSOR(ExternalOneByteString)
CAST_ACCESSOR(ExternalString)
CAST_ACCESSOR(ExternalTwoByteString)
CAST_ACCESSOR(InternalizedString)
CAST_ACCESSOR(SeqOneByteString)
CAST_ACCESSOR(SeqString)
CAST_ACCESSOR(SeqTwoByteString)
CAST_ACCESSOR(SlicedString)
CAST_ACCESSOR(String)
CAST_ACCESSOR(ThinString)

StringShape::StringShape(const String* str)
    : type_(str->map()->instance_type()) {
  set_valid();
  DCHECK_EQ(type_ & kIsNotStringMask, kStringTag);
}

StringShape::StringShape(Map* map) : type_(map->instance_type()) {
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
class SequentialStringKey : public StringTableKey {
 public:
  explicit SequentialStringKey(Vector<const Char> string, uint64_t seed)
      : StringTableKey(StringHasher::HashSequentialString<Char>(
            string.start(), string.length(), seed)),
        string_(string) {}

  Vector<const Char> string_;
};

class OneByteStringKey : public SequentialStringKey<uint8_t> {
 public:
  OneByteStringKey(Vector<const uint8_t> str, uint64_t seed)
      : SequentialStringKey<uint8_t>(str, seed) {}

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsOneByteEqualTo(string_);
  }

  Handle<String> AsHandle(Isolate* isolate) override;
};

class SeqOneByteSubStringKey : public StringTableKey {
 public:
// VS 2017 on official builds gives this spurious warning:
// warning C4789: buffer 'key' of size 16 bytes will be overrun; 4 bytes will
// be written starting at offset 16
// https://bugs.chromium.org/p/v8/issues/detail?id=6068
#if defined(V8_CC_MSVC)
#pragma warning(push)
#pragma warning(disable : 4789)
#endif
  SeqOneByteSubStringKey(Isolate* isolate, Handle<SeqOneByteString> string,
                         int from, int length)
      : StringTableKey(StringHasher::HashSequentialString(
            string->GetChars() + from, length, isolate->heap()->HashSeed())),
        string_(string),
        from_(from),
        length_(length) {
    DCHECK_LE(0, length_);
    DCHECK_LE(from_ + length_, string_->length());
    DCHECK(string_->IsSeqOneByteString());
  }
#if defined(V8_CC_MSVC)
#pragma warning(pop)
#endif

  bool IsMatch(Object* string) override;
  Handle<String> AsHandle(Isolate* isolate) override;

 private:
  Handle<SeqOneByteString> string_;
  int from_;
  int length_;
};

class TwoByteStringKey : public SequentialStringKey<uc16> {
 public:
  explicit TwoByteStringKey(Vector<const uc16> str, uint64_t seed)
      : SequentialStringKey<uc16>(str, seed) {}

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsTwoByteEqualTo(string_);
  }

  Handle<String> AsHandle(Isolate* isolate) override;
};

// Utf8StringKey carries a vector of chars as key.
class Utf8StringKey : public StringTableKey {
 public:
  explicit Utf8StringKey(Vector<const char> string, uint64_t seed)
      : StringTableKey(StringHasher::ComputeUtf8Hash(string, seed, &chars_)),
        string_(string) {}

  bool IsMatch(Object* string) override {
    return String::cast(string)->IsUtf8EqualTo(string_);
  }

  Handle<String> AsHandle(Isolate* isolate) override {
    return isolate->factory()->NewInternalizedStringFromUtf8(string_, chars_,
                                                             HashField());
  }

 private:
  Vector<const char> string_;
  int chars_;  // Caches the number of characters when computing the hash code.
};

bool String::Equals(String* other) {
  if (other == this) return true;
  if (this->IsInternalizedString() && other->IsInternalizedString()) {
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

Handle<String> String::Flatten(Isolate* isolate, Handle<String> string,
                               PretenureFlag pretenure) {
  if (string->IsConsString()) {
    Handle<ConsString> cons = Handle<ConsString>::cast(string);
    if (cons->IsFlat()) {
      string = handle(cons->first(), isolate);
    } else {
      return SlowFlatten(isolate, cons, pretenure);
    }
  }
  if (string->IsThinString()) {
    string = handle(Handle<ThinString>::cast(string)->actual(), isolate);
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

template <class Visitor>
ConsString* String::VisitFlat(Visitor* visitor, String* string,
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
        return nullptr;

      case kSeqStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            SeqTwoByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return nullptr;

      case kExternalStringTag | kOneByteStringTag:
        visitor->VisitOneByteString(
            ExternalOneByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return nullptr;

      case kExternalStringTag | kTwoByteStringTag:
        visitor->VisitTwoByteString(
            ExternalTwoByteString::cast(string)->GetChars() + slice_offset,
            length - offset);
        return nullptr;

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

void SlicedString::set_parent(Isolate* isolate, String* parent,
                              WriteBarrierMode mode) {
  DCHECK(parent->IsSeqString() || parent->IsExternalString());
  WRITE_FIELD(this, kParentOffset, parent);
  CONDITIONAL_WRITE_BARRIER(this, kParentOffset, parent, mode);
}

SMI_ACCESSORS(SlicedString, offset, kOffsetOffset)

String* ConsString::first() {
  return String::cast(READ_FIELD(this, kFirstOffset));
}

Object* ConsString::unchecked_first() { return READ_FIELD(this, kFirstOffset); }

void ConsString::set_first(Isolate* isolate, String* value,
                           WriteBarrierMode mode) {
  WRITE_FIELD(this, kFirstOffset, value);
  CONDITIONAL_WRITE_BARRIER(this, kFirstOffset, value, mode);
}

String* ConsString::second() {
  return String::cast(READ_FIELD(this, kSecondOffset));
}

Object* ConsString::unchecked_second() {
  return RELAXED_READ_FIELD(this, kSecondOffset);
}

void ConsString::set_second(Isolate* isolate, String* value,
                            WriteBarrierMode mode) {
  WRITE_FIELD(this, kSecondOffset, value);
  CONDITIONAL_WRITE_BARRIER(this, kSecondOffset, value, mode);
}

ACCESSORS(ThinString, actual, String, kActualOffset);

HeapObject* ThinString::unchecked_actual() const {
  return reinterpret_cast<HeapObject*>(READ_FIELD(this, kActualOffset));
}

bool ExternalString::is_short() const {
  InstanceType type = map()->instance_type();
  return (type & kShortExternalStringMask) == kShortExternalStringTag;
}

Address ExternalString::resource_as_address() {
  return *reinterpret_cast<Address*>(FIELD_ADDR(this, kResourceOffset));
}

void ExternalString::set_address_as_resource(Address address) {
  DCHECK(IsAligned(address, kPointerSize));
  *reinterpret_cast<Address*>(FIELD_ADDR(this, kResourceOffset)) = address;
  if (IsExternalOneByteString()) {
    ExternalOneByteString::cast(this)->update_data_cache();
  } else {
    ExternalTwoByteString::cast(this)->update_data_cache();
  }
}

uint32_t ExternalString::resource_as_uint32() {
  return static_cast<uint32_t>(
      *reinterpret_cast<uintptr_t*>(FIELD_ADDR(this, kResourceOffset)));
}

void ExternalString::set_uint32_as_resource(uint32_t value) {
  *reinterpret_cast<uintptr_t*>(FIELD_ADDR(this, kResourceOffset)) = value;
  if (is_short()) return;
  const char** data_field =
      reinterpret_cast<const char**>(FIELD_ADDR(this, kResourceDataOffset));
  *data_field = nullptr;
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

void ExternalOneByteString::SetResource(
    Isolate* isolate, const ExternalOneByteString::Resource* resource) {
  set_resource(resource);
  size_t new_payload = resource == nullptr ? 0 : resource->length();
  if (new_payload > 0)
    isolate->heap()->UpdateExternalString(this, 0, new_payload);
}

void ExternalOneByteString::set_resource(
    const ExternalOneByteString::Resource* resource) {
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(resource), kPointerSize));
  *reinterpret_cast<const Resource**>(FIELD_ADDR(this, kResourceOffset)) =
      resource;
  if (resource != nullptr) update_data_cache();
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

void ExternalTwoByteString::SetResource(
    Isolate* isolate, const ExternalTwoByteString::Resource* resource) {
  set_resource(resource);
  size_t new_payload = resource == nullptr ? 0 : resource->length() * 2;
  if (new_payload > 0)
    isolate->heap()->UpdateExternalString(this, 0, new_payload);
}

void ExternalTwoByteString::set_resource(
    const ExternalTwoByteString::Resource* resource) {
  *reinterpret_cast<const Resource**>(FIELD_ADDR(this, kResourceOffset)) =
      resource;
  if (resource != nullptr) update_data_cache();
}

const uint16_t* ExternalTwoByteString::GetChars() { return resource()->data(); }

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

StringCharacterStream::StringCharacterStream(String* string, int offset)
    : is_one_byte_(false) {
  Reset(string, offset);
}

void StringCharacterStream::Reset(String* string, int offset) {
  buffer8_ = nullptr;
  end_ = nullptr;
  ConsString* cons_string = String::VisitFlat(this, string, offset);
  iter_.Reset(cons_string, offset);
  if (cons_string != nullptr) {
    string = iter_.Next(&offset);
    if (string != nullptr) String::VisitFlat(this, string, offset);
  }
}

bool StringCharacterStream::HasMore() {
  if (buffer8_ != end_) return true;
  int offset;
  String* string = iter_.Next(&offset);
  DCHECK_EQ(offset, 0);
  if (string == nullptr) return false;
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
  uint32_t field = hash_field();
  if (IsHashFieldComputed(field) && (field & kIsNotArrayIndexMask)) {
    return false;
  }
  return SlowAsArrayIndex(index);
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STRING_INL_H_
