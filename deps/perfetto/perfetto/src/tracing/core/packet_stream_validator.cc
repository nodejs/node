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

#include "src/tracing/core/packet_stream_validator.h"

#include <inttypes.h>
#include <stddef.h>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/protozero/proto_utils.h"

#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto {

namespace {

using protozero::proto_utils::ProtoWireType;

const uint32_t kReservedFieldIds[] = {
    protos::pbzero::TracePacket::kTrustedUidFieldNumber,
    protos::pbzero::TracePacket::kTrustedPacketSequenceIdFieldNumber,
    protos::pbzero::TracePacket::kTraceConfigFieldNumber,
    protos::pbzero::TracePacket::kTraceStatsFieldNumber,
    protos::pbzero::TracePacket::kCompressedPacketsFieldNumber,
    protos::pbzero::TracePacket::kSynchronizationMarkerFieldNumber,
};

// This translation unit is quite subtle and perf-sensitive. Remember to check
// BM_PacketStreamValidator in perfetto_benchmarks when making changes.

// Checks that a packet, spread over several slices, is well-formed and doesn't
// contain reserved top-level fields.
// The checking logic is based on a state-machine that skips the fields' payload
// and operates as follows:
//              +-------------------------------+ <-------------------------+
// +----------> | Read field preamble (varint)  | <----------------------+  |
// |            +-------------------------------+                        |  |
// |              |              |            |                          |  |
// |       <Varint>        <Fixed 32/64>     <Length-delimited field>    |  |
// |          V                  |                      V                |  |
// |  +------------------+       |               +--------------+        |  |
// |  | Read field value |       |               | Read length  |        |  |
// |  | (another varint) |       |               |   (varint)   |        |  |
// |  +------------------+       |               +--------------+        |  |
// |           |                 V                      V                |  |
// +-----------+        +----------------+     +-----------------+       |  |
//                      | Skip 4/8 Bytes |     | Skip $len Bytes |-------+  |
//                      +----------------+     +-----------------+          |
//                               |                                          |
//                               +------------------------------------------+
class ProtoFieldParserFSM {
 public:
  // This method effectively continuously parses varints (either for the field
  // preamble or the payload or the submessage length) and tells the caller
  // (the Validate() method) how many bytes to skip until the next field.
  size_t Push(uint8_t octet) {
    varint_ |= static_cast<uint64_t>(octet & 0x7F) << varint_shift_;
    if (octet & 0x80) {
      varint_shift_ += 7;
      if (varint_shift_ >= 64)
        state_ = kInvalidVarInt;
      return 0;
    }
    uint64_t varint = varint_;
    varint_ = 0;
    varint_shift_ = 0;

    switch (state_) {
      case kFieldPreamble: {
        uint64_t field_type = varint & 7;  // 7 = 0..0111
        auto field_id = static_cast<uint32_t>(varint >> 3);
        // Check if the field id is reserved, go into an error state if it is.
        for (size_t i = 0; i < base::ArraySize(kReservedFieldIds); ++i) {
          if (field_id == kReservedFieldIds[i]) {
            state_ = kWroteReservedField;
            return 0;
          }
        }
        // The field type is legit, now check it's well formed and within
        // boundaries.
        if (field_type == static_cast<uint64_t>(ProtoWireType::kVarInt)) {
          state_ = kVarIntValue;
        } else if (field_type ==
                   static_cast<uint64_t>(ProtoWireType::kFixed32)) {
          return 4;
        } else if (field_type ==
                   static_cast<uint64_t>(ProtoWireType::kFixed64)) {
          return 8;
        } else if (field_type ==
                   static_cast<uint64_t>(ProtoWireType::kLengthDelimited)) {
          state_ = kLenDelimitedLen;
        } else {
          state_ = kUnknownFieldType;
        }
        return 0;
      }

      case kVarIntValue: {
        // Consume the int field payload and go back to the next field.
        state_ = kFieldPreamble;
        return 0;
      }

      case kLenDelimitedLen: {
        if (varint > protozero::proto_utils::kMaxMessageLength) {
          state_ = kMessageTooBig;
          return 0;
        }
        state_ = kFieldPreamble;
        return static_cast<size_t>(varint);
      }

      case kWroteReservedField:
      case kUnknownFieldType:
      case kMessageTooBig:
      case kInvalidVarInt:
        // Persistent error states.
        return 0;

    }          // switch(state_)
    return 0;  // To keep GCC happy.
  }

  // Queried at the end of the all payload. A message is well-formed only
  // if the FSM is back to the state where it should parse the next field and
  // hasn't started parsing any preamble.
  bool valid() const { return state_ == kFieldPreamble && varint_shift_ == 0; }
  int state() const { return static_cast<int>(state_); }

 private:
  enum State {
    kFieldPreamble = 0,  // Parsing the varint for the field preamble.
    kVarIntValue,        // Parsing the varint value for the field payload.
    kLenDelimitedLen,    // Parsing the length of the length-delimited field.

    // Error states:
    kWroteReservedField,  // Tried to set a reserved field id.
    kUnknownFieldType,    // Encountered an invalid field type.
    kMessageTooBig,       // Size of the length delimited message was too big.
    kInvalidVarInt,       // VarInt larger than 64 bits.
  };

  State state_ = kFieldPreamble;
  uint64_t varint_ = 0;
  uint32_t varint_shift_ = 0;
};

}  // namespace

// static
bool PacketStreamValidator::Validate(const Slices& slices) {
  ProtoFieldParserFSM parser;
  size_t skip_bytes = 0;
  for (const Slice& slice : slices) {
    for (size_t i = 0; i < slice.size;) {
      const size_t skip_bytes_cur_slice = std::min(skip_bytes, slice.size - i);
      if (skip_bytes_cur_slice > 0) {
        i += skip_bytes_cur_slice;
        skip_bytes -= skip_bytes_cur_slice;
      } else {
        uint8_t octet = *(reinterpret_cast<const uint8_t*>(slice.start) + i);
        skip_bytes = parser.Push(octet);
        i++;
      }
    }
  }
  if (skip_bytes == 0 && parser.valid())
    return true;

  PERFETTO_DLOG("Packet validation error (state %d, skip = %zu)",
                parser.state(), skip_bytes);
  return false;
}

}  // namespace perfetto
