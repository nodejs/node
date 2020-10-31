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

#include "src/traced/probes/android_log/android_log_data_source.h"

#include "perfetto/tracing/core/data_source_config.h"
#include "src/base/test/test_task_runner.h"
#include "src/tracing/core/trace_writer_for_testing.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/common/android_log_constants.gen.h"
#include "protos/perfetto/config/android/android_log_config.gen.h"
#include "protos/perfetto/trace/android/android_log.gen.h"

using ::perfetto::protos::gen::AndroidLogConfig;
using ::perfetto::protos::gen::AndroidLogId;
using ::testing::Invoke;
using ::testing::Return;

namespace perfetto {
namespace {

class TestAndroidLogDataSource : public AndroidLogDataSource {
 public:
  TestAndroidLogDataSource(const DataSourceConfig& config,
                           base::TaskRunner* task_runner,
                           TracingSessionID id,
                           std::unique_ptr<TraceWriter> writer)
      : AndroidLogDataSource(config, task_runner, id, std::move(writer)) {}

  MOCK_METHOD0(ReadEventLogDefinitions, std::string());
  MOCK_METHOD0(ConnectLogdrSocket, base::UnixSocketRaw());
};

class AndroidLogDataSourceTest : public ::testing::Test {
 protected:
  AndroidLogDataSourceTest() {}

  void CreateInstance(const DataSourceConfig& cfg) {
    auto writer =
        std::unique_ptr<TraceWriterForTesting>(new TraceWriterForTesting());
    writer_raw_ = writer.get();
    data_source_.reset(
        new TestAndroidLogDataSource(cfg, &task_runner_, 0, std::move(writer)));
  }

  void StartAndSimulateLogd(
      const std::vector<std::vector<uint8_t>>& fake_events) {
    base::UnixSocketRaw send_sock;
    base::UnixSocketRaw recv_sock;
    // In theory this should be a kSeqPacket. We use kDgram here so that the
    // test can run also on MacOS (which doesn't support SOCK_SEQPACKET).
    std::tie(send_sock, recv_sock) = base::UnixSocketRaw::CreatePair(
        base::SockFamily::kUnix, base::SockType::kDgram);
    ASSERT_TRUE(send_sock);
    ASSERT_TRUE(recv_sock);

    EXPECT_CALL(*data_source_, ConnectLogdrSocket())
        .WillOnce(Invoke([&recv_sock] { return std::move(recv_sock); }));

    data_source_->Start();

    char cmd[64]{};
    EXPECT_GT(send_sock.Receive(cmd, sizeof(cmd) - 1), 0);
    EXPECT_STREQ("stream tail=1 lids=0,2,3,4,7", cmd);

    // Send back log messages emulating Android's logdr socket.
    for (const auto& buf : fake_events)
      send_sock.Send(buf.data(), buf.size());

    auto on_flush = task_runner_.CreateCheckpoint("on_flush");
    data_source_->Flush(1, on_flush);
    task_runner_.RunUntilCheckpoint("on_flush");
  }

  base::TestTaskRunner task_runner_;
  std::unique_ptr<TestAndroidLogDataSource> data_source_;
  TraceWriterForTesting* writer_raw_;

