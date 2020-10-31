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

#include "src/traced/probes/ftrace/ftrace_config_muxer.h"

#include <memory>

#include "src/traced/probes/ftrace/atrace_wrapper.h"
#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/proto_translation_table.h"
#include "test/gtest_and_gmock.h"

using testing::_;
using testing::AnyNumber;
using testing::MatchesRegex;
using testing::Contains;
using testing::ElementsAreArray;
using testing::Eq;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Not;
using testing::Return;

namespace perfetto {
namespace {

class MockFtraceProcfs : public FtraceProcfs {
 public:
  MockFtraceProcfs() : FtraceProcfs("/root/") {
    ON_CALL(*this, NumberOfCpus()).WillByDefault(Return(1));
    ON_CALL(*this, WriteToFile(_, _)).WillByDefault(Return(true));
    ON_CALL(*this, ClearFile(_)).WillByDefault(Return(true));
    EXPECT_CALL(*this, NumberOfCpus()).Times(AnyNumber());
  }

  MOCK_METHOD2(WriteToFile,
               bool(const std::string& path, const std::string& str));
  MOCK_METHOD2(AppendToFile,
               bool(const std::string& path, const std::string& str));
  MOCK_METHOD1(ReadOneCharFromFile, char(const std::string& path));
  MOCK_METHOD1(ClearFile, bool(const std::string& path));
  MOCK_CONST_METHOD1(ReadFileIntoString, std::string(const std::string& path));
  MOCK_CONST_METHOD0(NumberOfCpus, size_t());
  MOCK_CONST_METHOD1(GetEventNamesForGroup,
                     const std::set<std::string>(const std::string& path));
};

struct MockRunAtrace {
  MockRunAtrace() {
    static MockRunAtrace* instance;
    instance = this;
    SetRunAtraceForTesting([](const std::vector<std::string>& args) {
      return instance->RunAtrace(args);
    });
  }

  ~MockRunAtrace() { SetRunAtraceForTesting(nullptr); }

  MOCK_METHOD1(RunAtrace, bool(const std::vector<std::string>&));
};

class MockProtoTranslationTable : public ProtoTranslationTable {
 public:
  MockProtoTranslationTable(NiceMock<MockFtraceProcfs>* ftrace_procfs,
                            const std::vector<Event>& events,
                            std::vector<Field> common_fields,
                            FtracePageHeaderSpec ftrace_page_header_spec,
                            CompactSchedEventFormat compact_sched_format)
      : ProtoTranslationTable(ftrace_procfs,
                              events,
                              common_fields,
                              ftrace_page_header_spec,
                              compact_sched_format) {}
  MOCK_METHOD1(GetOrCreateEvent, Event*(const GroupAndName& group_and_name));
  MOCK_CONST_METHOD1(GetEvent,
                     const Event*(const GroupAndName& group_and_name));
};

class FtraceConfigMuxerTest : public ::testing::Test {
 protected:
  std::unique_ptr<MockProtoTranslationTable> GetMockTable() {
    std::vector<Field> common_fields;
    std::vector<Event> events;
    return std::unique_ptr<MockProtoTranslationTable>(
        new MockProtoTranslationTable(
            &table_procfs_, events, std::move(common_fields),
            ProtoTranslationTable::DefaultPageHeaderSpecForTesting(),
            InvalidCompactSchedEventFormatForTesting()));
  }

  static constexpr int kFakeSchedSwitchEventId = 1;
  static constexpr int kCgroupMkdirEventId = 12;
  static constexpr int kFakePrintEventId = 20;

