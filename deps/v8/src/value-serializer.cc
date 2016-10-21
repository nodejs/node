// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/value-serializer.h"

#include <type_traits>

#include "src/base/logging.h"
#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

static const uint32_t kLatestVersion = 9;

template <typename T>
static size_t BytesNeededForVarint(T value) {
  static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                "Only unsigned integer types can be written as varints.");
  size_t result = 0;
  do {
    result++;
    value >>= 7;
  } while (value);
  return result;
}

enum class SerializationTag : uint8_t {
  // version:uint32_t (if at beginning of data, sets version > 0)
  kVersion = 0xFF,
  // ignore
  kPadding = '\0',
  // refTableSize:uint32_t (previously used for sanity checks; safe to ignore)
  kVerifyObjectCount = '?',
  // Oddballs (no data).
  kUndefined = '_',
  kNull = '0',
  kTrue = 'T',
  kFalse = 'F',
  // Number represented as 32-bit integer, ZigZag-encoded
  // (like sint32 in protobuf)
  kInt32 = 'I',
  // Number represented as 32-bit unsigned integer, varint-encoded
  // (like uint32 in protobuf)
  kUint32 = 'U',
  // Number represented as a 64-bit double.
  // Host byte order is used (N.B. this makes the format non-portable).
  kDouble = 'N',
  // byteLength:uint32_t, then raw data
  kUtf8String = 'S',
  kTwoByteString = 'c',
  // Reference to a serialized object. objectID:uint32_t
  kObjectReference = '^',
  // Beginning of a JS object.
  kBeginJSObject = 'o',
  // End of a JS object. numProperties:uint32_t
  kEndJSObject = '{',
  // Beginning of a sparse JS array. length:uint32_t
  // Elements and properties are written as key/value pairs, like objects.
  kBeginSparseJSArray = 'a',
  // End of a sparse JS array. numProperties:uint32_t length:uint32_t
  kEndSparseJSArray = '@',
  // Beginning of a dense JS array. length:uint32_t
  // |length| elements, followed by properties as key/value pairs
  kBeginDenseJSArray = 'A',
  // End of a dense JS array. numProperties:uint32_t length:uint32_t
  kEndDenseJSArray = '$',
  // Date. millisSinceEpoch:double
  kDate = 'D',
  // Boolean object. No data.
  kTrueObject = 'y',
  kFalseObject = 'x',
  // Number object. value:double
  kNumberObject = 'n',
  // String object, UTF-8 encoding. byteLength:uint32_t, then raw data.
  kStringObject = 's',
  // Regular expression, UTF-8 encoding. byteLength:uint32_t, raw data,
  // flags:uint32_t.
  kRegExp = 'R',
  // Beginning of a JS map.
  kBeginJSMap = ';',
  // End of a JS map. length:uint32_t.
  kEndJSMap = ':',
  // Beginning of a JS set.
  kBeginJSSet = '\'',
  // End of a JS set. length:uint32_t.
  kEndJSSet = ',',
};

ValueSerializer::ValueSerializer(Isolate* isolate)
    : isolate_(isolate),
      zone_(isolate->allocator()),
      id_map_(isolate->heap(), &zone_) {}

ValueSerializer::~ValueSerializer() {}

void ValueSerializer::WriteHeader() {
  WriteTag(SerializationTag::kVersion);
  WriteVarint(kLatestVersion);
}

void ValueSerializer::WriteTag(SerializationTag tag) {
  buffer_.push_back(static_cast<uint8_t>(tag));
}

template <typename T>
void ValueSerializer::WriteVarint(T value) {
  // Writes an unsigned integer as a base-128 varint.
  // The number is written, 7 bits at a time, from the least significant to the
  // most significant 7 bits. Each byte, except the last, has the MSB set.
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                "Only unsigned integer types can be written as varints.");
  uint8_t stack_buffer[sizeof(T) * 8 / 7 + 1];
  uint8_t* next_byte = &stack_buffer[0];
  do {
    *next_byte = (value & 0x7f) | 0x80;
    next_byte++;
    value >>= 7;
  } while (value);
  *(next_byte - 1) &= 0x7f;
  buffer_.insert(buffer_.end(), stack_buffer, next_byte);
}

template <typename T>
void ValueSerializer::WriteZigZag(T value) {
  // Writes a signed integer as a varint using ZigZag encoding (i.e. 0 is
  // encoded as 0, -1 as 1, 1 as 2, -2 as 3, and so on).
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  // Note that this implementation relies on the right shift being arithmetic.
  static_assert(std::is_integral<T>::value && std::is_signed<T>::value,
                "Only signed integer types can be written as zigzag.");
  using UnsignedT = typename std::make_unsigned<T>::type;
  WriteVarint((static_cast<UnsignedT>(value) << 1) ^
              (value >> (8 * sizeof(T) - 1)));
}

void ValueSerializer::WriteDouble(double value) {
  // Warning: this uses host endianness.
  buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(&value),
                 reinterpret_cast<const uint8_t*>(&value + 1));
}

void ValueSerializer::WriteOneByteString(Vector<const uint8_t> chars) {
  WriteVarint<uint32_t>(chars.length());
  buffer_.insert(buffer_.end(), chars.begin(), chars.end());
}

