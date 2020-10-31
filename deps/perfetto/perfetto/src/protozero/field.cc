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

#include "perfetto/protozero/field.h"

#include "perfetto/base/logging.h"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
// The memcpy() for fixed32/64 below needs to be adjusted if we want to
// support big endian CPUs. There doesn't seem to be a compelling need today.
#error Unimplemented for big endian archs.
#endif

namespace protozero {

template <typename Container>
void Field::SerializeAndAppendToInternal(Container* dst) const {
  namespace pu = proto_utils;
  size_t initial_size = dst->size();
  dst->resize(initial_size + pu::kMaxSimpleFieldEncodedSize + size_);
  uint8_t* start = reinterpret_cast<uint8_t*>(&(*dst)[initial_size]);
  uint8_t* wptr = start;
  switch (type_) {
    case static_cast<int>(pu::ProtoWireType::kVarInt): {
      wptr = pu::WriteVarInt(pu::MakeTagVarInt(id_), wptr);
      wptr = pu::WriteVarInt(int_value_, wptr);
      break;
    }
    case static_cast<int>(pu::ProtoWireType::kFixed32): {
      wptr = pu::WriteVarInt(pu::MakeTagFixed<uint32_t>(id_), wptr);
      uint32_t value32 = static_cast<uint32_t>(int_value_);
      memcpy(wptr, &value32, sizeof(value32));
      wptr += sizeof(uint32_t);
      break;
    }
    case static_cast<int>(pu::ProtoWireType::kFixed64): {
      wptr = pu::WriteVarInt(pu::MakeTagFixed<uint64_t>(id_), wptr);
      memcpy(wptr, &int_value_, sizeof(int_value_));
      wptr += sizeof(uint64_t);
      break;
    }
    case static_cast<int>(pu::ProtoWireType::kLengthDelimited): {
      ConstBytes payload = as_bytes();
      wptr = pu::WriteVarInt(pu::MakeTagLengthDelimited(id_), wptr);
      wptr = pu::WriteVarInt(payload.size, wptr);
      memcpy(wptr, payload.data, payload.size);
      wptr += payload.size;
      break;
    }
    default:
      PERFETTO_FATAL("Unknown field type %u", type_);
  }
  size_t written_size = static_cast<size_t>(wptr - start);
  PERFETTO_DCHECK(written_size > 0 && written_size < pu::kMaxMessageLength);
  PERFETTO_DCHECK(initial_size + written_size <= dst->size());
  dst->resize(initial_size + written_size);
}

void Field::SerializeAndAppendTo(std::string* dst) const {
  SerializeAndAppendToInternal(dst);
}

void Field::SerializeAndAppendTo(std::vector<uint8_t>* dst) const {
  SerializeAndAppendToInternal(dst);
}

}  // namespace protozero