  std::unique_ptr<ProtoTranslationTable> CreateFakeTable(
      CompactSchedEventFormat compact_format =
          InvalidCompactSchedEventFormatForTesting()) {
    std::vector<Field> common_fields;
    std::vector<Event> events;
    {
      Event event;
      event.name = "sched_switch";
      event.group = "sched";
      event.ftrace_event_id = kFakeSchedSwitchEventId;
      events.push_back(event);
    }

    {
      Event event;
      event.name = "sched_wakeup";
      event.group = "sched";
      event.ftrace_event_id = 10;
      events.push_back(event);
    }

    {
      Event event;
      event.name = "sched_new";
      event.group = "sched";
      event.ftrace_event_id = 11;
      events.push_back(event);
    }

    {
      Event event;
      event.name = "cgroup_mkdir";
      event.group = "cgroup";
      event.ftrace_event_id = kCgroupMkdirEventId;
      events.push_back(event);
    }

    {
      Event event;
      event.name = "mm_vmscan_direct_reclaim_begin";
      event.group = "vmscan";
      event.ftrace_event_id = 13;
      events.push_back(event);
    }

    {
      Event event;
      event.name = "lowmemory_kill";
      event.group = "lowmemorykiller";
      event.ftrace_event_id = 14;
      events.push_back(event);
    }

    {
      Event event;
      event.name = "print";
      event.group = "ftrace";
      event.ftrace_event_id = kFakePrintEventId;
      events.push_back(event);
    }

    return std::unique_ptr<ProtoTranslationTable>(new ProtoTranslationTable(
        &table_procfs_, events, std::move(common_fields),
        ProtoTranslationTable::DefaultPageHeaderSpecForTesting(),
        compact_format));
  }

  NiceMock<MockFtraceProcfs> table_procfs_;
  std::unique_ptr<ProtoTranslationTable> table_ = CreateFakeTable();
};

TEST_F(FtraceConfigMuxerTest, ComputeCpuBufferSizeInPages) {
  static constexpr size_t kMaxBufSizeInPages = 16 * 1024u;
  // No buffer size given: good default (128 pages = 2mb).
  EXPECT_EQ(ComputeCpuBufferSizeInPages(0), 512u);
  // Buffer size given way too big: good default.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(10 * 1024 * 1024), kMaxBufSizeInPages);
  // The limit is 64mb per CPU, 512mb is too much.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(512 * 1024), kMaxBufSizeInPages);
  // Your size ends up with less than 1 page per cpu -> 1 page.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(3), 1u);
  // You picked a good size -> your size rounded to nearest page.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(42), 10u);
}

TEST_F(FtraceConfigMuxerTest, AddGenericEvent) {
  auto mock_table = GetMockTable();
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"power/cpu_frequency"});

  FtraceConfigMuxer model(&ftrace, mock_table.get(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .Times(2)
      .WillRepeatedly(Return('0'));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/power/cpu_frequency/enable", "1"));
  EXPECT_CALL(*mock_table, GetEvent(GroupAndName("power", "cpu_frequency")))
      .Times(AnyNumber());

  static constexpr int kExpectedEventId = 77;
  Event event_to_return;
  event_to_return.name = "cpu_frequency";
  event_to_return.group = "power";
  event_to_return.ftrace_event_id = kExpectedEventId;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("power", "cpu_frequency")))
      .WillByDefault(Return(&event_to_return));
  EXPECT_CALL(*mock_table,
              GetOrCreateEvent(GroupAndName("power", "cpu_frequency")));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kExpectedEventId}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kExpectedEventId}));
}

TEST_F(FtraceConfigMuxerTest, AddSameNameEvents) {
  auto mock_table = GetMockTable();
  NiceMock<MockFtraceProcfs> ftrace;

  FtraceConfig config = CreateFtraceConfig({"group_one/foo", "group_two/foo"});

  FtraceConfigMuxer model(&ftrace, mock_table.get(), {});

  static constexpr int kEventId1 = 1;
  Event event1;
  event1.name = "foo";
  event1.group = "group_one";
  event1.ftrace_event_id = kEventId1;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")))
      .WillByDefault(Return(&event1));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")));

  static constexpr int kEventId2 = 2;
  Event event2;
  event2.name = "foo";
  event2.group = "group_two";
  event2.ftrace_event_id = kEventId2;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")))
      .WillByDefault(Return(&event2));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));
}

