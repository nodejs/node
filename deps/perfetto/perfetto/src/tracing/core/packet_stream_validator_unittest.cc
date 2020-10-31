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

#include <string>

#include "protos/perfetto/trace/ftrace/ftrace_event.gen.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.gen.h"
#include "protos/perfetto/trace/ftrace/sched.gen.h"
#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

TEST(PacketStreamValidatorTest, NullPacket) {
  std::string ser_buf;
  Slices seq;
  EXPECT_TRUE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, SimplePacket) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  std::string ser_buf = proto.SerializeAsString();

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_TRUE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, ComplexPacket) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  proto.mutable_ftrace_events()->set_cpu(0);
  auto* ft = proto.mutable_ftrace_events()->add_event();
  ft->set_pid(42);
  ft->mutable_sched_switch()->set_prev_comm("tom");
  ft->mutable_sched_switch()->set_prev_pid(123);
  ft->mutable_sched_switch()->set_next_comm("jerry");
  ft->mutable_sched_switch()->set_next_pid(456);
  std::string ser_buf = proto.SerializeAsString();

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_TRUE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, SimplePacketWithUid) {
  protos::gen::TracePacket proto;
  proto.set_trusted_uid(123);
  std::string ser_buf = proto.SerializeAsString();

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_FALSE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, SimplePacketWithZeroUid) {
  protos::gen::TracePacket proto;
  proto.set_trusted_uid(0);
  std::string ser_buf = proto.SerializeAsString();

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_FALSE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, SimplePacketWithNegativeOneUid) {
  protos::gen::TracePacket proto;
  proto.set_trusted_uid(-1);
  std::string ser_buf = proto.SerializeAsString();

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_FALSE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, ComplexPacketWithUid) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  proto.mutable_ftrace_events()->set_cpu(0);
  auto* ft = proto.mutable_ftrace_events()->add_event();
  ft->set_pid(42);
  ft->mutable_sched_switch()->set_prev_comm("tom");
  ft->mutable_sched_switch()->set_prev_pid(123);
  ft->mutable_sched_switch()->set_next_comm("jerry");
  ft->mutable_sched_switch()->set_next_pid(456);
  proto.set_trusted_uid(123);
  std::string ser_buf = proto.SerializeAsString();

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_FALSE(PacketStreamValidator::Validate(seq));
}

TEST(PacketStreamValidatorTest, FragmentedPacket) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  proto.mutable_ftrace_events()->set_cpu(0);
  auto* ft = proto.mutable_ftrace_events()->add_event();
  ft->set_pid(42);
  ft->mutable_sched_switch()->set_prev_comm("tom");
  ft->mutable_sched_switch()->set_prev_pid(123);
  ft->mutable_sched_switch()->set_next_comm("jerry");
  ft->mutable_sched_switch()->set_next_pid(456);
  std::string ser_buf = proto.SerializeAsString();

  for (size_t i = 0; i < ser_buf.size(); i++) {
    Slices seq;
    seq.emplace_back(&ser_buf[0], i);
    seq.emplace_back(&ser_buf[i], ser_buf.size() - i);
    EXPECT_TRUE(PacketStreamValidator::Validate(seq));
  }
}

TEST(PacketStreamValidatorTest, FragmentedPacketWithUid) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  proto.set_trusted_uid(123);
  proto.mutable_ftrace_events()->set_cpu(0);
  auto* ft = proto.mutable_ftrace_events()->add_event();
  ft->set_pid(42);
  ft->mutable_sched_switch()->set_prev_comm("tom");
  ft->mutable_sched_switch()->set_prev_pid(123);
  ft->mutable_sched_switch()->set_next_comm("jerry");
  ft->mutable_sched_switch()->set_next_pid(456);
  proto.mutable_for_testing()->set_str("foo");
  std::string ser_buf = proto.SerializeAsString();

  for (size_t i = 0; i < ser_buf.size(); i++) {
    Slices seq;
    seq.emplace_back(&ser_buf[0], i);
    seq.emplace_back(&ser_buf[i], ser_buf.size() - i);
    EXPECT_FALSE(PacketStreamValidator::Validate(seq));
  }
}

TEST(PacketStreamValidatorTest, TruncatedPacket) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  std::string ser_buf = proto.SerializeAsString();

  for (size_t i = 1; i < ser_buf.size(); i++) {
    Slices seq;
    seq.emplace_back(&ser_buf[0], i);
    EXPECT_FALSE(PacketStreamValidator::Validate(seq));
  }
}

TEST(PacketStreamValidatorTest, TrailingGarbage) {
  protos::gen::TracePacket proto;
  proto.mutable_for_testing()->set_str("string field");
  std::string ser_buf = proto.SerializeAsString();
  ser_buf += "bike is short for bichael";

  Slices seq;
  seq.emplace_back(&ser_buf[0], ser_buf.size());
  EXPECT_FALSE(PacketStreamValidator::Validate(seq));
}

}  // namespace
}  // namespace perfetto