void ValueSerializer::WriteTwoByteString(Vector<const uc16> chars) {
  // Warning: this uses host endianness.
  WriteVarint<uint32_t>(chars.length() * sizeof(uc16));
  buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(chars.begin()),
                 reinterpret_cast<const uint8_t*>(chars.end()));
}

uint8_t* ValueSerializer::ReserveRawBytes(size_t bytes) {
  if (!bytes) return nullptr;
  auto old_size = buffer_.size();
  buffer_.resize(buffer_.size() + bytes);
  return &buffer_[old_size];
}

Maybe<bool> ValueSerializer::WriteObject(Handle<Object> object) {
  if (object->IsSmi()) {
    WriteSmi(Smi::cast(*object));
    return Just(true);
  }

  DCHECK(object->IsHeapObject());
  switch (HeapObject::cast(*object)->map()->instance_type()) {
    case ODDBALL_TYPE:
      WriteOddball(Oddball::cast(*object));
      return Just(true);
    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
      WriteHeapNumber(HeapNumber::cast(*object));
      return Just(true);
    default:
      if (object->IsString()) {
        WriteString(Handle<String>::cast(object));
        return Just(true);
      } else if (object->IsJSReceiver()) {
        return WriteJSReceiver(Handle<JSReceiver>::cast(object));
      }
      UNIMPLEMENTED();
      return Nothing<bool>();
  }
}

void ValueSerializer::WriteOddball(Oddball* oddball) {
  SerializationTag tag = SerializationTag::kUndefined;
  switch (oddball->kind()) {
    case Oddball::kUndefined:
      tag = SerializationTag::kUndefined;
      break;
    case Oddball::kFalse:
      tag = SerializationTag::kFalse;
      break;
    case Oddball::kTrue:
      tag = SerializationTag::kTrue;
      break;
    case Oddball::kNull:
      tag = SerializationTag::kNull;
      break;
    default:
      UNREACHABLE();
      break;
  }
  WriteTag(tag);
}

void ValueSerializer::WriteSmi(Smi* smi) {
  static_assert(kSmiValueSize <= 32, "Expected SMI <= 32 bits.");
  WriteTag(SerializationTag::kInt32);
  WriteZigZag<int32_t>(smi->value());
}

void ValueSerializer::WriteHeapNumber(HeapNumber* number) {
  WriteTag(SerializationTag::kDouble);
  WriteDouble(number->value());
}

void ValueSerializer::WriteString(Handle<String> string) {
  string = String::Flatten(string);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = string->GetFlatContent();
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    // The existing format uses UTF-8, rather than Latin-1. As a result we must
    // to do work to encode strings that have characters outside ASCII.
    // TODO(jbroman): In a future format version, consider adding a tag for
    // Latin-1 strings, so that this can be skipped.
    WriteTag(SerializationTag::kUtf8String);
    Vector<const uint8_t> chars = flat.ToOneByteVector();
    if (String::IsAscii(chars.begin(), chars.length())) {
      WriteOneByteString(chars);
    } else {
      v8::Local<v8::String> api_string = Utils::ToLocal(string);
      uint32_t utf8_length = api_string->Utf8Length();
      WriteVarint(utf8_length);
      api_string->WriteUtf8(
          reinterpret_cast<char*>(ReserveRawBytes(utf8_length)), utf8_length,
          nullptr, v8::String::NO_NULL_TERMINATION);
    }
  } else if (flat.IsTwoByte()) {
    Vector<const uc16> chars = flat.ToUC16Vector();
    uint32_t byte_length = chars.length() * sizeof(uc16);
    // The existing reading code expects 16-byte strings to be aligned.
    if ((buffer_.size() + 1 + BytesNeededForVarint(byte_length)) & 1)
      WriteTag(SerializationTag::kPadding);
    WriteTag(SerializationTag::kTwoByteString);
    WriteTwoByteString(chars);
  } else {
    UNREACHABLE();
  }
}

Maybe<bool> ValueSerializer::WriteJSReceiver(Handle<JSReceiver> receiver) {
  // If the object has already been serialized, just write its ID.
  uint32_t* id_map_entry = id_map_.Get(receiver);
  if (uint32_t id = *id_map_entry) {
    WriteTag(SerializationTag::kObjectReference);
    WriteVarint(id - 1);
    return Just(true);
  }

  // Otherwise, allocate an ID for it.
  uint32_t id = next_id_++;
  *id_map_entry = id + 1;

  // Eliminate callable and exotic objects, which should not be serialized.
  InstanceType instance_type = receiver->map()->instance_type();
  if (receiver->IsCallable() || instance_type <= LAST_SPECIAL_RECEIVER_TYPE) {
    return Nothing<bool>();
  }

  // If we are at the end of the stack, abort. This function may recurse.
  if (StackLimitCheck(isolate_).HasOverflowed()) return Nothing<bool>();

  HandleScope scope(isolate_);
  switch (instance_type) {
    case JS_ARRAY_TYPE:
      return WriteJSArray(Handle<JSArray>::cast(receiver));
    case JS_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE:
      return WriteJSObject(Handle<JSObject>::cast(receiver));
    case JS_DATE_TYPE:
      WriteJSDate(JSDate::cast(*receiver));
      return Just(true);
    case JS_VALUE_TYPE:
      return WriteJSValue(Handle<JSValue>::cast(receiver));
    case JS_REGEXP_TYPE:
      WriteJSRegExp(JSRegExp::cast(*receiver));
      return Just(true);
    case JS_MAP_TYPE:
      return WriteJSMap(Handle<JSMap>::cast(receiver));
    case JS_SET_TYPE:
      return WriteJSSet(Handle<JSSet>::cast(receiver));
    default:
      UNIMPLEMENTED();
      break;
  }
  return Nothing<bool>();
}

