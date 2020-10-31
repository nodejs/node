/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_PROTOZERO_FIELD_H_
#define INCLUDE_PERFETTO_PROTOZERO_FIELD_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/contiguous_memory_range.h"
#include "perfetto/protozero/proto_utils.h"

namespace protozero {

struct ConstBytes {
  std::string ToStdString() const {
    return std::string(reinterpret_cast<const char*>(data), size);
  }

  const uint8_t* data;
  size_t size;
};

struct ConstChars {
  // Allow implicit conversion to perfetto's base::StringView without depending
  // on perfetto/base or viceversa.
  static constexpr bool kConvertibleToStringView = true;
  std::string ToStdString() const { return std::string(data, size); }

  const char* data;
  size_t size;
};

// A protobuf field decoded by the protozero proto decoders. It exposes
// convenience accessors with minimal debug checks.
// This class is used both by the iterator-based ProtoDecoder and by the
// one-shot TypedProtoDecoder.
// If the field is not valid the accessors consistently return zero-integers or
// null strings.
class Field {
 public:
  bool valid() const { return id_ != 0; }
  uint16_t id() const { return id_; }
  explicit operator bool() const { return valid(); }

  proto_utils::ProtoWireType type() const {
    auto res = static_cast<proto_utils::ProtoWireType>(type_);
    PERFETTO_DCHECK(res == proto_utils::ProtoWireType::kVarInt ||
                    res == proto_utils::ProtoWireType::kLengthDelimited ||
                    res == proto_utils::ProtoWireType::kFixed32 ||
                    res == proto_utils::ProtoWireType::kFixed64);
    return res;
  }

  bool as_bool() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt);
    return static_cast<bool>(int_value_);
  }

  uint32_t as_uint32() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt ||
                    type() == proto_utils::ProtoWireType::kFixed32);
    return static_cast<uint32_t>(int_value_);
  }

  int32_t as_int32() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt ||
                    type() == proto_utils::ProtoWireType::kFixed32);
    return static_cast<int32_t>(int_value_);
  }

  int32_t as_sint32() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt);
    return proto_utils::ZigZagDecode(static_cast<uint32_t>(int_value_));
  }

  uint64_t as_uint64() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt ||
                    type() == proto_utils::ProtoWireType::kFixed32 ||
                    type() == proto_utils::ProtoWireType::kFixed64);
    return int_value_;
  }

  int64_t as_int64() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt ||
                    type() == proto_utils::ProtoWireType::kFixed32 ||
                    type() == proto_utils::ProtoWireType::kFixed64);
    return static_cast<int64_t>(int_value_);
  }

  int64_t as_sint64() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kVarInt);
    return proto_utils::ZigZagDecode(static_cast<uint64_t>(int_value_));
  }

  float as_float() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kFixed32);
    float res;
    uint32_t value32 = static_cast<uint32_t>(int_value_);
    memcpy(&res, &value32, sizeof(res));
    return res;
  }

  double as_double() const {
    PERFETTO_DCHECK(!valid() || type() == proto_utils::ProtoWireType::kFixed64);
    double res;
    memcpy(&res, &int_value_, sizeof(res));
    return res;
  }

  ConstChars as_string() const {
    PERFETTO_DCHECK(!valid() ||
                    type() == proto_utils::ProtoWireType::kLengthDelimited);
    return ConstChars{reinterpret_cast<const char*>(data()), size_};
  }

  std::string as_std_string() const { return as_string().ToStdString(); }

  ConstBytes as_bytes() const {
    PERFETTO_DCHECK(!valid() ||
                    type() == proto_utils::ProtoWireType::kLengthDelimited);
    return ConstBytes{data(), size_};
  }

  const uint8_t* data() const {
    PERFETTO_DCHECK(!valid() ||
                    type() == proto_utils::ProtoWireType::kLengthDelimited);
    return reinterpret_cast<const uint8_t*>(int_value_);
  }

  size_t size() const {
    PERFETTO_DCHECK(!valid() ||
                    type() == proto_utils::ProtoWireType::kLengthDelimited);
    return size_;
  }

  uint64_t raw_int_value() const { return int_value_; }

  void initialize(uint16_t id,
                  uint8_t type,
                  uint64_t int_value,
                  uint32_t size) {
    id_ = id;
    type_ = type;
    int_value_ = int_value;
    size_ = size;
  }

  // For use with templates. This is used by RepeatedFieldIterator::operator*().
  void get(bool* val) const { *val = as_bool(); }
  void get(uint32_t* val) const { *val = as_uint32(); }
  void get(int32_t* val) const { *val = as_int32(); }
  void get(uint64_t* val) const { *val = as_uint64(); }
  void get(int64_t* val) const { *val = as_int64(); }
  void get(float* val) const { *val = as_float(); }
  void get(double* val) const { *val = as_double(); }
  void get(std::string* val) const { *val = as_std_string(); }
  void get(ConstChars* val) const { *val = as_string(); }
  void get(ConstBytes* val) const { *val = as_bytes(); }
  void get_signed(int32_t* val) const { *val = as_sint32(); }
  void get_signed(int64_t* val) const { *val = as_sint64(); }

  // For enum types.
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
  void get(T* val) const {
    *val = static_cast<T>(as_int32());
  }

  // Serializes the field back into a proto-encoded byte stream and appends it
  // to |dst|. |dst| is resized accordingly.
  void SerializeAndAppendTo(std::string* dst) const;

  // Serializes the field back into a proto-encoded byte stream and appends it
  // to |dst|. |dst| is resized accordingly.
  void SerializeAndAppendTo(std::vector<uint8_t>* dst) const;

 private:
  template <typename Container>
  void SerializeAndAppendToInternal(Container* dst) const;

  // Fields are deliberately not initialized to keep the class trivially
  // constructible. It makes a large perf difference for ProtoDecoder.

  uint64_t int_value_;  // In kLengthDelimited this contains the data() addr.
  uint32_t size_;       // Only valid when when type == kLengthDelimited.
  uint16_t id_;         // Proto field ordinal.
  uint8_t type_;        // proto_utils::ProtoWireType.
};

// The Field struct is used in a lot of perf-sensitive contexts.
static_assert(sizeof(Field) == 16, "Field struct too big");

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_FIELD_H_
