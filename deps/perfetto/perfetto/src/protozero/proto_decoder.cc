/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "perfetto/protozero/proto_decoder.h"

#include <string.h>
#include <limits>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/proto_utils.h"

namespace protozero {

using namespace proto_utils;

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Unimplemented for big endian archs.
#endif

namespace {

struct ParseFieldResult {
  enum ParseResult { kAbort, kSkip, kOk };
  ParseResult parse_res;
  const uint8_t* next;
  Field field;
};

// Parses one field and returns the field itself and a pointer to the next
// field to parse. If parsing fails, the returned |next| == |buffer|.
PERFETTO_ALWAYS_INLINE ParseFieldResult
ParseOneField(const uint8_t* const buffer, const uint8_t* const end) {
  ParseFieldResult res{ParseFieldResult::kAbort, buffer, Field{}};

  // The first byte of a proto field is structured as follows:
  // The least 3 significant bits determine the field type.
  // The most 5 significant bits determine the field id. If MSB == 1, the
  // field id continues on the next bytes following the VarInt encoding.
  const uint8_t kFieldTypeNumBits = 3;
  const uint64_t kFieldTypeMask = (1 << kFieldTypeNumBits) - 1;  // 0000 0111;
  const uint8_t* pos = buffer;

  // If we've already hit the end, just return an invalid field.
  if (PERFETTO_UNLIKELY(pos >= end))
    return res;

  uint64_t preamble = 0;
  if (PERFETTO_LIKELY(*pos < 0x80)) {  // Fastpath for fields with ID < 16.
    preamble = *(pos++);
  } else {
    const uint8_t* next = ParseVarInt(pos, end, &preamble);
    if (PERFETTO_UNLIKELY(pos == next))
      return res;
    pos = next;
  }

  uint32_t field_id = static_cast<uint32_t>(preamble >> kFieldTypeNumBits);
  if (field_id == 0 || pos >= end)
    return res;

  auto field_type = static_cast<uint8_t>(preamble & kFieldTypeMask);
  const uint8_t* new_pos = pos;
  uint64_t int_value = 0;
  uint64_t size = 0;

  switch (field_type) {
    case static_cast<uint8_t>(ProtoWireType::kVarInt): {
      new_pos = ParseVarInt(pos, end, &int_value);

      // new_pos not being greater than pos means ParseVarInt could not fully
      // parse the number. This is because we are out of space in the buffer.
      // Set the id to zero and return but don't update the offset so a future
      // read can read this field.
      if (PERFETTO_UNLIKELY(new_pos == pos))
        return res;

      break;
    }

    case static_cast<uint8_t>(ProtoWireType::kLengthDelimited): {
      uint64_t payload_length;
      new_pos = ParseVarInt(pos, end, &payload_length);
      if (PERFETTO_UNLIKELY(new_pos == pos))
        return res;

      // ParseVarInt guarantees that |new_pos| <= |end| when it succeeds;
      if (payload_length > static_cast<uint64_t>(end - new_pos))
        return res;

      const uintptr_t payload_start = reinterpret_cast<uintptr_t>(new_pos);
      int_value = payload_start;
      size = payload_length;
      new_pos += payload_length;
      break;
    }

    case static_cast<uint8_t>(ProtoWireType::kFixed64): {
      new_pos = pos + sizeof(uint64_t);
      if (PERFETTO_UNLIKELY(new_pos > end))
        return res;
      memcpy(&int_value, pos, sizeof(uint64_t));
      break;
    }

    case static_cast<uint8_t>(ProtoWireType::kFixed32): {
      new_pos = pos + sizeof(uint32_t);
      if (PERFETTO_UNLIKELY(new_pos > end))
        return res;
      memcpy(&int_value, pos, sizeof(uint32_t));
      break;
    }

    default:
      PERFETTO_DLOG("Invalid proto field type: %u", field_type);
      return res;
  }

  res.next = new_pos;

  if (PERFETTO_UNLIKELY(field_id > std::numeric_limits<uint16_t>::max())) {
    PERFETTO_DLOG("Skipping field %" PRIu32 " because its id > 0xFFFF",
                  field_id);
    res.parse_res = ParseFieldResult::kSkip;
    return res;
  }

  if (PERFETTO_UNLIKELY(size > proto_utils::kMaxMessageLength)) {
    PERFETTO_DLOG("Skipping field %" PRIu32 " because it's too big (%" PRIu64
                  " KB)",
                  field_id, size / 1024);
    res.parse_res = ParseFieldResult::kSkip;
    return res;
  }

  res.parse_res = ParseFieldResult::kOk;
  res.field.initialize(static_cast<uint16_t>(field_id), field_type, int_value,
                       static_cast<uint32_t>(size));
  return res;
}

}  // namespace

Field ProtoDecoder::FindField(uint32_t field_id) {
  Field res{};
  auto old_position = read_ptr_;
  read_ptr_ = begin_;
  for (auto f = ReadField(); f.valid(); f = ReadField()) {
    if (f.id() == field_id) {
      res = f;
      break;
    }
  }
  read_ptr_ = old_position;
  return res;
}

PERFETTO_ALWAYS_INLINE
Field ProtoDecoder::ReadField() {
  ParseFieldResult res;
  do {
    res = ParseOneField(read_ptr_, end_);
    read_ptr_ = res.next;
  } while (PERFETTO_UNLIKELY(res.parse_res == ParseFieldResult::kSkip));
  return res.field;
}

void TypedProtoDecoderBase::ParseAllFields() {
  const uint8_t* cur = begin_;
  ParseFieldResult res;
  for (;;) {
    res = ParseOneField(cur, end_);
    PERFETTO_DCHECK(res.parse_res != ParseFieldResult::kOk || res.next != cur);
    cur = res.next;
    if (PERFETTO_UNLIKELY(res.parse_res == ParseFieldResult::kSkip)) {
      continue;
    } else if (PERFETTO_UNLIKELY(res.parse_res == ParseFieldResult::kAbort)) {
      break;
    }
    PERFETTO_DCHECK(res.parse_res == ParseFieldResult::kOk);
    PERFETTO_DCHECK(res.field.valid());
    auto field_id = res.field.id();
    if (PERFETTO_UNLIKELY(field_id >= num_fields_))
      continue;

    Field* fld = &fields_[field_id];
    if (PERFETTO_LIKELY(!fld->valid())) {
      // This is the first time we see this field.
      *fld = std::move(res.field);
    } else {
      // Repeated field case.
      // In this case we need to:
      // 1. Append the last value of the field to end of the repeated field
      //    storage.
      // 2. Replace the default instance at offset |field_id| with the current
      //    value. This is because in case of repeated field a call to Get(X) is
      //    supposed to return the last value of X, not the first one.
      // This is so that the RepeatedFieldIterator will iterate in the right
      // order, see comments on RepeatedFieldIterator.
      if (PERFETTO_UNLIKELY(size_ >= capacity_)) {
        ExpandHeapStorage();
        // ExpandHeapStorage moves fields_ so we need to update the ptr to fld:
        fld = &fields_[field_id];
        PERFETTO_DCHECK(size_ < capacity_);
      }
      fields_[size_++] = *fld;
      *fld = std::move(res.field);
    }
  }
  read_ptr_ = res.next;
}

void TypedProtoDecoderBase::ExpandHeapStorage() {
  uint32_t new_capacity = capacity_ * 2;
  PERFETTO_CHECK(new_capacity > size_);
  std::unique_ptr<Field[]> new_storage(new Field[new_capacity]);

  static_assert(std::is_trivially_copyable<Field>::value,
                "Field must be trivially copyable");
  memcpy(&new_storage[0], fields_, sizeof(Field) * size_);

  heap_storage_ = std::move(new_storage);
  fields_ = &heap_storage_[0];
  capacity_ = new_capacity;
}

}  // namespace protozero