  const std::vector<std::vector<uint8_t>> kValidTextEvents{
      // 12-29 23:13:59.679  7546  8991 I ActivityManager:
      // Killing 11660:com.google.android.videos/u0a168 (adj 985): empty #17
      {0x55, 0x00, 0x1c, 0x00, 0x7a, 0x1d, 0x00, 0x00, 0x1f, 0x23, 0x00, 0x00,
       0xb7, 0xff, 0x27, 0x5c, 0xe6, 0x58, 0x7b, 0x28, 0x03, 0x00, 0x00, 0x00,
       0xe8, 0x03, 0x00, 0x00, 0x04, 0x41, 0x63, 0x74, 0x69, 0x76, 0x69, 0x74,
       0x79, 0x4d, 0x61, 0x6e, 0x61, 0x67, 0x65, 0x72, 0x00, 0x4b, 0x69, 0x6c,
       0x6c, 0x69, 0x6e, 0x67, 0x20, 0x31, 0x31, 0x36, 0x36, 0x30, 0x3a, 0x63,
       0x6f, 0x6d, 0x2e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e, 0x61, 0x6e,
       0x64, 0x72, 0x6f, 0x69, 0x64, 0x2e, 0x76, 0x69, 0x64, 0x65, 0x6f, 0x73,
       0x2f, 0x75, 0x30, 0x61, 0x31, 0x36, 0x38, 0x20, 0x28, 0x61, 0x64, 0x6a,
       0x20, 0x39, 0x38, 0x35, 0x29, 0x3a, 0x20, 0x65, 0x6d, 0x70, 0x74, 0x79,
       0x20, 0x23, 0x31, 0x37, 0x00},

      // 12-29 23:13:59.683  7546  7570 W libprocessgroup:
      // kill(-11660, 9) failed: No such process
      {0x39, 0x00, 0x1c, 0x00, 0x7a, 0x1d, 0x00, 0x00, 0x92, 0x1d, 0x00,
       0x00, 0xb7, 0xff, 0x27, 0x5c, 0x12, 0xf3, 0xbd, 0x28, 0x00, 0x00,
       0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x05, 0x6c, 0x69, 0x62, 0x70,
       0x72, 0x6f, 0x63, 0x65, 0x73, 0x73, 0x67, 0x72, 0x6f, 0x75, 0x70,
       0x00, 0x6b, 0x69, 0x6c, 0x6c, 0x28, 0x2d, 0x31, 0x31, 0x36, 0x36,
       0x30, 0x2c, 0x20, 0x39, 0x29, 0x20, 0x66, 0x61, 0x69, 0x6c, 0x65,
       0x64, 0x3a, 0x20, 0x4e, 0x6f, 0x20, 0x73, 0x75, 0x63, 0x68, 0x20,
       0x70, 0x72, 0x6f, 0x63, 0x65, 0x73, 0x73, 0x00},

      // 12-29 23:13:59.719  7415  7415 I Zygote:
      // Process 11660 exited due to signal (9)
      {0x2f, 0x00, 0x1c, 0x00, 0xf7, 0x1c, 0x00, 0x00, 0xf7, 0x1c, 0x00,
       0x00, 0xb7, 0xff, 0x27, 0x5c, 0x7c, 0x11, 0xe2, 0x2a, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x5a, 0x79, 0x67, 0x6f,
       0x74, 0x65, 0x00, 0x50, 0x72, 0x6f, 0x63, 0x65, 0x73, 0x73, 0x20,
       0x31, 0x31, 0x36, 0x36, 0x30, 0x20, 0x65, 0x78, 0x69, 0x74, 0x65,
       0x64, 0x20, 0x64, 0x75, 0x65, 0x20, 0x74, 0x6f, 0x20, 0x73, 0x69,
       0x67, 0x6e, 0x61, 0x6c, 0x20, 0x28, 0x39, 0x29, 0x00},
  };