TEST_F(FtraceConfigMuxerTest, AddAllEvents) {
  auto mock_table = GetMockTable();
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"sched/*"});

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .Times(2)
      .WillRepeatedly(Return('0'));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_new_event/enable", "1"));

  FtraceConfigMuxer model(&ftrace, mock_table.get(), {});
  std::set<std::string> n = {"sched_switch", "sched_new_event"};
  ON_CALL(ftrace, GetEventNamesForGroup("events/sched"))
      .WillByDefault(Return(n));
  EXPECT_CALL(ftrace, GetEventNamesForGroup("events/sched")).Times(1);

  // Non-generic event.
  static constexpr int kSchedSwitchEventId = 1;
  Event sched_switch = {"sched_switch", "sched", {}, 0, 0, 0};
  sched_switch.ftrace_event_id = kSchedSwitchEventId;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("sched", "sched_switch")))
      .WillByDefault(Return(&sched_switch));
  EXPECT_CALL(*mock_table,
              GetOrCreateEvent(GroupAndName("sched", "sched_switch")))
      .Times(AnyNumber());

  // Generic event.
  static constexpr int kGenericEventId = 2;
  Event event_to_return;
  event_to_return.name = "sched_new_event";
  event_to_return.group = "sched";
  event_to_return.ftrace_event_id = kGenericEventId;
  ON_CALL(*mock_table,
          GetOrCreateEvent(GroupAndName("sched", "sched_new_event")))
      .WillByDefault(Return(&event_to_return));
  EXPECT_CALL(*mock_table,
              GetOrCreateEvent(GroupAndName("sched", "sched_new_event")));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(id);
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kSchedSwitchEventId, kGenericEventId}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kSchedSwitchEventId, kGenericEventId}));
}

TEST_F(FtraceConfigMuxerTest, TwoWildcardGroups) {
  auto mock_table = GetMockTable();
  NiceMock<MockFtraceProcfs> ftrace;

  FtraceConfig config = CreateFtraceConfig({"group_one/*", "group_two/*"});

  FtraceConfigMuxer model(&ftrace, mock_table.get(), {});

  std::set<std::string> event_names = {"foo"};
  ON_CALL(ftrace, GetEventNamesForGroup("events/group_one"))
      .WillByDefault(Return(event_names));
  EXPECT_CALL(ftrace, GetEventNamesForGroup("events/group_one"))
      .Times(AnyNumber());

  ON_CALL(ftrace, GetEventNamesForGroup("events/group_two"))
      .WillByDefault(Return(event_names));
  EXPECT_CALL(ftrace, GetEventNamesForGroup("events/group_two"))
      .Times(AnyNumber());

  static constexpr int kEventId1 = 1;
  Event event1;
  event1.name = "foo";
  event1.group = "group_one";
  event1.ftrace_event_id = kEventId1;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")))
      .WillByDefault(Return(&event1));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")));

  static constexpr int kEventId2 = 2;
  Event event2;
  event2.name = "foo";
  event2.group = "group_two";
  event2.ftrace_event_id = kEventId2;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")))
      .WillByDefault(Return(&event2));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));
}

TEST_F(FtraceConfigMuxerTest, TurnFtraceOnOff) {
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"sched_switch", "foo"});

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .Times(2)
      .WillRepeatedly(Return('0'));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(id);
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(
      ds_config->event_filter.GetEnabledEvents(),
      ElementsAreArray({FtraceConfigMuxerTest::kFakeSchedSwitchEventId}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(
      central_filter->GetEnabledEvents(),
      ElementsAreArray({FtraceConfigMuxerTest::kFakeSchedSwitchEventId}));

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace));
  EXPECT_CALL(ftrace, NumberOfCpus()).Times(AnyNumber());

  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", "4"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));

  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, FtraceIsAlreadyOn) {
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  // If someone is using ftrace already don't stomp on what they are doing.
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_FALSE(id);
}

