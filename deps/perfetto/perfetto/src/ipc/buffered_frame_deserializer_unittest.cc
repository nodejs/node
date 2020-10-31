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

#include <algorithm>
#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/ipc/wire_protocol.gen.h"

namespace perfetto {
namespace ipc {
namespace {

constexpr uint32_t kHeaderSize = sizeof(uint32_t);

// Generates a parsable Frame of exactly |size| bytes (including header).
std::vector<char> GetSimpleFrame(size_t size) {
  // A bit of reverse math of the proto encoding: a Frame which has only the
  // |data_for_testing| fields, will require for each data_for_testing that is
  // up to 127 bytes:
  // - 1 byte to write the field preamble (field type and id).
  // - 1 byte to write the field size, if 0 < size <= 127.
  // - N bytes for the actual content (|padding| below).
  // So below we split the payload into chunks of <= 127 bytes, keeping into
  // account the extra 2 bytes for each chunk.
  Frame frame;
  std::vector<char> padding;
  char padding_char = '0';
  const uint32_t payload_size = static_cast<uint32_t>(size - kHeaderSize);
  for (uint32_t size_left = payload_size; size_left > 0;) {
    PERFETTO_CHECK(size_left >= 2);  // We cannot produce frames < 2 bytes.
    uint32_t padding_size;
    if (size_left <= 127) {
      padding_size = size_left - 2;
      size_left = 0;
    } else {
      padding_size = 124;
      size_left -= padding_size + 2;
    }
    padding.resize(padding_size);
    for (uint32_t i = 0; i < padding_size; i++) {
      padding_char = padding_char == 'z' ? '0' : padding_char + 1;
      padding[i] = padding_char;
    }
    frame.add_data_for_testing(std::string(padding.data(), padding_size));
  }
  PERFETTO_CHECK(frame.SerializeAsString().size() == payload_size);
  std::vector<char> encoded_frame;
  encoded_frame.resize(size);
  char* enc_buf = encoded_frame.data();

  std::string payload = frame.SerializeAsString();
  memcpy(enc_buf, base::AssumeLittleEndian(&payload_size), kHeaderSize);
  memcpy(enc_buf + kHeaderSize, payload.data(), payload.size());
  PERFETTO_CHECK(encoded_frame.size() == size);
  return encoded_frame;
}

void CheckedMemcpy(BufferedFrameDeserializer::ReceiveBuffer rbuf,
                   const std::vector<char>& encoded_frame,
                   size_t offset = 0) {
  ASSERT_GE(rbuf.size, encoded_frame.size() + offset);
  memcpy(rbuf.data + offset, encoded_frame.data(), encoded_frame.size());
}

bool FrameEq(std::vector<char> expected_frame_with_header, const Frame& frame) {
  std::string reserialized_frame = frame.SerializeAsString();

  size_t expected_size = expected_frame_with_header.size() - kHeaderSize;
  EXPECT_EQ(expected_size, reserialized_frame.size());
  if (expected_size != reserialized_frame.size())
    return false;

  return memcmp(reserialized_frame.data(),
                expected_frame_with_header.data() + kHeaderSize,
                reserialized_frame.size()) == 0;
}

// Tests the simple case where each recv() just returns one whole header+frame.
TEST(BufferedFrameDeserializerTest, WholeMessages) {
  BufferedFrameDeserializer bfd;
  for (size_t i = 1; i <= 50; i++) {
    const size_t size = i * 10;
    BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();

    ASSERT_NE(nullptr, rbuf.data);
    std::vector<char> frame = GetSimpleFrame(size);
    CheckedMemcpy(rbuf, frame);
    ASSERT_TRUE(bfd.EndReceive(frame.size()));

    // Excactly one frame should be decoded, with no leftover buffer.
    auto decoded_frame = bfd.PopNextFrame();
    ASSERT_TRUE(decoded_frame);
    ASSERT_EQ(size - kHeaderSize, decoded_frame->SerializeAsString().size());
    ASSERT_FALSE(bfd.PopNextFrame());
    ASSERT_EQ(0u, bfd.size());
  }
}

// Sends first a simple test frame. Then creates a realistic Frame fragmenting
// it in three chunks and tests that the decoded Frame matches the original one.
// The recv() sequence is as follows:
// 1. [ simple_frame ] [ frame_chunk1 ... ]
// 2. [ ... frame_chunk2 ... ]
// 3. [ ... frame_chunk3 ]
TEST(BufferedFrameDeserializerTest, FragmentedFrameIsCorrectlyDeserialized) {
  BufferedFrameDeserializer bfd;
  Frame frame;
  frame.set_request_id(42);
  auto* bind_reply = frame.mutable_msg_bind_service_reply();
  bind_reply->set_success(true);
  bind_reply->set_service_id(0x4242);
  auto* method = bind_reply->add_methods();
  method->set_id(0x424242);
  method->set_name("foo");
  std::vector<char> serialized_frame;

  std::string payload = frame.SerializeAsString();
  uint32_t payload_size = static_cast<uint32_t>(payload.size());
  serialized_frame.resize(kHeaderSize + payload_size);
  memcpy(serialized_frame.data() + kHeaderSize, payload.data(), payload_size);
  memcpy(serialized_frame.data(), base::AssumeLittleEndian(&payload_size),
         kHeaderSize);

  std::vector<char> simple_frame = GetSimpleFrame(32);
  std::vector<char> frame_chunk1(serialized_frame.begin(),
                                 serialized_frame.begin() + 5);
  BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
  CheckedMemcpy(rbuf, simple_frame);
  CheckedMemcpy(rbuf, frame_chunk1, simple_frame.size());
  ASSERT_TRUE(bfd.EndReceive(simple_frame.size() + frame_chunk1.size()));

  std::vector<char> frame_chunk2(serialized_frame.begin() + 5,
                                 serialized_frame.begin() + 10);
  rbuf = bfd.BeginReceive();
  CheckedMemcpy(rbuf, frame_chunk2);
  ASSERT_TRUE(bfd.EndReceive(frame_chunk2.size()));

  std::vector<char> frame_chunk3(serialized_frame.begin() + 10,
                                 serialized_frame.end());
  rbuf = bfd.BeginReceive();
  CheckedMemcpy(rbuf, frame_chunk3);
  ASSERT_TRUE(bfd.EndReceive(frame_chunk3.size()));

  // Validate the received frame2.
  std::unique_ptr<Frame> decoded_simple_frame = bfd.PopNextFrame();
  ASSERT_TRUE(decoded_simple_frame);
  ASSERT_EQ(simple_frame.size() - kHeaderSize,
            decoded_simple_frame->SerializeAsString().size());

  std::unique_ptr<Frame> decoded_frame = bfd.PopNextFrame();
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(FrameEq(serialized_frame, *decoded_frame));
}

// Tests the case of a EndReceive(0) while receiving a valid frame in chunks.
TEST(BufferedFrameDeserializerTest, ZeroSizedReceive) {
  BufferedFrameDeserializer bfd;
  std::vector<char> frame = GetSimpleFrame(100);
  std::vector<char> frame_chunk1(frame.begin(), frame.begin() + 50);
  std::vector<char> frame_chunk2(frame.begin() + 50, frame.end());

  BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
  CheckedMemcpy(rbuf, frame_chunk1);
  ASSERT_TRUE(bfd.EndReceive(frame_chunk1.size()));

  rbuf = bfd.BeginReceive();
  ASSERT_TRUE(bfd.EndReceive(0));

  rbuf = bfd.BeginReceive();
  CheckedMemcpy(rbuf, frame_chunk2);
  ASSERT_TRUE(bfd.EndReceive(frame_chunk2.size()));

  // Excactly one frame should be decoded, with no leftover buffer.
  std::unique_ptr<Frame> decoded_frame = bfd.PopNextFrame();
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(FrameEq(frame, *decoded_frame));
  ASSERT_FALSE(bfd.PopNextFrame());
  ASSERT_EQ(0u, bfd.size());
}

// Tests the case of a EndReceive(4) where the header has no payload. The frame
// should be just skipped and not returned by PopNextFrame().
TEST(BufferedFrameDeserializerTest, EmptyPayload) {
  BufferedFrameDeserializer bfd;
  std::vector<char> frame = GetSimpleFrame(100);

  BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
  std::vector<char> empty_frame(kHeaderSize, 0);
  CheckedMemcpy(rbuf, empty_frame);
  ASSERT_TRUE(bfd.EndReceive(kHeaderSize));

  rbuf = bfd.BeginReceive();
  CheckedMemcpy(rbuf, frame);
  ASSERT_TRUE(bfd.EndReceive(frame.size()));

  // |fram| should be properly decoded.
  std::unique_ptr<Frame> decoded_frame = bfd.PopNextFrame();
  ASSERT_TRUE(decoded_frame);
  ASSERT_TRUE(FrameEq(frame, *decoded_frame));
  ASSERT_FALSE(bfd.PopNextFrame());
}

// Test the case where a single Receive() returns batches of > 1 whole frames.
// See case C in the comments for BufferedFrameDeserializer::EndReceive().
TEST(BufferedFrameDeserializerTest, MultipleFramesInOneReceive) {
  BufferedFrameDeserializer bfd;
  std::vector<std::vector<size_t>> frame_batch_sizes(
      {{11}, {13, 17, 19}, {23}, {29, 31}});

  for (std::vector<size_t>& batch : frame_batch_sizes) {
    BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
    size_t frame_offset_in_batch = 0;
    for (size_t frame_size : batch) {
      auto frame = GetSimpleFrame(frame_size);
      CheckedMemcpy(rbuf, frame, frame_offset_in_batch);
      frame_offset_in_batch += frame.size();
    }
    ASSERT_TRUE(bfd.EndReceive(frame_offset_in_batch));
    for (size_t expected_size : batch) {
      auto frame = bfd.PopNextFrame();
      ASSERT_TRUE(frame);
      ASSERT_EQ(expected_size - kHeaderSize, frame->SerializeAsString().size());
    }
    ASSERT_FALSE(bfd.PopNextFrame());
    ASSERT_EQ(0u, bfd.size());
  }
}

TEST(BufferedFrameDeserializerTest, RejectVeryLargeFrames) {
  BufferedFrameDeserializer bfd;
  BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
  const uint32_t kBigSize = std::numeric_limits<uint32_t>::max() - 2;
  memcpy(rbuf.data, base::AssumeLittleEndian(&kBigSize), kHeaderSize);
  memcpy(rbuf.data + kHeaderSize, "some initial payload", 20);
  ASSERT_FALSE(bfd.EndReceive(kHeaderSize + 20));
}

// Tests the extreme case of recv() fragmentation. Two valid frames are received
// but each recv() puts one byte at a time. Covers cases A and B commented in
// BufferedFrameDeserializer::EndReceive().
TEST(BufferedFrameDeserializerTest, HighlyFragmentedFrames) {
  BufferedFrameDeserializer bfd;
  for (size_t i = 1; i <= 50; i++) {
    std::vector<char> frame = GetSimpleFrame(i * 100);
    for (size_t off = 0; off < frame.size(); off++) {
      BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
      CheckedMemcpy(rbuf, {frame[off]});

      // The frame should be available only when receiving the last byte.
      ASSERT_TRUE(bfd.EndReceive(1));
      if (off < frame.size() - 1) {
        ASSERT_FALSE(bfd.PopNextFrame()) << off << "/" << frame.size();
        ASSERT_EQ(off + 1, bfd.size());
      } else {
        ASSERT_TRUE(bfd.PopNextFrame());
      }
    }
  }
}

// A bunch of valid frames interleaved with frames that have a valid header
// but unparsable payload. The expectation is that PopNextFrame() returns
// nullptr for the unparsable frames but the other frames are decoded peroperly.
TEST(BufferedFrameDeserializerTest, CanRecoverAfterUnparsableFrames) {
  BufferedFrameDeserializer bfd;
  for (size_t i = 1; i <= 50; i++) {
    const size_t size = i * 10;
    std::vector<char> frame = GetSimpleFrame(size);
    const bool unparsable = (i % 3) == 1;
    if (unparsable)
      memset(frame.data() + kHeaderSize, 0xFF, size - kHeaderSize);

    BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
    CheckedMemcpy(rbuf, frame);
    ASSERT_TRUE(bfd.EndReceive(frame.size()));

    // Excactly one frame should be decoded if |parsable|. In any case no
    // leftover bytes should be left in the buffer.
    auto decoded_frame = bfd.PopNextFrame();
    if (unparsable) {
      ASSERT_FALSE(decoded_frame);
    } else {
      ASSERT_TRUE(decoded_frame);
      ASSERT_EQ(size - kHeaderSize, decoded_frame->SerializeAsString().size());
    }
    ASSERT_EQ(0u, bfd.size());
  }
}

// Test that we can sustain recvs() which constantly max out the capacity.
// It sets up four frames:
// |frame1|: small, 1024 + 4 bytes.
// |frame2|: as big as the |kMaxCapacity|. Its recv() is split into two chunks.
// |frame3|: together with the 2nd part of |frame2| it maxes out capacity again.
// |frame4|: as big as the |kMaxCapacity|. Received in one recv(), no splits.
//
// Which are then recv()'d in a loop in the following way.
//    |------------ max recv capacity ------------|
// 1. [       frame1      ] [ frame2_chunk1 ..... ]
// 2. [ ... frame2_chunk2 ]
// 3. [  frame3  ]
// 4. [               frame 4                     ]
TEST(BufferedFrameDeserializerTest, FillCapacity) {
  size_t kMaxCapacity = 1024 * 16;
  BufferedFrameDeserializer bfd(kMaxCapacity);

  for (int i = 0; i < 3; i++) {
    std::vector<char> frame1 = GetSimpleFrame(1024);
    std::vector<char> frame2 = GetSimpleFrame(kMaxCapacity);
    std::vector<char> frame2_chunk1(
        frame2.begin(),
        frame2.begin() + static_cast<ptrdiff_t>(kMaxCapacity - frame1.size()));
    std::vector<char> frame2_chunk2(
        frame2.begin() + static_cast<ptrdiff_t>(frame2_chunk1.size()),
        frame2.end());
    std::vector<char> frame3 =
        GetSimpleFrame(kMaxCapacity - frame2_chunk2.size());
    std::vector<char> frame4 = GetSimpleFrame(kMaxCapacity);
    ASSERT_EQ(kMaxCapacity, frame1.size() + frame2_chunk1.size());
    ASSERT_EQ(kMaxCapacity, frame2_chunk1.size() + frame2_chunk2.size());
    ASSERT_EQ(kMaxCapacity, frame2_chunk2.size() + frame3.size());
    ASSERT_EQ(kMaxCapacity, frame4.size());

    BufferedFrameDeserializer::ReceiveBuffer rbuf = bfd.BeginReceive();
    CheckedMemcpy(rbuf, frame1);
    CheckedMemcpy(rbuf, frame2_chunk1, frame1.size());
    ASSERT_TRUE(bfd.EndReceive(frame1.size() + frame2_chunk1.size()));

    rbuf = bfd.BeginReceive();
    CheckedMemcpy(rbuf, frame2_chunk2);
    ASSERT_TRUE(bfd.EndReceive(frame2_chunk2.size()));

    rbuf = bfd.BeginReceive();
    CheckedMemcpy(rbuf, frame3);
    ASSERT_TRUE(bfd.EndReceive(frame3.size()));

    rbuf = bfd.BeginReceive();
    CheckedMemcpy(rbuf, frame4);
    ASSERT_TRUE(bfd.EndReceive(frame4.size()));

    std::unique_ptr<Frame> decoded_frame_1 = bfd.PopNextFrame();
    ASSERT_TRUE(decoded_frame_1);
    ASSERT_TRUE(FrameEq(frame1, *decoded_frame_1));

    std::unique_ptr<Frame> decoded_frame_2 = bfd.PopNextFrame();
    ASSERT_TRUE(decoded_frame_2);
    ASSERT_TRUE(FrameEq(frame2, *decoded_frame_2));

    std::unique_ptr<Frame> decoded_frame_3 = bfd.PopNextFrame();
    ASSERT_TRUE(decoded_frame_3);
    ASSERT_TRUE(FrameEq(frame3, *decoded_frame_3));

    std::unique_ptr<Frame> decoded_frame_4 = bfd.PopNextFrame();
    ASSERT_TRUE(decoded_frame_4);
    ASSERT_TRUE(FrameEq(frame4, *decoded_frame_4));

    ASSERT_FALSE(bfd.PopNextFrame());
  }
}

}  // namespace
}  // namespace ipc
}  // namespace perfetto