Maybe<bool> ValueSerializer::WriteJSObject(Handle<JSObject> object) {
  WriteTag(SerializationTag::kBeginJSObject);
  Handle<FixedArray> keys;
  uint32_t properties_written;
  if (!KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                               ENUMERABLE_STRINGS)
           .ToHandle(&keys) ||
      !WriteJSObjectProperties(object, keys).To(&properties_written)) {
    return Nothing<bool>();
  }
  WriteTag(SerializationTag::kEndJSObject);
  WriteVarint<uint32_t>(properties_written);
  return Just(true);
}

Maybe<bool> ValueSerializer::WriteJSArray(Handle<JSArray> array) {
  uint32_t length = 0;
  bool valid_length = array->length()->ToArrayLength(&length);
  DCHECK(valid_length);
  USE(valid_length);

  // To keep things simple, for now we decide between dense and sparse
  // serialization based on elements kind. A more principled heuristic could
  // count the elements, but would need to take care to note which indices
  // existed (as only indices which were enumerable own properties at this point
  // should be serialized).
  const bool should_serialize_densely =
      array->HasFastElements() && !array->HasFastHoleyElements();

  if (should_serialize_densely) {
    // TODO(jbroman): Distinguish between undefined and a hole (this can happen
    // if serializing one of the elements deletes another). This requires wire
    // format changes.
    WriteTag(SerializationTag::kBeginDenseJSArray);
    WriteVarint<uint32_t>(length);
    for (uint32_t i = 0; i < length; i++) {
      // Serializing the array's elements can have arbitrary side effects, so we
      // cannot rely on still having fast elements, even if it did to begin
      // with.
      Handle<Object> element;
      LookupIterator it(isolate_, array, i, array, LookupIterator::OWN);
      if (!Object::GetProperty(&it).ToHandle(&element) ||
          !WriteObject(element).FromMaybe(false)) {
        return Nothing<bool>();
      }
    }
    KeyAccumulator accumulator(isolate_, KeyCollectionMode::kOwnOnly,
                               ENUMERABLE_STRINGS);
    if (!accumulator.CollectOwnPropertyNames(array, array).FromMaybe(false)) {
      return Nothing<bool>();
    }
    Handle<FixedArray> keys =
        accumulator.GetKeys(GetKeysConversion::kConvertToString);
    uint32_t properties_written;
    if (!WriteJSObjectProperties(array, keys).To(&properties_written)) {
      return Nothing<bool>();
    }
    WriteTag(SerializationTag::kEndDenseJSArray);
    WriteVarint<uint32_t>(properties_written);
    WriteVarint<uint32_t>(length);
  } else {
    WriteTag(SerializationTag::kBeginSparseJSArray);
    WriteVarint<uint32_t>(length);
    Handle<FixedArray> keys;
    uint32_t properties_written;
    if (!KeyAccumulator::GetKeys(array, KeyCollectionMode::kOwnOnly,
                                 ENUMERABLE_STRINGS)
             .ToHandle(&keys) ||
        !WriteJSObjectProperties(array, keys).To(&properties_written)) {
      return Nothing<bool>();
    }
    WriteTag(SerializationTag::kEndSparseJSArray);
    WriteVarint<uint32_t>(properties_written);
    WriteVarint<uint32_t>(length);
  }
  return Just(true);
}

void ValueSerializer::WriteJSDate(JSDate* date) {
  WriteTag(SerializationTag::kDate);
  WriteDouble(date->value()->Number());
}

Maybe<bool> ValueSerializer::WriteJSValue(Handle<JSValue> value) {
  Object* inner_value = value->value();
  if (inner_value->IsTrue(isolate_)) {
    WriteTag(SerializationTag::kTrueObject);
  } else if (inner_value->IsFalse(isolate_)) {
    WriteTag(SerializationTag::kFalseObject);
  } else if (inner_value->IsNumber()) {
    WriteTag(SerializationTag::kNumberObject);
    WriteDouble(inner_value->Number());
  } else if (inner_value->IsString()) {
    // TODO(jbroman): Replace UTF-8 encoding with the same options available for
    // ordinary strings.
    WriteTag(SerializationTag::kStringObject);
    v8::Local<v8::String> api_string =
        Utils::ToLocal(handle(String::cast(inner_value), isolate_));
    uint32_t utf8_length = api_string->Utf8Length();
    WriteVarint(utf8_length);
    api_string->WriteUtf8(reinterpret_cast<char*>(ReserveRawBytes(utf8_length)),
                          utf8_length, nullptr,
                          v8::String::NO_NULL_TERMINATION);
  } else {
    DCHECK(inner_value->IsSymbol());
    return Nothing<bool>();
  }
  return Just(true);
}

