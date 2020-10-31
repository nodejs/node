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
#include "src/trace_processor/importers/proto/proto_trace_parser.h"

#include <map>
#include <random>
#include <vector>

#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_sorter.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;

class MockTraceParser : public ProtoTraceParser {
 public:
  MockTraceParser(TraceProcessorContext* context) : ProtoTraceParser(context) {}

  MOCK_METHOD4(MOCK_ParseFtracePacket,
               void(uint32_t cpu,
                    int64_t timestamp,
                    const uint8_t* data,
                    size_t length));

  void ParseFtracePacket(uint32_t cpu,
                         int64_t timestamp,
                         TimestampedTracePiece ttp) override {
    bool isNonCompact = ttp.type == TimestampedTracePiece::Type::kFtraceEvent;
    MOCK_ParseFtracePacket(cpu, timestamp,
                           isNonCompact ? ttp.ftrace_event.data() : nullptr,
                           isNonCompact ? ttp.ftrace_event.length() : 0);
  }

  MOCK_METHOD3(MOCK_ParseTracePacket,
               void(int64_t ts, const uint8_t* data, size_t length));

  void ParseTracePacket(int64_t ts, TimestampedTracePiece ttp) override {
    TraceBlobView& tbv = ttp.packet_data.packet;
    MOCK_ParseTracePacket(ts, tbv.data(), tbv.length());
  }
};

class MockTraceStorage : public TraceStorage {
 public:
  MockTraceStorage() : TraceStorage() {}

  MOCK_METHOD1(InternString, StringId(base::StringView view));
};

class TraceSorterTest : public ::testing::Test {
 public:
  TraceSorterTest()
      : test_buffer_(std::unique_ptr<uint8_t[]>(new uint8_t[8]), 0, 8) {
    storage_ = new NiceMock<MockTraceStorage>();
    context_.storage.reset(storage_);

    std::unique_ptr<MockTraceParser> parser(new MockTraceParser(&context_));
    parser_ = parser.get();

    context_.sorter.reset(
        new TraceSorter(std::move(parser),
                        std::numeric_limits<int64_t>::max() /*window_size*/));
  }

 protected:
  TraceProcessorContext context_;
  MockTraceParser* parser_;
  NiceMock<MockTraceStorage>* storage_;
  TraceBlobView test_buffer_;
};

TEST_F(TraceSorterTest, TestFtrace) {
  TraceBlobView view = test_buffer_.slice(0, 1);
  EXPECT_CALL(*parser_, MOCK_ParseFtracePacket(0, 1000, view.data(), 1));
  context_.sorter->PushFtraceEvent(0 /*cpu*/, 1000 /*timestamp*/,
                                   std::move(view));
  context_.sorter->FinalizeFtraceEventBatch(0);
  context_.sorter->ExtractEventsForced();
}

TEST_F(TraceSorterTest, TestTracePacket) {
  PacketSequenceState state(&context_);
  TraceBlobView view = test_buffer_.slice(0, 1);
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1000, view.data(), 1));
  context_.sorter->PushTracePacket(1000, &state, std::move(view));
  context_.sorter->FinalizeFtraceEventBatch(1000);
  context_.sorter->ExtractEventsForced();
}

TEST_F(TraceSorterTest, Ordering) {
  PacketSequenceState state(&context_);
  TraceBlobView view_1 = test_buffer_.slice(0, 1);
  TraceBlobView view_2 = test_buffer_.slice(0, 2);
  TraceBlobView view_3 = test_buffer_.slice(0, 3);
  TraceBlobView view_4 = test_buffer_.slice(0, 4);

  InSequence s;

  EXPECT_CALL(*parser_, MOCK_ParseFtracePacket(0, 1000, view_1.data(), 1));
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1001, view_2.data(), 2));
  EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1100, view_3.data(), 3));
  EXPECT_CALL(*parser_, MOCK_ParseFtracePacket(2, 1200, view_4.data(), 4));

  context_.sorter->SetWindowSizeNs(200);
  context_.sorter->PushFtraceEvent(2 /*cpu*/, 1200 /*timestamp*/,
                                   std::move(view_4));
  context_.sorter->FinalizeFtraceEventBatch(2);
  context_.sorter->PushTracePacket(1001, &state, std::move(view_2));
  context_.sorter->PushTracePacket(1100, &state, std::move(view_3));
  context_.sorter->PushFtraceEvent(0 /*cpu*/, 1000 /*timestamp*/,
                                   std::move(view_1));

  context_.sorter->FinalizeFtraceEventBatch(0);
  context_.sorter->ExtractEventsForced();
}

