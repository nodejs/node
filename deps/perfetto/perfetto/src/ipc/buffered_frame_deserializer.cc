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

#include "src/ipc/buffered_frame_deserializer.h"

#include <inttypes.h>

#include <algorithm>
#include <type_traits>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

#include "protos/perfetto/ipc/wire_protocol.gen.h"

namespace perfetto {
namespace ipc {

namespace {

// The header is just the number of bytes of the Frame protobuf message.
constexpr size_t kHeaderSize = sizeof(uint32_t);
}  // namespace

BufferedFrameDeserializer::BufferedFrameDeserializer(size_t max_capacity)
    : capacity_(max_capacity) {
  PERFETTO_CHECK(max_capacity % base::kPageSize == 0);
  PERFETTO_CHECK(max_capacity > base::kPageSize);
}

BufferedFrameDeserializer::~BufferedFrameDeserializer() = default;

BufferedFrameDeserializer::ReceiveBuffer
BufferedFrameDeserializer::BeginReceive() {
  // Upon the first recv initialize the buffer to the max message size but
  // release the physical memory for all but the first page. The kernel will
  // automatically give us physical pages back as soon as we page-fault on them.
  if (!buf_.IsValid()) {
    PERFETTO_DCHECK(size_ == 0);
    // TODO(eseckler): Don't commit all of the buffer at once on Windows.
    buf_ = base::PagedMemory::Allocate(capacity_);

    // Surely we are going to use at least the first page, but we may not need
    // the rest for a bit.
    buf_.AdviseDontNeed(buf() + base::kPageSize, capacity_ - base::kPageSize);
  }

  PERFETTO_CHECK(capacity_ > size_);
  return ReceiveBuffer{buf() + size_, capacity_ - size_};
}

bool BufferedFrameDeserializer::EndReceive(size_t recv_size) {
  PERFETTO_CHECK(recv_size + size_ <= capacity_);
  size_ += recv_size;

  // At this point the contents buf_ can contain:
  // A) Only a fragment of the header (the size of the frame). E.g.,
  //    03 00 00 (the header is 4 bytes, one is missing).
  //
  // B) A header and a part of the frame. E.g.,
  //     05 00 00 00         11 22 33
  //    [ header, size=5 ]  [ Partial frame ]
  //
  // C) One or more complete header+frame. E.g.,
  //     05 00 00 00         11 22 33 44 55   03 00 00 00        AA BB CC
  //    [ header, size=5 ]  [ Whole frame ]  [ header, size=3 ] [ Whole frame ]
  //
  // D) Some complete header+frame(s) and a partial header or frame (C + A/B).
  //
  // C Is the more likely case and the one we are optimizing for. A, B, D can
  // happen because of the streaming nature of the socket.
  // The invariant of this function is that, when it returns, buf_ is either
  // empty (we drained all the complete frames) or starts with the header of the
  // next, still incomplete, frame.

  size_t consumed_size = 0;
  for (;;) {
    if (size_ < consumed_size + kHeaderSize)
      break;  // Case A, not enough data to read even the header.

    // Read the header into |payload_size|.
    uint32_t payload_size = 0;
    const char* rd_ptr = buf() + consumed_size;
    memcpy(base::AssumeLittleEndian(&payload_size), rd_ptr, kHeaderSize);

    // Saturate the |payload_size| to prevent overflows. The > capacity_ check
    // below will abort the parsing.
    size_t next_frame_size =
        std::min(static_cast<size_t>(payload_size), capacity_);
    next_frame_size += kHeaderSize;
    rd_ptr += kHeaderSize;

    if (size_ < consumed_size + next_frame_size) {
      // Case B. We got the header but not the whole frame.
      if (next_frame_size > capacity_) {
        // The caller is expected to shut down the socket and give up at this
        // point. If it doesn't do that and insists going on at some point it
        // will hit the capacity check in BeginReceive().
        PERFETTO_LOG("IPC Frame too large (size %zu)", next_frame_size);
        return false;
      }
      break;
    }

    // Case C. We got at least one header and whole frame.
    DecodeFrame(rd_ptr, payload_size);
    consumed_size += next_frame_size;
  }

  PERFETTO_DCHECK(consumed_size <= size_);
  if (consumed_size > 0) {
    // Shift out the consumed data from the buffer. In the typical case (C)
    // there is nothing to shift really, just setting size_ = 0 is enough.
    // Shifting is only for the (unlikely) case D.
    size_ -= consumed_size;
    if (size_ > 0) {
      // Case D. We consumed some frames but there is a leftover at the end of
      // the buffer. Shift out the consumed bytes, so that on the next round
      // |buf_| starts with the header of the next unconsumed frame.
      const char* move_begin = buf() + consumed_size;
      PERFETTO_CHECK(move_begin > buf());
      PERFETTO_CHECK(move_begin + size_ <= buf() + capacity_);
      memmove(buf(), move_begin, size_);
    }
    // If we just finished decoding a large frame that used more than one page,
    // release the extra memory in the buffer. Large frames should be quite
    // rare.
    if (consumed_size > base::kPageSize) {
      size_t size_rounded_up = (size_ / base::kPageSize + 1) * base::kPageSize;
      if (size_rounded_up < capacity_) {
        char* madvise_begin = buf() + size_rounded_up;
        const size_t madvise_size = capacity_ - size_rounded_up;
        PERFETTO_CHECK(madvise_begin > buf() + size_);
        PERFETTO_CHECK(madvise_begin + madvise_size <= buf() + capacity_);
        buf_.AdviseDontNeed(madvise_begin, madvise_size);
      }
    }
  }
  // At this point |size_| == 0 for case C, > 0 for cases A, B, D.
  return true;
}

std::unique_ptr<Frame> BufferedFrameDeserializer::PopNextFrame() {
  if (decoded_frames_.empty())
    return nullptr;
  std::unique_ptr<Frame> frame = std::move(decoded_frames_.front());
  decoded_frames_.pop_front();
  return frame;
}

void BufferedFrameDeserializer::DecodeFrame(const char* data, size_t size) {
  if (size == 0)
    return;
  std::unique_ptr<Frame> frame(new Frame);
  if (frame->ParseFromArray(data, size))
    decoded_frames_.push_back(std::move(frame));
}

// static
std::string BufferedFrameDeserializer::Serialize(const Frame& frame) {
  std::vector<uint8_t> payload = frame.SerializeAsArray();
  const uint32_t payload_size = static_cast<uint32_t>(payload.size());
  std::string buf;
  buf.resize(kHeaderSize + payload_size);
  memcpy(&buf[0], base::AssumeLittleEndian(&payload_size), kHeaderSize);
  memcpy(&buf[kHeaderSize], payload.data(), payload.size());
  return buf;
}

}  // namespace ipc
}  // namespace perfetto