void ValueSerializer::WriteJSRegExp(JSRegExp* regexp) {
  WriteTag(SerializationTag::kRegExp);
  v8::Local<v8::String> api_string =
      Utils::ToLocal(handle(regexp->Pattern(), isolate_));
  uint32_t utf8_length = api_string->Utf8Length();
  WriteVarint(utf8_length);
  api_string->WriteUtf8(reinterpret_cast<char*>(ReserveRawBytes(utf8_length)),
                        utf8_length, nullptr, v8::String::NO_NULL_TERMINATION);
  WriteVarint(static_cast<uint32_t>(regexp->GetFlags()));
}

Maybe<bool> ValueSerializer::WriteJSMap(Handle<JSMap> map) {
  // First copy the key-value pairs, since getters could mutate them.
  Handle<OrderedHashMap> table(OrderedHashMap::cast(map->table()));
  int length = table->NumberOfElements() * 2;
  Handle<FixedArray> entries = isolate_->factory()->NewFixedArray(length);
  {
    DisallowHeapAllocation no_gc;
    Oddball* the_hole = isolate_->heap()->the_hole_value();
    int capacity = table->UsedCapacity();
    int result_index = 0;
    for (int i = 0; i < capacity; i++) {
      Object* key = table->KeyAt(i);
      if (key == the_hole) continue;
      entries->set(result_index++, key);
      entries->set(result_index++, table->ValueAt(i));
    }
    DCHECK_EQ(result_index, length);
  }

  // Then write it out.
  WriteTag(SerializationTag::kBeginJSMap);
  for (int i = 0; i < length; i++) {
    if (!WriteObject(handle(entries->get(i), isolate_)).FromMaybe(false)) {
      return Nothing<bool>();
    }
  }
  WriteTag(SerializationTag::kEndJSMap);
  WriteVarint<uint32_t>(length);
  return Just(true);
}

Maybe<bool> ValueSerializer::WriteJSSet(Handle<JSSet> set) {
  // First copy the element pointers, since getters could mutate them.
  Handle<OrderedHashSet> table(OrderedHashSet::cast(set->table()));
  int length = table->NumberOfElements();
  Handle<FixedArray> entries = isolate_->factory()->NewFixedArray(length);
  {
    DisallowHeapAllocation no_gc;
    Oddball* the_hole = isolate_->heap()->the_hole_value();
    int capacity = table->UsedCapacity();
    int result_index = 0;
    for (int i = 0; i < capacity; i++) {
      Object* key = table->KeyAt(i);
      if (key == the_hole) continue;
      entries->set(result_index++, key);
    }
    DCHECK_EQ(result_index, length);
  }

  // Then write it out.
  WriteTag(SerializationTag::kBeginJSSet);
  for (int i = 0; i < length; i++) {
    if (!WriteObject(handle(entries->get(i), isolate_)).FromMaybe(false)) {
      return Nothing<bool>();
    }
  }
  WriteTag(SerializationTag::kEndJSSet);
  WriteVarint<uint32_t>(length);
  return Just(true);
}

Maybe<uint32_t> ValueSerializer::WriteJSObjectProperties(
    Handle<JSObject> object, Handle<FixedArray> keys) {
  uint32_t properties_written = 0;
  int length = keys->length();
  for (int i = 0; i < length; i++) {
    Handle<Object> key(keys->get(i), isolate_);

    bool success;
    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate_, object, key, &success, LookupIterator::OWN);
    DCHECK(success);
    Handle<Object> value;
    if (!Object::GetProperty(&it).ToHandle(&value)) return Nothing<uint32_t>();

    // If the property is no longer found, do not serialize it.
    // This could happen if a getter deleted the property.
    if (!it.IsFound()) continue;

    if (!WriteObject(key).FromMaybe(false) ||
        !WriteObject(value).FromMaybe(false)) {
      return Nothing<uint32_t>();
    }

    properties_written++;
  }
  return Just(properties_written);
}

ValueDeserializer::ValueDeserializer(Isolate* isolate,
                                     Vector<const uint8_t> data)
    : isolate_(isolate),
      position_(data.start()),
      end_(data.start() + data.length()),
      id_map_(Handle<SeededNumberDictionary>::cast(
          isolate->global_handles()->Create(
              *SeededNumberDictionary::New(isolate, 0)))) {}

ValueDeserializer::~ValueDeserializer() {
  GlobalHandles::Destroy(Handle<Object>::cast(id_map_).location());
}

Maybe<bool> ValueDeserializer::ReadHeader() {
  if (position_ < end_ &&
      *position_ == static_cast<uint8_t>(SerializationTag::kVersion)) {
    ReadTag().ToChecked();
    if (!ReadVarint<uint32_t>().To(&version_)) return Nothing<bool>();
    if (version_ > kLatestVersion) return Nothing<bool>();
  }
  return Just(true);
}

Maybe<SerializationTag> ValueDeserializer::PeekTag() const {
  const uint8_t* peek_position = position_;
  SerializationTag tag;
  do {
    if (peek_position >= end_) return Nothing<SerializationTag>();
    tag = static_cast<SerializationTag>(*peek_position);
    peek_position++;
  } while (tag == SerializationTag::kPadding);
  return Just(tag);
}

void ValueDeserializer::ConsumeTag(SerializationTag peeked_tag) {
  SerializationTag actual_tag = ReadTag().ToChecked();
  DCHECK(actual_tag == peeked_tag);
  USE(actual_tag);
}

