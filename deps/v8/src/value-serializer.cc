// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/value-serializer.h"

#include <type_traits>

#include "src/base/logging.h"
#include "src/conversions.h"
#include "src/factory.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/transitions.h"

namespace v8 {
namespace internal {

static const uint32_t kLatestVersion = 9;
static const int kPretenureThreshold = 100 * KB;

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
  // Array buffer. byteLength:uint32_t, then raw data.
  kArrayBuffer = 'B',
  // Array buffer (transferred). transferID:uint32_t
  kArrayBufferTransfer = 't',
  // View into an array buffer.
  // subtag:ArrayBufferViewTag, byteOffset:uint32_t, byteLength:uint32_t
  // For typed arrays, byteOffset and byteLength must be divisible by the size
  // of the element.
  // Note: kArrayBufferView is special, and should have an ArrayBuffer (or an
  // ObjectReference to one) serialized just before it. This is a quirk arising
  // from the previous stack-based implementation.
  kArrayBufferView = 'V',
  // Shared array buffer (transferred). transferID:uint32_t
  kSharedArrayBufferTransfer = 'u',
};

namespace {

enum class ArrayBufferViewTag : uint8_t {
  kInt8Array = 'b',
  kUint8Array = 'B',
  kUint8ClampedArray = 'C',
  kInt16Array = 'w',
  kUint16Array = 'W',
  kInt32Array = 'd',
  kUint32Array = 'D',
  kFloat32Array = 'f',
  kFloat64Array = 'F',
  kDataView = '?',
};

}  // namespace

ValueSerializer::ValueSerializer(Isolate* isolate,
                                 v8::ValueSerializer::Delegate* delegate)
    : isolate_(isolate),
      delegate_(delegate),
      zone_(isolate->allocator()),
      id_map_(isolate->heap(), &zone_),
      array_buffer_transfer_map_(isolate->heap(), &zone_) {}

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

void ValueSerializer::WriteRawBytes(const void* source, size_t length) {
  const uint8_t* begin = reinterpret_cast<const uint8_t*>(source);
  buffer_.insert(buffer_.end(), begin, begin + length);
}

uint8_t* ValueSerializer::ReserveRawBytes(size_t bytes) {
  if (!bytes) return nullptr;
  auto old_size = buffer_.size();
  buffer_.resize(buffer_.size() + bytes);
  return &buffer_[old_size];
}

void ValueSerializer::WriteUint32(uint32_t value) {
  WriteVarint<uint32_t>(value);
}

void ValueSerializer::WriteUint64(uint64_t value) {
  WriteVarint<uint64_t>(value);
}

void ValueSerializer::TransferArrayBuffer(uint32_t transfer_id,
                                          Handle<JSArrayBuffer> array_buffer) {
  DCHECK(!array_buffer_transfer_map_.Find(array_buffer));
  array_buffer_transfer_map_.Set(array_buffer, transfer_id);
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
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE: {
      // Despite being JSReceivers, these have their wrapped buffer serialized
      // first. That makes this logic a little quirky, because it needs to
      // happen before we assign object IDs.
      // TODO(jbroman): It may be possible to avoid materializing a typed
      // array's buffer here.
      Handle<JSArrayBufferView> view = Handle<JSArrayBufferView>::cast(object);
      if (!id_map_.Find(view)) {
        Handle<JSArrayBuffer> buffer(
            view->IsJSTypedArray()
                ? Handle<JSTypedArray>::cast(view)->GetBuffer()
                : handle(JSArrayBuffer::cast(view->buffer()), isolate_));
        if (!WriteJSReceiver(buffer).FromMaybe(false)) return Nothing<bool>();
      }
      return WriteJSReceiver(view);
    }
    default:
      if (object->IsString()) {
        WriteString(Handle<String>::cast(object));
        return Just(true);
      } else if (object->IsJSReceiver()) {
        return WriteJSReceiver(Handle<JSReceiver>::cast(object));
      } else {
        ThrowDataCloneError(MessageTemplate::kDataCloneError, object);
        return Nothing<bool>();
      }
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
  if (receiver->IsCallable() || (instance_type <= LAST_SPECIAL_RECEIVER_TYPE &&
                                 instance_type != JS_SPECIAL_API_OBJECT_TYPE)) {
    ThrowDataCloneError(MessageTemplate::kDataCloneError, receiver);
    return Nothing<bool>();
  }

  // If we are at the end of the stack, abort. This function may recurse.
  STACK_CHECK(isolate_, Nothing<bool>());

  HandleScope scope(isolate_);
  switch (instance_type) {
    case JS_ARRAY_TYPE:
      return WriteJSArray(Handle<JSArray>::cast(receiver));
    case JS_OBJECT_TYPE:
    case JS_API_OBJECT_TYPE: {
      Handle<JSObject> js_object = Handle<JSObject>::cast(receiver);
      return js_object->GetInternalFieldCount() ? WriteHostObject(js_object)
                                                : WriteJSObject(js_object);
    }
    case JS_SPECIAL_API_OBJECT_TYPE:
      return WriteHostObject(Handle<JSObject>::cast(receiver));
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
    case JS_ARRAY_BUFFER_TYPE:
      return WriteJSArrayBuffer(JSArrayBuffer::cast(*receiver));
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
      return WriteJSArrayBufferView(JSArrayBufferView::cast(*receiver));
    default:
      ThrowDataCloneError(MessageTemplate::kDataCloneError, receiver);
      return Nothing<bool>();
  }
  return Nothing<bool>();
}

Maybe<bool> ValueSerializer::WriteJSObject(Handle<JSObject> object) {
  DCHECK_GT(object->map()->instance_type(), LAST_CUSTOM_ELEMENTS_RECEIVER);
  const bool can_serialize_fast =
      object->HasFastProperties() && object->elements()->length() == 0;
  if (!can_serialize_fast) return WriteJSObjectSlow(object);

  Handle<Map> map(object->map(), isolate_);
  WriteTag(SerializationTag::kBeginJSObject);

  // Write out fast properties as long as they are only data properties and the
  // map doesn't change.
  uint32_t properties_written = 0;
  bool map_changed = false;
  for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
    Handle<Name> key(map->instance_descriptors()->GetKey(i), isolate_);
    if (!key->IsString()) continue;
    PropertyDetails details = map->instance_descriptors()->GetDetails(i);
    if (details.IsDontEnum()) continue;

    Handle<Object> value;
    if (V8_LIKELY(!map_changed)) map_changed = *map == object->map();
    if (V8_LIKELY(!map_changed && details.type() == DATA)) {
      FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
      value = JSObject::FastPropertyAt(object, details.representation(),
                                       field_index);
    } else {
      // This logic should essentially match WriteJSObjectPropertiesSlow.
      // If the property is no longer found, do not serialize it.
      // This could happen if a getter deleted the property.
      LookupIterator it(isolate_, object, key, LookupIterator::OWN);
      if (!it.IsFound()) continue;
      if (!Object::GetProperty(&it).ToHandle(&value)) return Nothing<bool>();
    }

    if (!WriteObject(key).FromMaybe(false) ||
        !WriteObject(value).FromMaybe(false)) {
      return Nothing<bool>();
    }
    properties_written++;
  }

