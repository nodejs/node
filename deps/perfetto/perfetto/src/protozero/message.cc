/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "perfetto/protozero/message.h"

#include <atomic>
#include <type_traits>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/message_handle.h"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
// The memcpy() for float and double below needs to be adjusted if we want to
// support big endian CPUs. There doesn't seem to be a compelling need today.
#error Unimplemented for big endian archs.
#endif

namespace protozero {

namespace {

#if PERFETTO_DCHECK_IS_ON()
std::atomic<uint32_t> g_generation;
#endif

}  // namespace

// static
constexpr uint32_t Message::kMaxNestingDepth;

// Do NOT put any code in the constructor or use default initialization.
// Use the Reset() method below instead. See the header for the reason why.

// This method is called to initialize both root and nested messages.
void Message::Reset(ScatteredStreamWriter* stream_writer) {
// Older versions of libstdcxx don't have is_trivially_constructible.
#if !defined(__GLIBCXX__) || __GLIBCXX__ >= 20170516
  static_assert(std::is_trivially_constructible<Message>::value,
                "Message must be trivially constructible");
#endif

  static_assert(std::is_trivially_destructible<Message>::value,
                "Message must be trivially destructible");

  static_assert(
      sizeof(Message::nested_messages_arena_) >=
          kMaxNestingDepth *
              (sizeof(Message) - sizeof(Message::nested_messages_arena_)),
      "Message::nested_messages_arena_ is too small");

  stream_writer_ = stream_writer;
  size_ = 0;
  size_field_ = nullptr;
  size_already_written_ = 0;
  nested_message_ = nullptr;
  nesting_depth_ = 0;
  finalized_ = false;
#if PERFETTO_DCHECK_IS_ON()
  handle_ = nullptr;
  generation_ = g_generation.fetch_add(1, std::memory_order_relaxed);
#endif
}

void Message::AppendString(uint32_t field_id, const char* str) {
  AppendBytes(field_id, str, strlen(str));
}

void Message::AppendBytes(uint32_t field_id, const void* src, size_t size) {
  if (nested_message_)
    EndNestedMessage();

  PERFETTO_DCHECK(size < proto_utils::kMaxMessageLength);
  // Write the proto preamble (field id, type and length of the field).
  uint8_t buffer[proto_utils::kMaxSimpleFieldEncodedSize];
  uint8_t* pos = buffer;
  pos = proto_utils::WriteVarInt(proto_utils::MakeTagLengthDelimited(field_id),
                                 pos);
  pos = proto_utils::WriteVarInt(static_cast<uint32_t>(size), pos);
  WriteToStream(buffer, pos);

  const uint8_t* src_u8 = reinterpret_cast<const uint8_t*>(src);
  WriteToStream(src_u8, src_u8 + size);
}

size_t Message::AppendScatteredBytes(uint32_t field_id,
                                     ContiguousMemoryRange* ranges,
                                     size_t num_ranges) {
  size_t size = 0;
  for (size_t i = 0; i < num_ranges; ++i) {
    size += ranges[i].size();
  }

  PERFETTO_DCHECK(size < proto_utils::kMaxMessageLength);

  uint8_t buffer[proto_utils::kMaxSimpleFieldEncodedSize];
  uint8_t* pos = buffer;
  pos = proto_utils::WriteVarInt(proto_utils::MakeTagLengthDelimited(field_id),
                                 pos);
  pos = proto_utils::WriteVarInt(static_cast<uint32_t>(size), pos);
  WriteToStream(buffer, pos);

  for (size_t i = 0; i < num_ranges; ++i) {
    auto& range = ranges[i];
    WriteToStream(range.begin, range.end);
  }

  return size;
}

uint32_t Message::Finalize() {
  if (finalized_)
    return size_;

  if (nested_message_)
    EndNestedMessage();

  // Write the length of the nested message a posteriori, using a leading-zero
  // redundant varint encoding.
  if (size_field_) {
    PERFETTO_DCHECK(!finalized_);
    PERFETTO_DCHECK(size_ < proto_utils::kMaxMessageLength);
    PERFETTO_DCHECK(size_ >= size_already_written_);
    proto_utils::WriteRedundantVarInt(size_ - size_already_written_,
                                      size_field_);
    size_field_ = nullptr;
  }

  finalized_ = true;
#if PERFETTO_DCHECK_IS_ON()
  if (handle_)
    handle_->reset_message();
#endif

  return size_;
}

void Message::BeginNestedMessageInternal(uint32_t field_id, Message* message) {
  if (nested_message_)
    EndNestedMessage();

  // Write the proto preamble for the nested message.
  uint8_t data[proto_utils::kMaxTagEncodedSize];
  uint8_t* data_end = proto_utils::WriteVarInt(
      proto_utils::MakeTagLengthDelimited(field_id), data);
  WriteToStream(data, data_end);

  message->Reset(stream_writer_);
  PERFETTO_CHECK(nesting_depth_ < kMaxNestingDepth);
  message->nesting_depth_ = nesting_depth_ + 1;

  // The length of the nested message cannot be known upfront. So right now
  // just reserve the bytes to encode the size after the nested message is done.
  message->set_size_field(
      stream_writer_->ReserveBytes(proto_utils::kMessageLengthFieldSize));
  size_ += proto_utils::kMessageLengthFieldSize;
  nested_message_ = message;
}

void Message::EndNestedMessage() {
  size_ += nested_message_->Finalize();
  nested_message_ = nullptr;
}

}  // namespace protozero
