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

#include "perfetto/ext/tracing/core/trace_packet.h"

#include <string>

#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

TEST(TracePacketTest, Simple) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  std::string ser_buf = proto.SerializeAsString();
  TracePacket tp;
  tp.AddSlice(ser_buf.data(), ser_buf.size());
  auto slice = tp.slices().begin();
  ASSERT_NE(tp.slices().end(), slice);
  ASSERT_EQ(ser_buf.data(), slice->start);
  ASSERT_EQ(ser_buf.size(), slice->size);
  ASSERT_EQ(tp.slices().end(), ++slice);

  protos::gen::TracePacket decoded_packet;
  ASSERT_TRUE(decoded_packet.ParseFromString(tp.GetRawBytesForTesting()));
  ASSERT_EQ(proto.for_testing().str(), decoded_packet.for_testing().str());
}

TEST(TracePacketTest, Sliced) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str(
      "this is an arbitrarily long string ........................");
  std::string ser_buf = proto.SerializeAsString();
  TracePacket tp;
  tp.AddSlice({ser_buf.data(), 3});
  tp.AddSlice({ser_buf.data() + 3, 5});
  tp.AddSlice({ser_buf.data() + 3 + 5, ser_buf.size() - 3 - 5});
  ASSERT_EQ(ser_buf.size(), tp.size());

  auto slice = tp.slices().begin();
  ASSERT_NE(tp.slices().end(), slice);
  ASSERT_EQ(ser_buf.data(), slice->start);
  ASSERT_EQ(3u, slice->size);

  ASSERT_NE(tp.slices().end(), ++slice);
  ASSERT_EQ(ser_buf.data() + 3, slice->start);
  ASSERT_EQ(5u, slice->size);

  ASSERT_NE(tp.slices().end(), ++slice);
  ASSERT_EQ(ser_buf.data() + 3 + 5, slice->start);
  ASSERT_EQ(ser_buf.size() - 3 - 5, slice->size);

  ASSERT_EQ(tp.slices().end(), ++slice);

  protos::gen::TracePacket decoded_packet;
  ASSERT_TRUE(decoded_packet.ParseFromString(tp.GetRawBytesForTesting()));
  ASSERT_EQ(proto.for_testing().str(), decoded_packet.for_testing().str());
}

TEST(TracePacketTest, Corrupted) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  std::string ser_buf = proto.SerializeAsString();
  TracePacket tp;
  tp.AddSlice({ser_buf.data(), ser_buf.size() - 2});  // corrupted.
  protos::gen::TracePacket decoded_packet;
  ASSERT_FALSE(decoded_packet.ParseFromString(tp.GetRawBytesForTesting()));
}

// Tests that the GetProtoPreamble() logic returns a valid preamble that allows
// to encode a TracePacket as a field of the root trace.proto message.
TEST(TracePacketTest, GetProtoPreamble) {
  char* preamble;
  size_t preamble_size;

  // Test empty packet.
  TracePacket tp;
  std::tie(preamble, preamble_size) = tp.GetProtoPreamble();
  ASSERT_EQ(2u, preamble_size);
  ASSERT_EQ(0, preamble[1]);

  // Test packet with one slice.
  protos::gen::TracePacket tp_proto;
  char payload[257];
  for (size_t i = 0; i < sizeof(payload) - 1; i++)
    payload[i] = 'a' + (i % 16);
  payload[sizeof(payload) - 1] = '\0';
  tp_proto.mutable_for_testing()->set_str(payload);
  std::string ser_buf = tp_proto.SerializeAsString();
  tp.AddSlice({ser_buf.data(), ser_buf.size()});

  std::tie(preamble, preamble_size) = tp.GetProtoPreamble();
  ASSERT_EQ(3u, preamble_size);

  // Verify that the content is actually parsable using libprotobuf.
  char buf[512];
  memcpy(buf, preamble, preamble_size);
  ASSERT_EQ(1u, tp.slices().size());
  memcpy(&buf[preamble_size], tp.slices()[0].start, tp.slices()[0].size);
  protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(buf, preamble_size + tp.size()));
  ASSERT_EQ(1, trace.packet_size());
  ASSERT_EQ(payload, trace.packet()[0].for_testing().str());
}

TEST(TracePacketTest, MoveOperators) {
  char buf1[5]{};
  char buf2[7]{};

  TracePacket tp;
  tp.AddSlice(buf1, sizeof(buf1));
  tp.AddSlice(buf2, sizeof(buf2));
  tp.AddSlice(Slice::Allocate(11));

  TracePacket moved_tp(std::move(tp));
  ASSERT_EQ(0u, tp.size());
  ASSERT_TRUE(tp.slices().empty());
  ASSERT_EQ(3u, moved_tp.slices().size());
  ASSERT_EQ(5u + 7u + 11u, moved_tp.size());

  TracePacket moved_tp_2;
  moved_tp_2 = std::move(moved_tp);
  ASSERT_EQ(0u, moved_tp.size());
  ASSERT_TRUE(moved_tp.slices().empty());
  ASSERT_EQ(3u, moved_tp_2.slices().size());
  ASSERT_EQ(5u + 7u + 11u, moved_tp_2.size());
}

}  // namespace
}  // namespace perfetto