Maybe<SerializationTag> ValueDeserializer::ReadTag() {
  SerializationTag tag;
  do {
    if (position_ >= end_) return Nothing<SerializationTag>();
    tag = static_cast<SerializationTag>(*position_);
    position_++;
  } while (tag == SerializationTag::kPadding);
  return Just(tag);
}

template <typename T>
Maybe<T> ValueDeserializer::ReadVarint() {
  // Reads an unsigned integer as a base-128 varint.
  // The number is written, 7 bits at a time, from the least significant to the
  // most significant 7 bits. Each byte, except the last, has the MSB set.
  // If the varint is larger than T, any more significant bits are discarded.
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  static_assert(std::is_integral<T>::value && std::is_unsigned<T>::value,
                "Only unsigned integer types can be read as varints.");
  T value = 0;
  unsigned shift = 0;
  bool has_another_byte;
  do {
    if (position_ >= end_) return Nothing<T>();
    uint8_t byte = *position_;
    if (V8_LIKELY(shift < sizeof(T) * 8)) {
      value |= (byte & 0x7f) << shift;
      shift += 7;
    }
    has_another_byte = byte & 0x80;
    position_++;
  } while (has_another_byte);
  return Just(value);
}

template <typename T>
Maybe<T> ValueDeserializer::ReadZigZag() {
  // Writes a signed integer as a varint using ZigZag encoding (i.e. 0 is
  // encoded as 0, -1 as 1, 1 as 2, -2 as 3, and so on).
  // See also https://developers.google.com/protocol-buffers/docs/encoding
  static_assert(std::is_integral<T>::value && std::is_signed<T>::value,
                "Only signed integer types can be read as zigzag.");
  using UnsignedT = typename std::make_unsigned<T>::type;
  UnsignedT unsigned_value;
  if (!ReadVarint<UnsignedT>().To(&unsigned_value)) return Nothing<T>();
  return Just(static_cast<T>((unsigned_value >> 1) ^
                             -static_cast<T>(unsigned_value & 1)));
}

Maybe<double> ValueDeserializer::ReadDouble() {
  // Warning: this uses host endianness.
  if (position_ > end_ - sizeof(double)) return Nothing<double>();
  double value;
  memcpy(&value, position_, sizeof(double));
  position_ += sizeof(double);
  if (std::isnan(value)) value = std::numeric_limits<double>::quiet_NaN();
  return Just(value);
}

Maybe<Vector<const uint8_t>> ValueDeserializer::ReadRawBytes(int size) {
  if (size > end_ - position_) return Nothing<Vector<const uint8_t>>();
  const uint8_t* start = position_;
  position_ += size;
  return Just(Vector<const uint8_t>(start, size));
}

MaybeHandle<Object> ValueDeserializer::ReadObject() {
  SerializationTag tag;
  if (!ReadTag().To(&tag)) return MaybeHandle<Object>();
  switch (tag) {
    case SerializationTag::kVerifyObjectCount:
      // Read the count and ignore it.
      if (ReadVarint<uint32_t>().IsNothing()) return MaybeHandle<Object>();
      return ReadObject();
    case SerializationTag::kUndefined:
      return isolate_->factory()->undefined_value();
    case SerializationTag::kNull:
      return isolate_->factory()->null_value();
    case SerializationTag::kTrue:
      return isolate_->factory()->true_value();
    case SerializationTag::kFalse:
      return isolate_->factory()->false_value();
    case SerializationTag::kInt32: {
      Maybe<int32_t> number = ReadZigZag<int32_t>();
      if (number.IsNothing()) return MaybeHandle<Object>();
      return isolate_->factory()->NewNumberFromInt(number.FromJust());
    }
    case SerializationTag::kUint32: {
      Maybe<uint32_t> number = ReadVarint<uint32_t>();
      if (number.IsNothing()) return MaybeHandle<Object>();
      return isolate_->factory()->NewNumberFromUint(number.FromJust());
    }
    case SerializationTag::kDouble: {
      Maybe<double> number = ReadDouble();
      if (number.IsNothing()) return MaybeHandle<Object>();
      return isolate_->factory()->NewNumber(number.FromJust());
    }
    case SerializationTag::kUtf8String:
      return ReadUtf8String();
    case SerializationTag::kTwoByteString:
      return ReadTwoByteString();
    case SerializationTag::kObjectReference: {
      uint32_t id;
      if (!ReadVarint<uint32_t>().To(&id)) return MaybeHandle<Object>();
      return GetObjectWithID(id);
    }
    case SerializationTag::kBeginJSObject:
      return ReadJSObject();
    case SerializationTag::kBeginSparseJSArray:
      return ReadSparseJSArray();
    case SerializationTag::kBeginDenseJSArray:
      return ReadDenseJSArray();
    case SerializationTag::kDate:
      return ReadJSDate();
    case SerializationTag::kTrueObject:
    case SerializationTag::kFalseObject:
    case SerializationTag::kNumberObject:
    case SerializationTag::kStringObject:
      return ReadJSValue(tag);
    case SerializationTag::kRegExp:
      return ReadJSRegExp();
    case SerializationTag::kBeginJSMap:
      return ReadJSMap();
    case SerializationTag::kBeginJSSet:
      return ReadJSSet();
    default:
      return MaybeHandle<Object>();
  }
}