TEST_F(FtraceConfigMuxerTest, Atrace) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  *config.add_atrace_categories() = "sched";

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('0'));
  EXPECT_CALL(atrace,
              RunAtrace(ElementsAreArray(
                  {"atrace", "--async_start", "--only_userspace", "sched"})))
      .WillOnce(Return(true));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(id);

  // "ftrace" group events are always enabled, and therefore the "print" event
  // will show up in the per data source event filter (as we want to record it),
  // but not the central filter (as we're not enabling/disabling it).
  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakePrintEventId));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  EXPECT_THAT(central_filter->GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray(
                          {"atrace", "--async_stop", "--only_userspace"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, AtraceTwoApps) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_apps() = "com.google.android.gms.persistent";
  *config.add_atrace_apps() = "com.google.android.gms";

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('0'));
  EXPECT_CALL(
      atrace,
      RunAtrace(ElementsAreArray(
          {"atrace", "--async_start", "--only_userspace", "-a",
           "com.google.android.gms,com.google.android.gms.persistent"})))
      .WillOnce(Return(true));

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(id);

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakePrintEventId));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray(
                          {"atrace", "--async_stop", "--only_userspace"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, AtraceMultipleConfigs) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_apps() = "app_a";
  *config_a.add_atrace_categories() = "cat_a";

  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_apps() = "app_b";
  *config_b.add_atrace_categories() = "cat_b";

  FtraceConfig config_c = CreateFtraceConfig({});
  *config_c.add_atrace_apps() = "app_c";
  *config_c.add_atrace_categories() = "cat_c";

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_a",
                                                  "-a", "app_a"})))
      .WillOnce(Return(true));
  FtraceConfigId id_a = model.SetupConfig(config_a);
  ASSERT_TRUE(id_a);

  EXPECT_CALL(
      atrace,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_a", "cat_b", "-a", "app_a,app_b"})))
      .WillOnce(Return(true));
  FtraceConfigId id_b = model.SetupConfig(config_b);
  ASSERT_TRUE(id_b);

  EXPECT_CALL(atrace,
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "cat_a", "cat_b",
                                          "cat_c", "-a", "app_a,app_b,app_c"})))
      .WillOnce(Return(true));
  FtraceConfigId id_c = model.SetupConfig(config_c);
  ASSERT_TRUE(id_c);

  EXPECT_CALL(
      atrace,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_a", "cat_c", "-a", "app_a,app_c"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_b));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_c",
                                                  "-a", "app_c"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_a));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray(
                          {"atrace", "--async_stop", "--only_userspace"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_c));
}