TEST_F(TraceSorterTest, SetWindowSize) {
  PacketSequenceState state(&context_);
  TraceBlobView view_1 = test_buffer_.slice(0, 1);
  TraceBlobView view_2 = test_buffer_.slice(0, 2);
  TraceBlobView view_3 = test_buffer_.slice(0, 3);
  TraceBlobView view_4 = test_buffer_.slice(0, 4);

  MockFunction<void(std::string check_point_name)> check;

  {
    InSequence s;

    EXPECT_CALL(*parser_, MOCK_ParseFtracePacket(0, 1000, view_1.data(), 1));
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1001, view_2.data(), 2));
    EXPECT_CALL(check, Call("1"));
    EXPECT_CALL(*parser_, MOCK_ParseTracePacket(1100, view_3.data(), 3));
    EXPECT_CALL(check, Call("2"));
    EXPECT_CALL(*parser_, MOCK_ParseFtracePacket(2, 1200, view_4.data(), 4));
  }

  context_.sorter->SetWindowSizeNs(200);
  context_.sorter->PushFtraceEvent(2 /*cpu*/, 1200 /*timestamp*/,
                                   std::move(view_4));
  context_.sorter->FinalizeFtraceEventBatch(2);
  context_.sorter->PushTracePacket(1001, &state, std::move(view_2));
  context_.sorter->PushTracePacket(1100, &state, std::move(view_3));

  context_.sorter->PushFtraceEvent(0 /*cpu*/, 1000 /*timestamp*/,
                                   std::move(view_1));
  context_.sorter->FinalizeFtraceEventBatch(0);

  // At this point, we should just flush the 1000 and 1001 packets.
  context_.sorter->SetWindowSizeNs(101);

  // Inform the mock about where we are.
  check.Call("1");

  // Now we should flush the 1100 packet.
  context_.sorter->SetWindowSizeNs(99);

  // Inform the mock about where we are.
  check.Call("2");

  // Now we should flush the 1200 packet.
  context_.sorter->ExtractEventsForced();
}

// Simulates a random stream of ftrace events happening on random CPUs.
// Tests that the output of the TraceSorter matches the timestamp order
// (% events happening at the same time on different CPUs).
TEST_F(TraceSorterTest, MultiQueueSorting) {
  std::minstd_rand0 rnd_engine(0);
  std::map<int64_t /*ts*/, std::vector<uint32_t /*cpu*/>> expectations;

  EXPECT_CALL(*parser_, MOCK_ParseFtracePacket(_, _, _, _))
      .WillRepeatedly(Invoke([&expectations](uint32_t cpu, int64_t timestamp,
                                             const uint8_t*, size_t) {
        EXPECT_EQ(expectations.begin()->first, timestamp);
        auto& cpus = expectations.begin()->second;
        bool cpu_found = false;
        for (auto it = cpus.begin(); it < cpus.end(); it++) {
          if (*it != cpu)
            continue;
          cpu_found = true;
          cpus.erase(it);
          break;
        }
        if (cpus.empty())
          expectations.erase(expectations.begin());
        EXPECT_TRUE(cpu_found);
      }));

  for (int i = 0; i < 1000; i++) {
    int64_t ts = abs(static_cast<int64_t>(rnd_engine()));
    int num_cpus = rnd_engine() % 3;
    for (int j = 0; j < num_cpus; j++) {
      uint32_t cpu = static_cast<uint32_t>(rnd_engine() % 32);
      expectations[ts].push_back(cpu);
      context_.sorter->PushFtraceEvent(cpu, ts, TraceBlobView(nullptr, 0, 0));
      context_.sorter->FinalizeFtraceEventBatch(cpu);
    }
  }

  context_.sorter->ExtractEventsForced();
  EXPECT_TRUE(expectations.empty());
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