MaybeHandle<String> ValueDeserializer::ReadUtf8String() {
  uint32_t utf8_length;
  Vector<const uint8_t> utf8_bytes;
  if (!ReadVarint<uint32_t>().To(&utf8_length) ||
      utf8_length >
          static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
      !ReadRawBytes(utf8_length).To(&utf8_bytes))
    return MaybeHandle<String>();
  return isolate_->factory()->NewStringFromUtf8(
      Vector<const char>::cast(utf8_bytes));
}

MaybeHandle<String> ValueDeserializer::ReadTwoByteString() {
  uint32_t byte_length;
  Vector<const uint8_t> bytes;
  if (!ReadVarint<uint32_t>().To(&byte_length) ||
      byte_length >
          static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
      byte_length % sizeof(uc16) != 0 || !ReadRawBytes(byte_length).To(&bytes))
    return MaybeHandle<String>();

  // Allocate an uninitialized string so that we can do a raw memcpy into the
  // string on the heap (regardless of alignment).
  Handle<SeqTwoByteString> string;
  if (!isolate_->factory()
           ->NewRawTwoByteString(byte_length / sizeof(uc16))
           .ToHandle(&string))
    return MaybeHandle<String>();

  // Copy the bytes directly into the new string.
  // Warning: this uses host endianness.
  memcpy(string->GetChars(), bytes.begin(), bytes.length());
  return string;
}

MaybeHandle<JSObject> ValueDeserializer::ReadJSObject() {
  // If we are at the end of the stack, abort. This function may recurse.
  if (StackLimitCheck(isolate_).HasOverflowed()) return MaybeHandle<JSObject>();

  uint32_t id = next_id_++;
  HandleScope scope(isolate_);
  Handle<JSObject> object =
      isolate_->factory()->NewJSObject(isolate_->object_function());
  AddObjectWithID(id, object);

  uint32_t num_properties;
  uint32_t expected_num_properties;
  if (!ReadJSObjectProperties(object, SerializationTag::kEndJSObject)
           .To(&num_properties) ||
      !ReadVarint<uint32_t>().To(&expected_num_properties) ||
      num_properties != expected_num_properties) {
    return MaybeHandle<JSObject>();
  }

  DCHECK(HasObjectWithID(id));
  return scope.CloseAndEscape(object);
}

MaybeHandle<JSArray> ValueDeserializer::ReadSparseJSArray() {
  // If we are at the end of the stack, abort. This function may recurse.
  if (StackLimitCheck(isolate_).HasOverflowed()) return MaybeHandle<JSArray>();

  uint32_t length;
  if (!ReadVarint<uint32_t>().To(&length)) return MaybeHandle<JSArray>();

  uint32_t id = next_id_++;
  HandleScope scope(isolate_);
  Handle<JSArray> array = isolate_->factory()->NewJSArray(0);
  JSArray::SetLength(array, length);
  AddObjectWithID(id, array);

  uint32_t num_properties;
  uint32_t expected_num_properties;
  uint32_t expected_length;
  if (!ReadJSObjectProperties(array, SerializationTag::kEndSparseJSArray)
           .To(&num_properties) ||
      !ReadVarint<uint32_t>().To(&expected_num_properties) ||
      !ReadVarint<uint32_t>().To(&expected_length) ||
      num_properties != expected_num_properties || length != expected_length) {
    return MaybeHandle<JSArray>();
  }

  DCHECK(HasObjectWithID(id));
  return scope.CloseAndEscape(array);
}