TEST_F(FtraceConfigMuxerTest, AtraceFailedConfig) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_apps() = "app_1";
  *config_a.add_atrace_apps() = "app_2";
  *config_a.add_atrace_categories() = "cat_1";
  *config_a.add_atrace_categories() = "cat_2";

  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_apps() = "app_fail";
  *config_b.add_atrace_categories() = "cat_fail";

  FtraceConfig config_c = CreateFtraceConfig({});
  *config_c.add_atrace_apps() = "app_1";
  *config_c.add_atrace_apps() = "app_3";
  *config_c.add_atrace_categories() = "cat_1";
  *config_c.add_atrace_categories() = "cat_3";

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  EXPECT_CALL(
      atrace,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "-a", "app_1,app_2"})))
      .WillOnce(Return(true));
  FtraceConfigId id_a = model.SetupConfig(config_a);
  ASSERT_TRUE(id_a);

  EXPECT_CALL(atrace,
              RunAtrace(ElementsAreArray(
                  {"atrace", "--async_start", "--only_userspace", "cat_1",
                   "cat_2", "cat_fail", "-a", "app_1,app_2,app_fail"})))
      .WillOnce(Return(false));
  FtraceConfigId id_b = model.SetupConfig(config_b);
  ASSERT_TRUE(id_b);

  EXPECT_CALL(atrace,
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "cat_1", "cat_2",
                                          "cat_3", "-a", "app_1,app_2,app_3"})))
      .WillOnce(Return(true));
  FtraceConfigId id_c = model.SetupConfig(config_c);
  ASSERT_TRUE(id_c);

  EXPECT_CALL(
      atrace,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "-a", "app_1,app_2"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_c));

  // Removing the config we failed to enable doesn't change the atrace state
  // so we don't expect a call here.
  ASSERT_TRUE(model.RemoveConfig(id_b));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray(
                          {"atrace", "--async_stop", "--only_userspace"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerTest, AtraceDuplicateConfigs) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_apps() = "app_1";
  *config_a.add_atrace_categories() = "cat_1";

  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_apps() = "app_1";
  *config_b.add_atrace_categories() = "cat_1";

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_1",
                                                  "-a", "app_1"})))
      .WillOnce(Return(true));
  FtraceConfigId id_a = model.SetupConfig(config_a);
  ASSERT_TRUE(id_a);

  FtraceConfigId id_b = model.SetupConfig(config_b);
  ASSERT_TRUE(id_b);

  ASSERT_TRUE(model.RemoveConfig(id_a));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray(
                          {"atrace", "--async_stop", "--only_userspace"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_b));
}

TEST_F(FtraceConfigMuxerTest, AtraceAndFtraceConfigs) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config_a = CreateFtraceConfig({"sched/sched_cpu_hotplug"});

  FtraceConfig config_b = CreateFtraceConfig({"sched/sched_switch"});
  *config_b.add_atrace_categories() = "b";

  FtraceConfig config_c = CreateFtraceConfig({"sched/sched_switch"});

  FtraceConfig config_d = CreateFtraceConfig({"sched/sched_cpu_hotplug"});
  *config_d.add_atrace_categories() = "d";

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  FtraceConfigId id_a = model.SetupConfig(config_a);
  ASSERT_TRUE(id_a);

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "b"})))
      .WillOnce(Return(true));
  FtraceConfigId id_b = model.SetupConfig(config_b);
  ASSERT_TRUE(id_b);

  FtraceConfigId id_c = model.SetupConfig(config_c);
  ASSERT_TRUE(id_c);

  EXPECT_CALL(atrace,
              RunAtrace(ElementsAreArray(
                  {"atrace", "--async_start", "--only_userspace", "b", "d"})))
      .WillOnce(Return(true));
  FtraceConfigId id_d = model.SetupConfig(config_d);
  ASSERT_TRUE(id_d);

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "b"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_d));

  ASSERT_TRUE(model.RemoveConfig(id_c));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray(
                          {"atrace", "--async_stop", "--only_userspace"})))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_b));

  ASSERT_TRUE(model.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerTest, SetupClockForTesting) {
  MockFtraceProcfs ftrace;
  FtraceConfig config;

  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  model.SetupClockForTesting(config);

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global"));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "global"));
  model.SetupClockForTesting(config);

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return(""));
  model.SetupClockForTesting(config);

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("local [global]"));
  model.SetupClockForTesting(config);
}

TEST_F(FtraceConfigMuxerTest, GetFtraceEvents) {
  MockFtraceProcfs ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  std::set<GroupAndName> events =
      model.GetFtraceEventsForTesting(config, table_.get());

  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_switch")));
  EXPECT_THAT(events, Not(Contains(GroupAndName("ftrace", "print"))));
}

TEST_F(FtraceConfigMuxerTest, GetFtraceEventsAtrace) {
  MockFtraceProcfs ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_categories() = "sched";
  std::set<GroupAndName> events =
      model.GetFtraceEventsForTesting(config, table_.get());

  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_switch")));
  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_cpu_hotplug")));
  EXPECT_THAT(events, Contains(GroupAndName("ftrace", "print")));
}