  const std::vector<std::vector<uint8_t>> kValidBinaryEvents{
      // 12-30 10:22:08.914 29981 30962 I am_kill :
      // [0,31730,android.process.acore,985,empty #17]
      {0x3d, 0x00, 0x1c, 0x00, 0x1d, 0x75, 0x00, 0x00, 0xf2, 0x78, 0x00, 0x00,
       0x50, 0x9c, 0x28, 0x5c, 0xdb, 0x77, 0x7e, 0x36, 0x02, 0x00, 0x00, 0x00,
       0xe8, 0x03, 0x00, 0x00, 0x47, 0x75, 0x00, 0x00, 0x03, 0x05, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xf2, 0x7b, 0x00, 0x00, 0x02, 0x15, 0x00, 0x00,
       0x00, 0x61, 0x6e, 0x64, 0x72, 0x6f, 0x69, 0x64, 0x2e, 0x70, 0x72, 0x6f,
       0x63, 0x65, 0x73, 0x73, 0x2e, 0x61, 0x63, 0x6f, 0x72, 0x65, 0x00, 0xd9,
       0x03, 0x00, 0x00, 0x02, 0x09, 0x00, 0x00, 0x00, 0x65, 0x6d, 0x70, 0x74,
       0x79, 0x20, 0x23, 0x31, 0x37},

      // 12-30 10:22:08.946 29981 30962 I am_uid_stopped: 10018
      {0x09, 0x00, 0x1c, 0x00, 0x1d, 0x75, 0x00, 0x00, 0xf2, 0x78,
       0x00, 0x00, 0x50, 0x9c, 0x28, 0x5c, 0x24, 0x5a, 0x66, 0x38,
       0x02, 0x00, 0x00, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x65, 0x75,
       0x00, 0x00, 0x00, 0x22, 0x27, 0x00, 0x00},

      // 12-30 10:22:08.960 29981 29998 I am_pss  :
      // [1417,10098,com.google.android.connectivitymonitor,4831232,3723264,0,56053760,0,9,39]
      {0x72, 0x00, 0x1c, 0x00, 0x1d, 0x75, 0x00, 0x00, 0x2e, 0x75, 0x00, 0x00,
       0x50, 0x9c, 0x28, 0x5c, 0xf4, 0xd7, 0x44, 0x39, 0x02, 0x00, 0x00, 0x00,
       0xe8, 0x03, 0x00, 0x00, 0x5f, 0x75, 0x00, 0x00, 0x03, 0x0a, 0x00, 0x89,
       0x05, 0x00, 0x00, 0x00, 0x72, 0x27, 0x00, 0x00, 0x02, 0x26, 0x00, 0x00,
       0x00, 0x63, 0x6f, 0x6d, 0x2e, 0x67, 0x6f, 0x6f, 0x67, 0x6c, 0x65, 0x2e,
       0x61, 0x6e, 0x64, 0x72, 0x6f, 0x69, 0x64, 0x2e, 0x63, 0x6f, 0x6e, 0x6e,
       0x65, 0x63, 0x74, 0x69, 0x76, 0x69, 0x74, 0x79, 0x6d, 0x6f, 0x6e, 0x69,
       0x74, 0x6f, 0x72, 0x01, 0x00, 0xb8, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x01, 0x00, 0xd0, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x50, 0x57, 0x03, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00,
       0x00, 0x01, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
};

TEST_F(AndroidLogDataSourceTest, ParseEventLogDefinitions) {
  CreateInstance(DataSourceConfig());
  static const char kContents[] = R"(
42 answer (to life the universe etc|3)
314 pi
1003 auditd (avc|3)
1004 chatty (dropped|3)
1005 tag_def (tag|1),(name|3),(format|3)
2718 e
2732 storaged_disk_stats (type|3),(start_time|2|3),(end_time|2|3),(read_ios|2|1),(read_merges|2|1),(read_sectors|2|1),(read_ticks|2|3),(write_ios|2|1),(write_merges|2|1),(write_sectors|2|1),(write_ticks|2|3),(o_in_flight|2|1),(io_ticks|2|3),(io_in_queue|2|1)
invalid_line (
9999 invalid_line2 (
1937006964 stats_log (atom_id|1|5),(data|4)
)";
  EXPECT_CALL(*data_source_, ReadEventLogDefinitions())
      .WillOnce(Return(kContents));
  data_source_->ParseEventLogDefinitions();

  auto* fmt = data_source_->GetEventFormat(42);
  ASSERT_NE(fmt, nullptr);
  ASSERT_EQ(fmt->name, "answer");
  ASSERT_EQ(fmt->fields.size(), 1u);
  ASSERT_EQ(fmt->fields[0], "to life the universe etc");

  fmt = data_source_->GetEventFormat(314);
  ASSERT_NE(fmt, nullptr);
  ASSERT_EQ(fmt->name, "pi");
  ASSERT_EQ(fmt->fields.size(), 0u);

  fmt = data_source_->GetEventFormat(1005);
  ASSERT_NE(fmt, nullptr);
  ASSERT_EQ(fmt->name, "tag_def");
  ASSERT_EQ(fmt->fields.size(), 3u);
  ASSERT_EQ(fmt->fields[0], "tag");
  ASSERT_EQ(fmt->fields[1], "name");
  ASSERT_EQ(fmt->fields[2], "format");

  fmt = data_source_->GetEventFormat(1937006964);
  ASSERT_NE(fmt, nullptr);
  ASSERT_EQ(fmt->name, "stats_log");
  ASSERT_EQ(fmt->fields.size(), 2u);
  ASSERT_EQ(fmt->fields[0], "atom_id");
  ASSERT_EQ(fmt->fields[1], "data");
}

TEST_F(AndroidLogDataSourceTest, TextEvents) {
  DataSourceConfig cfg;
  CreateInstance(cfg);
  EXPECT_CALL(*data_source_, ReadEventLogDefinitions()).WillOnce(Return(""));
  StartAndSimulateLogd(kValidTextEvents);

  // Read back the data that would have been written into the trace. One packet
  // with the events, one with stats.
  auto packets = writer_raw_->GetAllTracePackets();
  ASSERT_TRUE(packets.size() == 2);
  auto event_packet = packets[0];
  auto stats_packet = packets[1];
  EXPECT_TRUE(stats_packet.android_log().has_stats());

  EXPECT_EQ(event_packet.android_log().events_size(), 3);
  const auto& decoded = event_packet.android_log().events();

  EXPECT_EQ(decoded[0].log_id(), protos::gen::AndroidLogId::LID_SYSTEM);
  EXPECT_EQ(decoded[0].pid(), 7546);
  EXPECT_EQ(decoded[0].tid(), 8991);
  EXPECT_EQ(decoded[0].uid(), 1000);
  EXPECT_EQ(decoded[0].prio(), protos::gen::AndroidLogPriority::PRIO_INFO);
  EXPECT_EQ(decoded[0].timestamp(), 1546125239679172326ULL);
  EXPECT_EQ(decoded[0].tag(), "ActivityManager");
  EXPECT_EQ(
      decoded[0].message(),
      "Killing 11660:com.google.android.videos/u0a168 (adj 985): empty #17");

  EXPECT_EQ(decoded[1].log_id(), protos::gen::AndroidLogId::LID_DEFAULT);
  EXPECT_EQ(decoded[1].pid(), 7546);
  EXPECT_EQ(decoded[1].tid(), 7570);
  EXPECT_EQ(decoded[1].uid(), 1000);
  EXPECT_EQ(decoded[1].prio(), protos::gen::AndroidLogPriority::PRIO_WARN);
  EXPECT_EQ(decoded[1].timestamp(), 1546125239683537170ULL);
  EXPECT_EQ(decoded[1].tag(), "libprocessgroup");
  EXPECT_EQ(decoded[1].message(), "kill(-11660, 9) failed: No such process");

  EXPECT_EQ(decoded[2].log_id(), protos::gen::AndroidLogId::LID_DEFAULT);
  EXPECT_EQ(decoded[2].pid(), 7415);
  EXPECT_EQ(decoded[2].tid(), 7415);
  EXPECT_EQ(decoded[2].uid(), 0);
  EXPECT_EQ(decoded[2].prio(), protos::gen::AndroidLogPriority::PRIO_INFO);
  EXPECT_EQ(decoded[2].timestamp(), 1546125239719458684ULL);
  EXPECT_EQ(decoded[2].tag(), "Zygote");
  EXPECT_EQ(decoded[2].message(), "Process 11660 exited due to signal (9)");
}

TEST_F(AndroidLogDataSourceTest, TextEventsWithTagFiltering) {
  DataSourceConfig cfg;
  AndroidLogConfig acfg;
  acfg.add_filter_tags("Zygote");
  acfg.add_filter_tags("ActivityManager");
  acfg.add_filter_tags("Unmatched");
  acfg.add_filter_tags("libprocessgroupZZ");
  cfg.set_android_log_config_raw(acfg.SerializeAsString());

  CreateInstance(cfg);
  EXPECT_CALL(*data_source_, ReadEventLogDefinitions()).WillOnce(Return(""));
  StartAndSimulateLogd(kValidTextEvents);

  auto packets = writer_raw_->GetAllTracePackets();
  ASSERT_TRUE(packets.size() == 2);
  auto event_packet = packets[0];
  auto stats_packet = packets[1];
  EXPECT_TRUE(stats_packet.android_log().has_stats());

  EXPECT_EQ(event_packet.android_log().events_size(), 2);
  const auto& decoded = event_packet.android_log().events();
  EXPECT_EQ(decoded[0].tag(), "ActivityManager");
  EXPECT_EQ(decoded[1].tag(), "Zygote");
}

TEST_F(AndroidLogDataSourceTest, TextEventsWithPrioFiltering) {
  DataSourceConfig cfg;
  AndroidLogConfig acfg;
  acfg.set_min_prio(protos::gen::AndroidLogPriority::PRIO_WARN);
  cfg.set_android_log_config_raw(acfg.SerializeAsString());

  CreateInstance(cfg);
  EXPECT_CALL(*data_source_, ReadEventLogDefinitions()).WillOnce(Return(""));
  StartAndSimulateLogd(kValidTextEvents);

  auto packets = writer_raw_->GetAllTracePackets();
  ASSERT_TRUE(packets.size() == 2);
  auto event_packet = packets[0];
  auto stats_packet = packets[1];
  EXPECT_TRUE(stats_packet.android_log().has_stats());

  EXPECT_EQ(event_packet.android_log().events_size(), 1);
  const auto& decoded = event_packet.android_log().events();
  EXPECT_EQ(decoded[0].tag(), "libprocessgroup");
}

TEST_F(AndroidLogDataSourceTest, BinaryEvents) {
  DataSourceConfig cfg;
  CreateInstance(cfg);
  static const char kDefs[] = R"(
30023 am_kill (User|1|5),(PID|1|5),(Process Name|3),(OomAdj|1|5),(Reason|3)
30053 am_uid_stopped (UID|1|5)
30047 am_pss (Pid|1|5),(UID|1|5),(Process Name|3),(Pss|2|2),(Uss|2|2),(SwapPss|2|2),(Rss|2|2),(StatType|1|5),(ProcState|1|5),(TimeToCollect|2|2)
)";
  EXPECT_CALL(*data_source_, ReadEventLogDefinitions()).WillOnce(Return(kDefs));
  StartAndSimulateLogd(kValidBinaryEvents);

  // Read back the data that would have been written into the trace. One packet
  // with the events, one with stats.
  auto packets = writer_raw_->GetAllTracePackets();
  ASSERT_TRUE(packets.size() == 2);
  auto event_packet = packets[0];
  auto stats_packet = packets[1];
  EXPECT_TRUE(stats_packet.android_log().has_stats());

  EXPECT_EQ(event_packet.android_log().events_size(), 3);
  const auto& decoded = event_packet.android_log().events();

  EXPECT_EQ(decoded[0].log_id(), protos::gen::AndroidLogId::LID_EVENTS);
  EXPECT_EQ(decoded[0].pid(), 29981);
  EXPECT_EQ(decoded[0].tid(), 30962);
  EXPECT_EQ(decoded[0].uid(), 1000);
  EXPECT_EQ(decoded[0].timestamp(), 1546165328914257883ULL);
  EXPECT_EQ(decoded[0].tag(), "am_kill");
  ASSERT_EQ(decoded[0].args_size(), 5);
  EXPECT_EQ(decoded[0].args()[0].name(), "User");
  EXPECT_EQ(decoded[0].args()[0].int_value(), 0);
  EXPECT_EQ(decoded[0].args()[1].name(), "PID");
  EXPECT_EQ(decoded[0].args()[1].int_value(), 31730);
  EXPECT_EQ(decoded[0].args()[2].name(), "Process Name");
  EXPECT_EQ(decoded[0].args()[2].string_value(), "android.process.acore");
  EXPECT_EQ(decoded[0].args()[3].name(), "OomAdj");
  EXPECT_EQ(decoded[0].args()[3].int_value(), 985);
  EXPECT_EQ(decoded[0].args()[4].name(), "Reason");
  EXPECT_EQ(decoded[0].args()[4].string_value(), "empty #17");

  EXPECT_EQ(decoded[1].log_id(), protos::gen::AndroidLogId::LID_EVENTS);
  EXPECT_EQ(decoded[1].pid(), 29981);
  EXPECT_EQ(decoded[1].tid(), 30962);
  EXPECT_EQ(decoded[1].uid(), 1000);
  EXPECT_EQ(decoded[1].timestamp(), 1546165328946231844ULL);
  EXPECT_EQ(decoded[1].tag(), "am_uid_stopped");
  ASSERT_EQ(decoded[1].args_size(), 1);
  EXPECT_EQ(decoded[1].args()[0].name(), "UID");
  EXPECT_EQ(decoded[1].args()[0].int_value(), 10018);

  EXPECT_EQ(decoded[2].log_id(), protos::gen::AndroidLogId::LID_EVENTS);
  EXPECT_EQ(decoded[2].pid(), 29981);
  EXPECT_EQ(decoded[2].tid(), 29998);
  EXPECT_EQ(decoded[2].uid(), 1000);
  EXPECT_EQ(decoded[2].timestamp(), 1546165328960813044ULL);
  EXPECT_EQ(decoded[2].tag(), "am_pss");
  ASSERT_EQ(decoded[2].args_size(), 10);
  EXPECT_EQ(decoded[2].args()[0].name(), "Pid");
  EXPECT_EQ(decoded[2].args()[0].int_value(), 1417);
  EXPECT_EQ(decoded[2].args()[1].name(), "UID");
  EXPECT_EQ(decoded[2].args()[1].int_value(), 10098);
  EXPECT_EQ(decoded[2].args()[2].name(), "Process Name");
  EXPECT_EQ(decoded[2].args()[2].string_value(),
            "com.google.android.connectivitymonitor");
  EXPECT_EQ(decoded[2].args()[3].name(), "Pss");
  EXPECT_EQ(decoded[2].args()[3].int_value(), 4831232);
  EXPECT_EQ(decoded[2].args()[4].name(), "Uss");
  EXPECT_EQ(decoded[2].args()[4].int_value(), 3723264);
  EXPECT_EQ(decoded[2].args()[5].name(), "SwapPss");
  EXPECT_EQ(decoded[2].args()[5].int_value(), 0);
  EXPECT_EQ(decoded[2].args()[6].name(), "Rss");
  EXPECT_EQ(decoded[2].args()[6].int_value(), 56053760);
  EXPECT_EQ(decoded[2].args()[7].name(), "StatType");
  EXPECT_EQ(decoded[2].args()[7].int_value(), 0);
  EXPECT_EQ(decoded[2].args()[8].name(), "ProcState");
  EXPECT_EQ(decoded[2].args()[8].int_value(), 9);
  EXPECT_EQ(decoded[2].args()[9].name(), "TimeToCollect");
  EXPECT_EQ(decoded[2].args()[9].int_value(), 39);
}

TEST_F(AndroidLogDataSourceTest, BinaryEventsWithTagFiltering) {
  DataSourceConfig cfg;
  AndroidLogConfig acfg;
  acfg.add_filter_tags("not mached");
  acfg.add_filter_tags("am_uid_stopped");
  cfg.set_android_log_config_raw(acfg.SerializeAsString());
  CreateInstance(cfg);
  static const char kDefs[] = R"(
30023 am_kill (User|1|5),(PID|1|5),(Process Name|3),(OomAdj|1|5),(Reason|3)
30053 am_uid_stopped (UID|1|5)
30047 am_pss (Pid|1|5),(UID|1|5),(Process Name|3),(Pss|2|2),(Uss|2|2),(SwapPss|2|2),(Rss|2|2),(StatType|1|5),(ProcState|1|5),(TimeToCollect|2|2)
)";
  EXPECT_CALL(*data_source_, ReadEventLogDefinitions()).WillOnce(Return(kDefs));
  StartAndSimulateLogd(kValidBinaryEvents);

  // Read back the data that would have been written into the trace. One packet
  // with the events, one with stats.
  auto packets = writer_raw_->GetAllTracePackets();
  ASSERT_TRUE(packets.size() == 2);
  auto event_packet = packets[0];
  auto stats_packet = packets[1];
  EXPECT_TRUE(stats_packet.android_log().has_stats());

  EXPECT_EQ(event_packet.android_log().events_size(), 1);
  const auto& decoded = event_packet.android_log().events();
  EXPECT_EQ(decoded[0].timestamp(), 1546165328946231844ULL);
  EXPECT_EQ(decoded[0].tag(), "am_uid_stopped");
}

}  // namespace
}  // namespace perfetto