MaybeHandle<JSArray> ValueDeserializer::ReadDenseJSArray() {
  // If we are at the end of the stack, abort. This function may recurse.
  if (StackLimitCheck(isolate_).HasOverflowed()) return MaybeHandle<JSArray>();

  uint32_t length;
  if (!ReadVarint<uint32_t>().To(&length)) return MaybeHandle<JSArray>();

  uint32_t id = next_id_++;
  HandleScope scope(isolate_);
  Handle<JSArray> array = isolate_->factory()->NewJSArray(
      FAST_HOLEY_ELEMENTS, length, length, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
  AddObjectWithID(id, array);

  Handle<FixedArray> elements(FixedArray::cast(array->elements()), isolate_);
  for (uint32_t i = 0; i < length; i++) {
    Handle<Object> element;
    if (!ReadObject().ToHandle(&element)) return MaybeHandle<JSArray>();
    // TODO(jbroman): Distinguish between undefined and a hole.
    if (element->IsUndefined(isolate_)) continue;
    elements->set(i, *element);
  }

  uint32_t num_properties;
  uint32_t expected_num_properties;
  uint32_t expected_length;
  if (!ReadJSObjectProperties(array, SerializationTag::kEndDenseJSArray)
           .To(&num_properties) ||
      !ReadVarint<uint32_t>().To(&expected_num_properties) ||
      !ReadVarint<uint32_t>().To(&expected_length) ||
      num_properties != expected_num_properties || length != expected_length) {
    return MaybeHandle<JSArray>();
  }

  DCHECK(HasObjectWithID(id));
  return scope.CloseAndEscape(array);
}

MaybeHandle<JSDate> ValueDeserializer::ReadJSDate() {
  double value;
  if (!ReadDouble().To(&value)) return MaybeHandle<JSDate>();
  uint32_t id = next_id_++;
  Handle<JSDate> date;
  if (!JSDate::New(isolate_->date_function(), isolate_->date_function(), value)
           .ToHandle(&date)) {
    return MaybeHandle<JSDate>();
  }
  AddObjectWithID(id, date);
  return date;
}

MaybeHandle<JSValue> ValueDeserializer::ReadJSValue(SerializationTag tag) {
  uint32_t id = next_id_++;
  Handle<JSValue> value;
  switch (tag) {
    case SerializationTag::kTrueObject:
      value = Handle<JSValue>::cast(
          isolate_->factory()->NewJSObject(isolate_->boolean_function()));
      value->set_value(isolate_->heap()->true_value());
      break;
    case SerializationTag::kFalseObject:
      value = Handle<JSValue>::cast(
          isolate_->factory()->NewJSObject(isolate_->boolean_function()));
      value->set_value(isolate_->heap()->false_value());
      break;
    case SerializationTag::kNumberObject: {
      double number;
      if (!ReadDouble().To(&number)) return MaybeHandle<JSValue>();
      value = Handle<JSValue>::cast(
          isolate_->factory()->NewJSObject(isolate_->number_function()));
      Handle<Object> number_object = isolate_->factory()->NewNumber(number);
      value->set_value(*number_object);
      break;
    }
    case SerializationTag::kStringObject: {
      Handle<String> string;
      if (!ReadUtf8String().ToHandle(&string)) return MaybeHandle<JSValue>();
      value = Handle<JSValue>::cast(
          isolate_->factory()->NewJSObject(isolate_->string_function()));
      value->set_value(*string);
      break;
    }
    default:
      UNREACHABLE();
      return MaybeHandle<JSValue>();
  }
  AddObjectWithID(id, value);
  return value;
}

MaybeHandle<JSRegExp> ValueDeserializer::ReadJSRegExp() {
  uint32_t id = next_id_++;
  Handle<String> pattern;
  uint32_t raw_flags;
  Handle<JSRegExp> regexp;
  if (!ReadUtf8String().ToHandle(&pattern) ||
      !ReadVarint<uint32_t>().To(&raw_flags) ||
      !JSRegExp::New(pattern, static_cast<JSRegExp::Flags>(raw_flags))
           .ToHandle(&regexp)) {
    return MaybeHandle<JSRegExp>();
  }
  AddObjectWithID(id, regexp);
  return regexp;
}

MaybeHandle<JSMap> ValueDeserializer::ReadJSMap() {
  // If we are at the end of the stack, abort. This function may recurse.
  if (StackLimitCheck(isolate_).HasOverflowed()) return MaybeHandle<JSMap>();

  HandleScope scope(isolate_);
  uint32_t id = next_id_++;
  Handle<JSMap> map = isolate_->factory()->NewJSMap();
  AddObjectWithID(id, map);

  Handle<JSFunction> map_set = isolate_->map_set();
  uint32_t length = 0;
  while (true) {
    SerializationTag tag;
    if (!PeekTag().To(&tag)) return MaybeHandle<JSMap>();
    if (tag == SerializationTag::kEndJSMap) {
      ConsumeTag(SerializationTag::kEndJSMap);
      break;
    }

    Handle<Object> argv[2];
    if (!ReadObject().ToHandle(&argv[0]) || !ReadObject().ToHandle(&argv[1]) ||
        Execution::Call(isolate_, map_set, map, arraysize(argv), argv)
            .is_null()) {
      return MaybeHandle<JSMap>();
    }
    length += 2;
  }

  uint32_t expected_length;
  if (!ReadVarint<uint32_t>().To(&expected_length) ||
      length != expected_length) {
    return MaybeHandle<JSMap>();
  }
  DCHECK(HasObjectWithID(id));
  return scope.CloseAndEscape(map);
}

MaybeHandle<JSSet> ValueDeserializer::ReadJSSet() {
  // If we are at the end of the stack, abort. This function may recurse.
  if (StackLimitCheck(isolate_).HasOverflowed()) return MaybeHandle<JSSet>();

  HandleScope scope(isolate_);
  uint32_t id = next_id_++;
  Handle<JSSet> set = isolate_->factory()->NewJSSet();
  AddObjectWithID(id, set);
  Handle<JSFunction> set_add = isolate_->set_add();
  uint32_t length = 0;
  while (true) {
    SerializationTag tag;
    if (!PeekTag().To(&tag)) return MaybeHandle<JSSet>();
    if (tag == SerializationTag::kEndJSSet) {
      ConsumeTag(SerializationTag::kEndJSSet);
      break;
    }

    Handle<Object> argv[1];
    if (!ReadObject().ToHandle(&argv[0]) ||
        Execution::Call(isolate_, set_add, set, arraysize(argv), argv)
            .is_null()) {
      return MaybeHandle<JSSet>();
    }
    length++;
  }

  uint32_t expected_length;
  if (!ReadVarint<uint32_t>().To(&expected_length) ||
      length != expected_length) {
    return MaybeHandle<JSSet>();
  }
  DCHECK(HasObjectWithID(id));
  return scope.CloseAndEscape(set);
}

Maybe<uint32_t> ValueDeserializer::ReadJSObjectProperties(
    Handle<JSObject> object, SerializationTag end_tag) {
  for (uint32_t num_properties = 0;; num_properties++) {
    SerializationTag tag;
    if (!PeekTag().To(&tag)) return Nothing<uint32_t>();
    if (tag == end_tag) {
      ConsumeTag(end_tag);
      return Just(num_properties);
    }

    Handle<Object> key;
    if (!ReadObject().ToHandle(&key)) return Nothing<uint32_t>();
    Handle<Object> value;
    if (!ReadObject().ToHandle(&value)) return Nothing<uint32_t>();

    bool success;
    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate_, object, key, &success, LookupIterator::OWN);
    if (!success ||
        JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, NONE)
            .is_null()) {
      return Nothing<uint32_t>();
    }
  }
}