TEST_F(FtraceConfigMuxerTest, GetFtraceEventsAtraceCategories) {
  MockFtraceProcfs ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_categories() = "sched";
  *config.add_atrace_categories() = "memreclaim";
  std::set<GroupAndName> events =
      model.GetFtraceEventsForTesting(config, table_.get());

  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_switch")));
  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_cpu_hotplug")));
  EXPECT_THAT(events, Contains(GroupAndName("cgroup", "cgroup_mkdir")));
  EXPECT_THAT(events, Contains(GroupAndName("vmscan",
                                            "mm_vmscan_direct_reclaim_begin")));
  EXPECT_THAT(events,
              Contains(GroupAndName("lowmemorykiller", "lowmemory_kill")));
  EXPECT_THAT(events, Contains(GroupAndName("ftrace", "print")));
}

// Tests the enabling fallback logic that tries to use the "set_event" interface
// if writing the individual xxx/enable file fails.
TEST_F(FtraceConfigMuxerTest, FallbackOnSetEvent) {
  MockFtraceProcfs ftrace;
  FtraceConfig config =
      CreateFtraceConfig({"sched/sched_switch", "cgroup/cgroup_mkdir"});
  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .Times(2)
      .WillRepeatedly(Return('0'));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/cgroup/cgroup_mkdir/enable", "1"))
      .WillOnce(Return(false));
  EXPECT_CALL(ftrace, AppendToFile("/root/set_event", "cgroup:cgroup_mkdir"))
      .WillOnce(Return(true));
  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(id);
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kCgroupMkdirEventId));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  EXPECT_THAT(central_filter->GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));
  EXPECT_THAT(central_filter->GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kCgroupMkdirEventId));

  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", "4"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "0"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/cgroup/cgroup_mkdir/enable", "0"))
      .WillOnce(Return(false));
  EXPECT_CALL(ftrace, AppendToFile("/root/set_event", "!cgroup:cgroup_mkdir"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, CompactSchedConfig) {
  // Set scheduling event format as validated. The pre-parsed format itself
  // doesn't need to be sensible, as the tests won't use it.
  auto valid_compact_format =
      CompactSchedEventFormat{/*format_valid=*/true, CompactSchedSwitchFormat{},
                              CompactSchedWakingFormat{}};

  NiceMock<MockFtraceProcfs> ftrace;
  table_ = CreateFakeTable(valid_compact_format);
  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  // First data source - request compact encoding.
  FtraceConfig config_enabled = CreateFtraceConfig({"sched/sched_switch"});
  config_enabled.mutable_compact_sched()->set_enabled(true);

  // Second data source - no compact encoding (default).
  FtraceConfig config_disabled = CreateFtraceConfig({"sched/sched_switch"});

  {
    FtraceConfigId id = model.SetupConfig(config_enabled);
    ASSERT_TRUE(id);
    const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));
    EXPECT_TRUE(ds_config->compact_sched.enabled);
  }
  {
    FtraceConfigId id = model.SetupConfig(config_disabled);
    ASSERT_TRUE(id);
    const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));
    EXPECT_FALSE(ds_config->compact_sched.enabled);
  }
}

TEST_F(FtraceConfigMuxerTest, CompactSchedConfigWithInvalidFormat) {
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), {});

  // Request compact encoding.
  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  config.mutable_compact_sched()->set_enabled(true);

  FtraceConfigId id = model.SetupConfig(config);
  ASSERT_TRUE(id);

  // The translation table says that the scheduling events' format didn't match
  // compile-time assumptions, so we won't enable compact events even if
  // requested.
  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(FtraceConfigMuxerTest::kFakeSchedSwitchEventId));
  EXPECT_FALSE(ds_config->compact_sched.enabled);
}

}  // namespace
}  // namespace perfetto