  WriteTag(SerializationTag::kEndJSObject);
  WriteVarint<uint32_t>(properties_written);
  return Just(true);
}

Maybe<bool> ValueSerializer::WriteJSObjectSlow(Handle<JSObject> object) {
  WriteTag(SerializationTag::kBeginJSObject);
  Handle<FixedArray> keys;
  uint32_t properties_written;
  if (!KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                               ENUMERABLE_STRINGS)
           .ToHandle(&keys) ||
      !WriteJSObjectPropertiesSlow(object, keys).To(&properties_written)) {
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
    uint32_t i = 0;

    // Fast paths. Note that FAST_ELEMENTS in particular can bail due to the
    // structure of the elements changing.
    switch (array->GetElementsKind()) {
      case FAST_SMI_ELEMENTS: {
        Handle<FixedArray> elements(FixedArray::cast(array->elements()),
                                    isolate_);
        for (; i < length; i++) WriteSmi(Smi::cast(elements->get(i)));
        break;
      }
      case FAST_DOUBLE_ELEMENTS: {
        Handle<FixedDoubleArray> elements(
            FixedDoubleArray::cast(array->elements()), isolate_);
        for (; i < length; i++) {
          WriteTag(SerializationTag::kDouble);
          WriteDouble(elements->get_scalar(i));
        }
        break;
      }
      case FAST_ELEMENTS: {
        Handle<Object> old_length(array->length(), isolate_);
        for (; i < length; i++) {
          if (array->length() != *old_length ||
              array->GetElementsKind() != FAST_ELEMENTS) {
            // Fall back to slow path.
            break;
          }
          Handle<Object> element(FixedArray::cast(array->elements())->get(i),
                                 isolate_);
          if (!WriteObject(element).FromMaybe(false)) return Nothing<bool>();
        }
        break;
      }
      default:
        break;
    }

    // If there are elements remaining, serialize them slowly.
    for (; i < length; i++) {
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
    if (!WriteJSObjectPropertiesSlow(array, keys).To(&properties_written)) {
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
        !WriteJSObjectPropertiesSlow(array, keys).To(&properties_written)) {
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
    ThrowDataCloneError(MessageTemplate::kDataCloneError, value);
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

Maybe<bool> ValueSerializer::WriteJSArrayBuffer(JSArrayBuffer* array_buffer) {
  uint32_t* transfer_entry = array_buffer_transfer_map_.Find(array_buffer);
  if (transfer_entry) {
    DCHECK(array_buffer->was_neutered() || array_buffer->is_shared());
    WriteTag(array_buffer->is_shared()
                 ? SerializationTag::kSharedArrayBufferTransfer
                 : SerializationTag::kArrayBufferTransfer);
    WriteVarint(*transfer_entry);
    return Just(true);
  }

  if (array_buffer->is_shared()) {
    ThrowDataCloneError(
        MessageTemplate::kDataCloneErrorSharedArrayBufferNotTransferred);
    return Nothing<bool>();
  }
  if (array_buffer->was_neutered()) {
    ThrowDataCloneError(MessageTemplate::kDataCloneErrorNeuteredArrayBuffer);
    return Nothing<bool>();
  }
  double byte_length = array_buffer->byte_length()->Number();
  if (byte_length > std::numeric_limits<uint32_t>::max()) {
    ThrowDataCloneError(MessageTemplate::kDataCloneError, handle(array_buffer));
    return Nothing<bool>();
  }
  WriteTag(SerializationTag::kArrayBuffer);
  WriteVarint<uint32_t>(byte_length);
  WriteRawBytes(array_buffer->backing_store(), byte_length);
  return Just(true);
}

Maybe<bool> ValueSerializer::WriteJSArrayBufferView(JSArrayBufferView* view) {
  WriteTag(SerializationTag::kArrayBufferView);
  ArrayBufferViewTag tag = ArrayBufferViewTag::kInt8Array;
  if (view->IsJSTypedArray()) {
    switch (JSTypedArray::cast(view)->type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    tag = ArrayBufferViewTag::k##Type##Array;           \
    break;
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    }
  } else {
    DCHECK(view->IsJSDataView());
    tag = ArrayBufferViewTag::kDataView;
  }
  WriteVarint(static_cast<uint8_t>(tag));
  WriteVarint(NumberToUint32(view->byte_offset()));
  WriteVarint(NumberToUint32(view->byte_length()));
  return Just(true);
}

Maybe<bool> ValueSerializer::WriteHostObject(Handle<JSObject> object) {
  if (!delegate_) {
    isolate_->Throw(*isolate_->factory()->NewError(
        isolate_->error_function(), MessageTemplate::kDataCloneError, object));
    return Nothing<bool>();
  }
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  Maybe<bool> result =
      delegate_->WriteHostObject(v8_isolate, Utils::ToLocal(object));
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate_, Nothing<bool>());
  DCHECK(!result.IsNothing());
  return result;
}

Maybe<uint32_t> ValueSerializer::WriteJSObjectPropertiesSlow(
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

void ValueSerializer::ThrowDataCloneError(
    MessageTemplate::Template template_index) {
  return ThrowDataCloneError(template_index,
                             isolate_->factory()->empty_string());
}

void ValueSerializer::ThrowDataCloneError(
    MessageTemplate::Template template_index, Handle<Object> arg0) {
  Handle<String> message =
      MessageTemplate::FormatMessage(isolate_, template_index, arg0);
  if (delegate_) {
    delegate_->ThrowDataCloneError(Utils::ToLocal(message));
  } else {
    isolate_->Throw(
        *isolate_->factory()->NewError(isolate_->error_function(), message));
  }
  if (isolate_->has_scheduled_exception()) {
    isolate_->PromoteScheduledException();
  }
}

ValueDeserializer::ValueDeserializer(Isolate* isolate,
                                     Vector<const uint8_t> data,
                                     v8::ValueDeserializer::Delegate* delegate)
    : isolate_(isolate),
      delegate_(delegate),
      position_(data.start()),
      end_(data.start() + data.length()),
      pretenure_(data.length() > kPretenureThreshold ? TENURED : NOT_TENURED),
      id_map_(Handle<FixedArray>::cast(isolate->global_handles()->Create(
          isolate_->heap()->empty_fixed_array()))) {}

ValueDeserializer::~ValueDeserializer() {
  GlobalHandles::Destroy(Handle<Object>::cast(id_map_).location());

  Handle<Object> transfer_map_handle;
  if (array_buffer_transfer_map_.ToHandle(&transfer_map_handle)) {
    GlobalHandles::Destroy(transfer_map_handle.location());
  }
}

Maybe<bool> ValueDeserializer::ReadHeader() {
  if (position_ < end_ &&
      *position_ == static_cast<uint8_t>(SerializationTag::kVersion)) {
    ReadTag().ToChecked();
    if (!ReadVarint<uint32_t>().To(&version_) || version_ > kLatestVersion) {
      isolate_->Throw(*isolate_->factory()->NewError(
          MessageTemplate::kDataCloneDeserializationVersionError));
      return Nothing<bool>();
    }
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
      value |= static_cast<T>(byte & 0x7f) << shift;
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

bool ValueDeserializer::ReadUint32(uint32_t* value) {
  return ReadVarint<uint32_t>().To(value);
}

bool ValueDeserializer::ReadUint64(uint64_t* value) {
  return ReadVarint<uint64_t>().To(value);
}

bool ValueDeserializer::ReadDouble(double* value) {
  return ReadDouble().To(value);
}

bool ValueDeserializer::ReadRawBytes(size_t length, const void** data) {
  if (length > static_cast<size_t>(end_ - position_)) return false;
  *data = position_;
  position_ += length;
  return true;
}

void ValueDeserializer::TransferArrayBuffer(
    uint32_t transfer_id, Handle<JSArrayBuffer> array_buffer) {
  if (array_buffer_transfer_map_.is_null()) {
    array_buffer_transfer_map_ =
        Handle<SeededNumberDictionary>::cast(isolate_->global_handles()->Create(
            *SeededNumberDictionary::New(isolate_, 0)));
  }
  Handle<SeededNumberDictionary> dictionary =
      array_buffer_transfer_map_.ToHandleChecked();
  const bool used_as_prototype = false;
  Handle<SeededNumberDictionary> new_dictionary =
      SeededNumberDictionary::AtNumberPut(dictionary, transfer_id, array_buffer,
                                          used_as_prototype);
  if (!new_dictionary.is_identical_to(dictionary)) {
    GlobalHandles::Destroy(Handle<Object>::cast(dictionary).location());
    array_buffer_transfer_map_ = Handle<SeededNumberDictionary>::cast(
        isolate_->global_handles()->Create(*new_dictionary));
  }
}

MaybeHandle<Object> ValueDeserializer::ReadObject() {
  MaybeHandle<Object> result = ReadObjectInternal();

  // ArrayBufferView is special in that it consumes the value before it, even
  // after format version 0.
  Handle<Object> object;
  SerializationTag tag;
  if (result.ToHandle(&object) && V8_UNLIKELY(object->IsJSArrayBuffer()) &&
      PeekTag().To(&tag) && tag == SerializationTag::kArrayBufferView) {
    ConsumeTag(SerializationTag::kArrayBufferView);
    result = ReadJSArrayBufferView(Handle<JSArrayBuffer>::cast(object));
  }

  if (result.is_null() && !isolate_->has_pending_exception()) {
    isolate_->Throw(*isolate_->factory()->NewError(
        MessageTemplate::kDataCloneDeserializationError));
  }

  return result;
}

MaybeHandle<Object> ValueDeserializer::ReadObjectInternal() {
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
      return isolate_->factory()->NewNumberFromInt(number.FromJust(),
                                                   pretenure_);
    }
    case SerializationTag::kUint32: {
      Maybe<uint32_t> number = ReadVarint<uint32_t>();
      if (number.IsNothing()) return MaybeHandle<Object>();
      return isolate_->factory()->NewNumberFromUint(number.FromJust(),
                                                    pretenure_);
    }
    case SerializationTag::kDouble: {
      Maybe<double> number = ReadDouble();
      if (number.IsNothing()) return MaybeHandle<Object>();
      return isolate_->factory()->NewNumber(number.FromJust(), pretenure_);
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
    case SerializationTag::kArrayBuffer:
      return ReadJSArrayBuffer();
    case SerializationTag::kArrayBufferTransfer: {
      const bool is_shared = false;
      return ReadTransferredJSArrayBuffer(is_shared);
    }
    case SerializationTag::kSharedArrayBufferTransfer: {
      const bool is_shared = true;
      return ReadTransferredJSArrayBuffer(is_shared);
    }
    default:
      // TODO(jbroman): Introduce an explicit tag for host objects to avoid
      // having to treat every unknown tag as a potential host object.
      position_--;
      return ReadHostObject();
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
      Vector<const char>::cast(utf8_bytes), pretenure_);
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
           ->NewRawTwoByteString(byte_length / sizeof(uc16), pretenure_)
           .ToHandle(&string))
    return MaybeHandle<String>();

  // Copy the bytes directly into the new string.
  // Warning: this uses host endianness.
  memcpy(string->GetChars(), bytes.begin(), bytes.length());
  return string;
}

bool ValueDeserializer::ReadExpectedString(Handle<String> expected) {
  // In the case of failure, the position in the stream is reset.
  const uint8_t* original_position = position_;

  SerializationTag tag;
  uint32_t byte_length;
  Vector<const uint8_t> bytes;
  if (!ReadTag().To(&tag) || !ReadVarint<uint32_t>().To(&byte_length) ||
      byte_length >
          static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
      !ReadRawBytes(byte_length).To(&bytes)) {
    position_ = original_position;
    return false;
  }

  expected = String::Flatten(expected);
  DisallowHeapAllocation no_gc;
  String::FlatContent flat = expected->GetFlatContent();

  // If the bytes are verbatim what is in the flattened string, then the string
  // is successfully consumed.
  if (tag == SerializationTag::kUtf8String && flat.IsOneByte()) {
    Vector<const uint8_t> chars = flat.ToOneByteVector();
    if (byte_length == chars.length() &&
        String::IsAscii(chars.begin(), chars.length()) &&
        memcmp(bytes.begin(), chars.begin(), byte_length) == 0) {
      return true;
    }
  } else if (tag == SerializationTag::kTwoByteString && flat.IsTwoByte()) {
    Vector<const uc16> chars = flat.ToUC16Vector();
    if (byte_length == static_cast<unsigned>(chars.length()) * sizeof(uc16) &&
        memcmp(bytes.begin(), chars.begin(), byte_length) == 0) {
      return true;
    }
  }

  position_ = original_position;
  return false;
}

MaybeHandle<JSObject> ValueDeserializer::ReadJSObject() {
  // If we are at the end of the stack, abort. This function may recurse.
  STACK_CHECK(isolate_, MaybeHandle<JSObject>());

  uint32_t id = next_id_++;
  HandleScope scope(isolate_);
  Handle<JSObject> object =
      isolate_->factory()->NewJSObject(isolate_->object_function(), pretenure_);
  AddObjectWithID(id, object);

  uint32_t num_properties;
  uint32_t expected_num_properties;
  if (!ReadJSObjectProperties(object, SerializationTag::kEndJSObject, true)
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
  STACK_CHECK(isolate_, MaybeHandle<JSArray>());

  uint32_t length;
  if (!ReadVarint<uint32_t>().To(&length)) return MaybeHandle<JSArray>();

  uint32_t id = next_id_++;
  HandleScope scope(isolate_);
  Handle<JSArray> array = isolate_->factory()->NewJSArray(
      0, TERMINAL_FAST_ELEMENTS_KIND, pretenure_);
  JSArray::SetLength(array, length);
  AddObjectWithID(id, array);

  uint32_t num_properties;
  uint32_t expected_num_properties;
  uint32_t expected_length;
  if (!ReadJSObjectProperties(array, SerializationTag::kEndSparseJSArray, false)
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
  STACK_CHECK(isolate_, MaybeHandle<JSArray>());

  uint32_t length;
  if (!ReadVarint<uint32_t>().To(&length)) return MaybeHandle<JSArray>();

  uint32_t id = next_id_++;
  HandleScope scope(isolate_);
  Handle<JSArray> array = isolate_->factory()->NewJSArray(
      FAST_HOLEY_ELEMENTS, length, length, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE,
      pretenure_);
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
  if (!ReadJSObjectProperties(array, SerializationTag::kEndDenseJSArray, false)
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
      value = Handle<JSValue>::cast(isolate_->factory()->NewJSObject(
          isolate_->boolean_function(), pretenure_));
      value->set_value(isolate_->heap()->true_value());
      break;
    case SerializationTag::kFalseObject:
      value = Handle<JSValue>::cast(isolate_->factory()->NewJSObject(
          isolate_->boolean_function(), pretenure_));
      value->set_value(isolate_->heap()->false_value());
      break;
    case SerializationTag::kNumberObject: {
      double number;
      if (!ReadDouble().To(&number)) return MaybeHandle<JSValue>();
      value = Handle<JSValue>::cast(isolate_->factory()->NewJSObject(
          isolate_->number_function(), pretenure_));
      Handle<Object> number_object =
          isolate_->factory()->NewNumber(number, pretenure_);
      value->set_value(*number_object);
      break;
    }
    case SerializationTag::kStringObject: {
      Handle<String> string;
      if (!ReadUtf8String().ToHandle(&string)) return MaybeHandle<JSValue>();
      value = Handle<JSValue>::cast(isolate_->factory()->NewJSObject(
          isolate_->string_function(), pretenure_));
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
  STACK_CHECK(isolate_, MaybeHandle<JSMap>());

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
  STACK_CHECK(isolate_, MaybeHandle<JSSet>());

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

MaybeHandle<JSArrayBuffer> ValueDeserializer::ReadJSArrayBuffer() {
  uint32_t id = next_id_++;
  uint32_t byte_length;
  Vector<const uint8_t> bytes;
  if (!ReadVarint<uint32_t>().To(&byte_length) ||
      byte_length > static_cast<size_t>(end_ - position_)) {
    return MaybeHandle<JSArrayBuffer>();
  }
  const bool should_initialize = false;
  Handle<JSArrayBuffer> array_buffer =
      isolate_->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, pretenure_);
  JSArrayBuffer::SetupAllocatingData(array_buffer, isolate_, byte_length,
                                     should_initialize);
  memcpy(array_buffer->backing_store(), position_, byte_length);
  position_ += byte_length;
  AddObjectWithID(id, array_buffer);
  return array_buffer;
}

MaybeHandle<JSArrayBuffer> ValueDeserializer::ReadTransferredJSArrayBuffer(
    bool is_shared) {
  uint32_t id = next_id_++;
  uint32_t transfer_id;
  Handle<SeededNumberDictionary> transfer_map;
  if (!ReadVarint<uint32_t>().To(&transfer_id) ||
      !array_buffer_transfer_map_.ToHandle(&transfer_map)) {
    return MaybeHandle<JSArrayBuffer>();
  }
  int index = transfer_map->FindEntry(isolate_, transfer_id);
  if (index == SeededNumberDictionary::kNotFound) {
    return MaybeHandle<JSArrayBuffer>();
  }
  Handle<JSArrayBuffer> array_buffer(
      JSArrayBuffer::cast(transfer_map->ValueAt(index)), isolate_);
  DCHECK_EQ(is_shared, array_buffer->is_shared());
  AddObjectWithID(id, array_buffer);
  return array_buffer;
}

MaybeHandle<JSArrayBufferView> ValueDeserializer::ReadJSArrayBufferView(
    Handle<JSArrayBuffer> buffer) {
  uint32_t buffer_byte_length = NumberToUint32(buffer->byte_length());
  uint8_t tag = 0;
  uint32_t byte_offset = 0;
  uint32_t byte_length = 0;
  if (!ReadVarint<uint8_t>().To(&tag) ||
      !ReadVarint<uint32_t>().To(&byte_offset) ||
      !ReadVarint<uint32_t>().To(&byte_length) ||
      byte_offset > buffer_byte_length ||
      byte_length > buffer_byte_length - byte_offset) {
    return MaybeHandle<JSArrayBufferView>();
  }
  uint32_t id = next_id_++;
  ExternalArrayType external_array_type = kExternalInt8Array;
  unsigned element_size = 0;
  switch (static_cast<ArrayBufferViewTag>(tag)) {
    case ArrayBufferViewTag::kDataView: {
      Handle<JSDataView> data_view =
          isolate_->factory()->NewJSDataView(buffer, byte_offset, byte_length);
      AddObjectWithID(id, data_view);
      return data_view;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case ArrayBufferViewTag::k##Type##Array:              \
    external_array_type = kExternal##Type##Array;       \
    element_size = size;                                \
    break;
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  if (byte_offset % element_size != 0 || byte_length % element_size != 0) {
    return MaybeHandle<JSArrayBufferView>();
  }
  Handle<JSTypedArray> typed_array = isolate_->factory()->NewJSTypedArray(
      external_array_type, buffer, byte_offset, byte_length / element_size,
      pretenure_);
  AddObjectWithID(id, typed_array);
  return typed_array;
}

MaybeHandle<JSObject> ValueDeserializer::ReadHostObject() {
  if (!delegate_) return MaybeHandle<JSObject>();
  STACK_CHECK(isolate_, MaybeHandle<JSObject>());
  uint32_t id = next_id_++;
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  v8::Local<v8::Object> object;
  if (!delegate_->ReadHostObject(v8_isolate).ToLocal(&object)) {
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate_, JSObject);
    return MaybeHandle<JSObject>();
  }
  Handle<JSObject> js_object =
      Handle<JSObject>::cast(Utils::OpenHandle(*object));
  AddObjectWithID(id, js_object);
  return js_object;
}

// Copies a vector of property values into an object, given the map that should
// be used.
static void CommitProperties(Handle<JSObject> object, Handle<Map> map,
                             const std::vector<Handle<Object>>& properties) {
  JSObject::AllocateStorageForMap(object, map);
  DCHECK(!object->map()->is_dictionary_map());

  DisallowHeapAllocation no_gc;
  DescriptorArray* descriptors = object->map()->instance_descriptors();
  for (unsigned i = 0; i < properties.size(); i++) {
    object->WriteToField(i, descriptors->GetDetails(i), *properties[i]);
  }
}

Maybe<uint32_t> ValueDeserializer::ReadJSObjectProperties(
    Handle<JSObject> object, SerializationTag end_tag,
    bool can_use_transitions) {
  uint32_t num_properties = 0;

  // Fast path (following map transitions).
  if (can_use_transitions) {
    bool transitioning = true;
    Handle<Map> map(object->map(), isolate_);
    DCHECK(!map->is_dictionary_map());
    DCHECK(map->instance_descriptors()->IsEmpty());
    std::vector<Handle<Object>> properties;
    properties.reserve(8);

    while (transitioning) {
      // If there are no more properties, finish.
      SerializationTag tag;
      if (!PeekTag().To(&tag)) return Nothing<uint32_t>();
      if (tag == end_tag) {
        ConsumeTag(end_tag);
        CommitProperties(object, map, properties);
        CHECK_LT(properties.size(), std::numeric_limits<uint32_t>::max());
        return Just(static_cast<uint32_t>(properties.size()));
      }

      // Determine the key to be used and the target map to transition to, if
      // possible. Transitioning may abort if the key is not a string, or if no
      // transition was found.
      Handle<Object> key;
      Handle<Map> target;
      Handle<String> expected_key = TransitionArray::ExpectedTransitionKey(map);
      if (!expected_key.is_null() && ReadExpectedString(expected_key)) {
        key = expected_key;
        target = TransitionArray::ExpectedTransitionTarget(map);
      } else {
        if (!ReadObject().ToHandle(&key)) return Nothing<uint32_t>();
        if (key->IsString()) {
          key =
              isolate_->factory()->InternalizeString(Handle<String>::cast(key));
          target = TransitionArray::FindTransitionToField(
              map, Handle<String>::cast(key));
          transitioning = !target.is_null();
        } else {
          transitioning = false;
        }
      }

      // Read the value that corresponds to it.
      Handle<Object> value;
      if (!ReadObject().ToHandle(&value)) return Nothing<uint32_t>();

      // If still transitioning and the value fits the field representation
      // (though generalization may be required), store the property value so
      // that we can copy them all at once. Otherwise, stop transitioning.
      if (transitioning) {
        int descriptor = static_cast<int>(properties.size());
        PropertyDetails details =
            target->instance_descriptors()->GetDetails(descriptor);
        Representation expected_representation = details.representation();
        if (value->FitsRepresentation(expected_representation)) {
          if (expected_representation.IsHeapObject() &&
              !target->instance_descriptors()
                   ->GetFieldType(descriptor)
                   ->NowContains(value)) {
            Handle<FieldType> value_type =
                value->OptimalType(isolate_, expected_representation);
            Map::GeneralizeFieldType(target, descriptor,
                                     expected_representation, value_type);
          }
          DCHECK(target->instance_descriptors()
                     ->GetFieldType(descriptor)
                     ->NowContains(value));
          properties.push_back(value);
          map = target;
          continue;
        } else {
          transitioning = false;
        }
      }

      // Fell out of transitioning fast path. Commit the properties gathered so
      // far, and then start setting properties slowly instead.
      DCHECK(!transitioning);
      CHECK_LT(properties.size(), std::numeric_limits<uint32_t>::max());
      CommitProperties(object, map, properties);
      num_properties = static_cast<uint32_t>(properties.size());

      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate_, object, key, &success, LookupIterator::OWN);
      if (!success ||
          JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, NONE)
              .is_null()) {
        return Nothing<uint32_t>();
      }
      num_properties++;
    }

    // At this point, transitioning should be done, but at least one property
    // should have been written (in the zero-property case, there is an early
    // return).
    DCHECK(!transitioning);
    DCHECK_GE(num_properties, 1u);
  }

  // Slow path.
  for (;; num_properties++) {
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
  return id < static_cast<unsigned>(id_map_->length()) &&
         !id_map_->get(id)->IsTheHole(isolate_);
}

MaybeHandle<JSReceiver> ValueDeserializer::GetObjectWithID(uint32_t id) {
  if (id >= static_cast<unsigned>(id_map_->length())) {
    return MaybeHandle<JSReceiver>();
  }
  Object* value = id_map_->get(id);
  if (value->IsTheHole(isolate_)) return MaybeHandle<JSReceiver>();
  DCHECK(value->IsJSReceiver());
  return Handle<JSReceiver>(JSReceiver::cast(value), isolate_);
}

void ValueDeserializer::AddObjectWithID(uint32_t id,
                                        Handle<JSReceiver> object) {
  DCHECK(!HasObjectWithID(id));
  Handle<FixedArray> new_array = FixedArray::SetAndGrow(id_map_, id, object);

  // If the dictionary was reallocated, update the global handle.
  if (!new_array.is_identical_to(id_map_)) {
    GlobalHandles::Destroy(Handle<Object>::cast(id_map_).location());
    id_map_ = Handle<FixedArray>::cast(
        isolate_->global_handles()->Create(*new_array));
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
  DCHECK_EQ(version_, 0);
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
        Handle<JSObject> js_object = isolate_->factory()->NewJSObject(
            isolate_->object_function(), pretenure_);
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

        Handle<JSArray> js_array = isolate_->factory()->NewJSArray(
            0, TERMINAL_FAST_ELEMENTS_KIND, pretenure_);
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
      case SerializationTag::kEndDenseJSArray: {
        // This was already broken in Chromium, and apparently wasn't missed.
        isolate_->Throw(*isolate_->factory()->NewError(
            MessageTemplate::kDataCloneDeserializationError));
        return MaybeHandle<Object>();
      }
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

  if (stack.size() != 1) {
    isolate_->Throw(*isolate_->factory()->NewError(
        MessageTemplate::kDataCloneDeserializationError));
    return MaybeHandle<Object>();
  }
  return scope.CloseAndEscape(stack[0]);
}

}  // namespace internal
}  // namespace v8