bool ValueDeserializer::HasObjectWithID(uint32_t id) {
  return id_map_->Has(isolate_, id);
}

MaybeHandle<JSReceiver> ValueDeserializer::GetObjectWithID(uint32_t id) {
  int index = id_map_->FindEntry(isolate_, id);
  if (index == SeededNumberDictionary::kNotFound) {
    return MaybeHandle<JSReceiver>();
  }
  Object* value = id_map_->ValueAt(index);
  DCHECK(value->IsJSReceiver());
  return Handle<JSReceiver>(JSReceiver::cast(value), isolate_);
}

void ValueDeserializer::AddObjectWithID(uint32_t id,
                                        Handle<JSReceiver> object) {
  DCHECK(!HasObjectWithID(id));
  const bool used_as_prototype = false;
  Handle<SeededNumberDictionary> new_dictionary =
      SeededNumberDictionary::AtNumberPut(id_map_, id, object,
                                          used_as_prototype);

  // If the dictionary was reallocated, update the global handle.
  if (!new_dictionary.is_identical_to(id_map_)) {
    GlobalHandles::Destroy(Handle<Object>::cast(id_map_).location());
    id_map_ = Handle<SeededNumberDictionary>::cast(
        isolate_->global_handles()->Create(*new_dictionary));
  }
}

static Maybe<bool> SetPropertiesFromKeyValuePairs(Isolate* isolate,
                                                  Handle<JSObject> object,
                                                  Handle<Object>* data,
                                                  uint32_t num_properties) {
  for (unsigned i = 0; i < 2 * num_properties; i += 2) {
    Handle<Object> key = data[i];
    Handle<Object> value = data[i + 1];
    bool success;
    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate, object, key, &success, LookupIterator::OWN);
    if (!success ||
        JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, NONE)
            .is_null()) {
      return Nothing<bool>();
    }
  }
  return Just(true);
}

MaybeHandle<Object>
ValueDeserializer::ReadObjectUsingEntireBufferForLegacyFormat() {
  if (version_ > 0) return MaybeHandle<Object>();

  HandleScope scope(isolate_);
  std::vector<Handle<Object>> stack;
  while (position_ < end_) {
    SerializationTag tag;
    if (!PeekTag().To(&tag)) break;

    Handle<Object> new_object;
    switch (tag) {
      case SerializationTag::kEndJSObject: {
        ConsumeTag(SerializationTag::kEndJSObject);

        // JS Object: Read the last 2*n values from the stack and use them as
        // key-value pairs.
        uint32_t num_properties;
        if (!ReadVarint<uint32_t>().To(&num_properties) ||
            stack.size() / 2 < num_properties) {
          return MaybeHandle<Object>();
        }

        size_t begin_properties =
            stack.size() - 2 * static_cast<size_t>(num_properties);
        Handle<JSObject> js_object =
            isolate_->factory()->NewJSObject(isolate_->object_function());
        if (num_properties &&
            !SetPropertiesFromKeyValuePairs(
                 isolate_, js_object, &stack[begin_properties], num_properties)
                 .FromMaybe(false)) {
          return MaybeHandle<Object>();
        }

        stack.resize(begin_properties);
        new_object = js_object;
        break;
      }
      case SerializationTag::kEndSparseJSArray: {
        ConsumeTag(SerializationTag::kEndSparseJSArray);

        // Sparse JS Array: Read the last 2*|num_properties| from the stack.
        uint32_t num_properties;
        uint32_t length;
        if (!ReadVarint<uint32_t>().To(&num_properties) ||
            !ReadVarint<uint32_t>().To(&length) ||
            stack.size() / 2 < num_properties) {
          return MaybeHandle<Object>();
        }

        Handle<JSArray> js_array = isolate_->factory()->NewJSArray(0);
        JSArray::SetLength(js_array, length);
        size_t begin_properties =
            stack.size() - 2 * static_cast<size_t>(num_properties);
        if (num_properties &&
            !SetPropertiesFromKeyValuePairs(
                 isolate_, js_array, &stack[begin_properties], num_properties)
                 .FromMaybe(false)) {
          return MaybeHandle<Object>();
        }

        stack.resize(begin_properties);
        new_object = js_array;
        break;
      }
      case SerializationTag::kEndDenseJSArray:
        // This was already broken in Chromium, and apparently wasn't missed.
        return MaybeHandle<Object>();
      default:
        if (!ReadObject().ToHandle(&new_object)) return MaybeHandle<Object>();
        break;
    }
    stack.push_back(new_object);
  }

// Nothing remains but padding.
#ifdef DEBUG
  while (position_ < end_) {
    DCHECK(*position_++ == static_cast<uint8_t>(SerializationTag::kPadding));
  }
#endif
  position_ = end_;

  if (stack.size() != 1) return MaybeHandle<Object>();
  return scope.CloseAndEscape(stack[0]);
}

}  // namespace internal
}  // namespace v8
