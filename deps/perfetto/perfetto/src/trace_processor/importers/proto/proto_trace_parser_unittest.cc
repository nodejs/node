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

#include "src/trace_processor/importers/proto/proto_trace_tokenizer.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/trace_processor/importers/additional_modules.h"
#include "src/trace_processor/importers/common/args_tracker.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/slice_tracker.h"
#include "src/trace_processor/importers/common/track_tracker.h"
#include "src/trace_processor/importers/default_modules.h"
#include "src/trace_processor/importers/ftrace/sched_event_tracker.h"
#include "src/trace_processor/importers/proto/metadata_tracker.h"
#include "src/trace_processor/importers/proto/proto_trace_parser.h"
#include "src/trace_processor/importers/proto/stack_profile_tracker.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/trace_sorter.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/common/sys_stats_counters.pbzero.h"
#include "protos/perfetto/trace/android/packages_list.pbzero.h"
#include "protos/perfetto/trace/chrome/chrome_benchmark_metadata.pbzero.h"
#include "protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
#include "protos/perfetto/trace/ftrace/ftrace_event_bundle.pbzero.h"
#include "protos/perfetto/trace/ftrace/generic.pbzero.h"
#include "protos/perfetto/trace/ftrace/power.pbzero.h"
#include "protos/perfetto/trace/ftrace/sched.pbzero.h"
#include "protos/perfetto/trace/ftrace/task.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "protos/perfetto/trace/ps/process_tree.pbzero.h"
#include "protos/perfetto/trace/sys_stats/sys_stats.pbzero.h"
#include "protos/perfetto/trace/trace.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/track_event/counter_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "protos/perfetto/trace/track_event/task_execution.pbzero.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace perfetto {
namespace trace_processor {
namespace {

using ::testing::_;
using ::testing::Args;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::NiceMock;
using ::testing::Pointwise;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::UnorderedElementsAreArray;

namespace {
MATCHER_P(DoubleEq, exp, "Double matcher that satisfies -Wfloat-equal") {
  // The IEEE standard says that any comparison operation involving
  // a NAN must return false.
  double d_exp = exp;
  double d_arg = arg;
  if (isnan(d_exp) || isnan(d_arg))
    return false;
  return fabs(d_arg - d_exp) < 1e-128;
}
}  // namespace

class MockSchedEventTracker : public SchedEventTracker {
 public:
  MockSchedEventTracker(TraceProcessorContext* context)
      : SchedEventTracker(context) {}

  MOCK_METHOD9(PushSchedSwitch,
               void(uint32_t cpu,
                    int64_t timestamp,
                    uint32_t prev_pid,
                    base::StringView prev_comm,
                    int32_t prev_prio,
                    int64_t prev_state,
                    uint32_t next_pid,
                    base::StringView next_comm,
                    int32_t next_prio));
};

class MockEventTracker : public EventTracker {
 public:
  MockEventTracker(TraceProcessorContext* context) : EventTracker(context) {}
  virtual ~MockEventTracker() = default;

  MOCK_METHOD9(PushSchedSwitch,
               void(uint32_t cpu,
                    int64_t timestamp,
                    uint32_t prev_pid,
                    base::StringView prev_comm,
                    int32_t prev_prio,
                    int64_t prev_state,
                    uint32_t next_pid,
                    base::StringView next_comm,
                    int32_t next_prio));

  MOCK_METHOD3(PushCounter,
               base::Optional<CounterId>(int64_t timestamp,
                                         double value,
                                         TrackId track_id));
};

class MockProcessTracker : public ProcessTracker {
 public:
  MockProcessTracker(TraceProcessorContext* context)
      : ProcessTracker(context) {}

  MOCK_METHOD3(SetProcessMetadata,
               UniquePid(uint32_t pid,
                         base::Optional<uint32_t> ppid,
                         base::StringView process_name));

  MOCK_METHOD2(UpdateThreadName,
               UniqueTid(uint32_t tid, StringId thread_name_id));
  MOCK_METHOD2(SetThreadNameIfUnset,
               void(UniqueTid utid, StringId thread_name_id));
  MOCK_METHOD2(UpdateThread, UniqueTid(uint32_t tid, uint32_t tgid));

  MOCK_METHOD1(GetOrCreateProcess, UniquePid(uint32_t pid));
  MOCK_METHOD2(SetProcessNameIfUnset,
               void(UniquePid upid, StringId process_name_id));
};

class MockBoundInserter : public ArgsTracker::BoundInserter {
 public:
  MockBoundInserter()
      : ArgsTracker::BoundInserter(&tracker_, nullptr, 0u), tracker_(nullptr) {
    ON_CALL(*this, AddArg(_, _, _, _)).WillByDefault(ReturnRef(*this));
  }

  MOCK_METHOD4(
      AddArg,
      ArgsTracker::BoundInserter&(StringId flat_key,
                                  StringId key,
                                  Variadic v,
                                  ArgsTracker::UpdatePolicy update_policy));

 private:
  ArgsTracker tracker_;
};

class MockSliceTracker : public SliceTracker {
 public:
  MockSliceTracker(TraceProcessorContext* context) : SliceTracker(context) {}

  MOCK_METHOD5(Begin,
               base::Optional<uint32_t>(int64_t timestamp,
                                        TrackId track_id,
                                        StringId cat,
                                        StringId name,
                                        SetArgsCallback args_callback));
  MOCK_METHOD5(End,
               base::Optional<uint32_t>(int64_t timestamp,
                                        TrackId track_id,
                                        StringId cat,
                                        StringId name,
                                        SetArgsCallback args_callback));
  MOCK_METHOD6(Scoped,
               base::Optional<uint32_t>(int64_t timestamp,
                                        TrackId track_id,
                                        StringId cat,
                                        StringId name,
                                        int64_t duration,
                                        SetArgsCallback args_callback));
};

class ProtoTraceParserTest : public ::testing::Test {
 public:
  ProtoTraceParserTest() {
    storage_ = new TraceStorage();
    context_.storage.reset(storage_);
    context_.track_tracker.reset(new TrackTracker(&context_));
    context_.global_args_tracker.reset(new GlobalArgsTracker(&context_));
    context_.args_tracker.reset(new ArgsTracker(&context_));
    context_.metadata_tracker.reset(new MetadataTracker(&context_));
    event_ = new MockEventTracker(&context_);
    context_.event_tracker.reset(event_);
    sched_ = new MockSchedEventTracker(&context_);
    context_.sched_tracker.reset(sched_);
    process_ = new MockProcessTracker(&context_);
    context_.process_tracker.reset(process_);
    slice_ = new MockSliceTracker(&context_);
    context_.slice_tracker.reset(slice_);
    clock_ = new ClockTracker(&context_);
    context_.clock_tracker.reset(clock_);
    context_.sorter.reset(new TraceSorter(CreateParser(), 0 /*window size*/));

    RegisterDefaultModules(&context_);
    RegisterAdditionalModules(&context_);
  }

  void ResetTraceBuffers() {
    heap_buf_.reset(new protozero::ScatteredHeapBuffer());
    stream_writer_.reset(new protozero::ScatteredStreamWriter(heap_buf_.get()));
    heap_buf_->set_writer(stream_writer_.get());
    trace_.Reset(stream_writer_.get());
  }

  void SetUp() override { ResetTraceBuffers(); }

  util::Status Tokenize() {
    trace_.Finalize();
    std::vector<uint8_t> trace_bytes = heap_buf_->StitchSlices();
    std::unique_ptr<uint8_t[]> raw_trace(new uint8_t[trace_bytes.size()]);
    memcpy(raw_trace.get(), trace_bytes.data(), trace_bytes.size());
    context_.chunk_reader.reset(new ProtoTraceTokenizer(&context_));
    auto status =
        context_.chunk_reader->Parse(std::move(raw_trace), trace_bytes.size());

    ResetTraceBuffers();
    return status;
  }

  bool HasArg(ArgSetId set_id, StringId key_id, Variadic value) {
    const auto& args = storage_->arg_table();
    RowMap rm = args.FilterToRowMap({args.arg_set_id().eq(set_id)});
    bool found = false;
    for (auto it = rm.IterateRows(); it; it.Next()) {
      if (args.key()[it.row()] == key_id) {
        EXPECT_EQ(args.flat_key()[it.row()], key_id);
        if (storage_->GetArgValue(it.row()) == value) {
          found = true;
          break;
        }
      }
    }
    return found;
  }

 protected:
  std::unique_ptr<TraceParser> CreateParser() {
    return std::unique_ptr<TraceParser>(new ProtoTraceParser(&context_));
  }

  std::unique_ptr<protozero::ScatteredHeapBuffer> heap_buf_;
  std::unique_ptr<protozero::ScatteredStreamWriter> stream_writer_;
  protos::pbzero::Trace trace_;
  TraceProcessorContext context_;
  MockEventTracker* event_;
  MockSchedEventTracker* sched_;
  MockProcessTracker* process_;
  MockSliceTracker* slice_;
  ClockTracker* clock_;
  TraceStorage* storage_;
};

// TODO(eseckler): Refactor these into a new file for ftrace tests.

TEST_F(ProtoTraceParserTest, LoadSingleEvent) {
  auto* bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);

  auto* event = bundle->add_event();
  event->set_timestamp(1000);
  event->set_pid(12);

  static const char kProc1Name[] = "proc1";
  static const char kProc2Name[] = "proc2";
  auto* sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(10);
  sched_switch->set_prev_comm(kProc2Name);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProc1Name);
  sched_switch->set_next_pid(100);
  sched_switch->set_next_prio(1024);

  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1000, 10, base::StringView(kProc2Name), 256,
                              32, 100, base::StringView(kProc1Name), 1024));
  Tokenize();
}

TEST_F(ProtoTraceParserTest, LoadEventsIntoRaw) {
  auto* bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);

  // This event is unknown and will only appear in
  // raw events table.
  auto* event = bundle->add_event();
  event->set_timestamp(1000);
  event->set_pid(12);
  auto* task = event->set_task_newtask();
  task->set_pid(123);
  static const char task_newtask[] = "task_newtask";
  task->set_comm(task_newtask);
  task->set_clone_flags(12);
  task->set_oom_score_adj(15);

  // This event has specific parsing logic, but will
  // also appear in raw events table.
  event = bundle->add_event();
  event->set_timestamp(1001);
  event->set_pid(12);
  auto* print = event->set_print();
  print->set_ip(20);
  static const char buf_value[] = "This is a print event";
  print->set_buf(buf_value);

  EXPECT_CALL(*process_, GetOrCreateProcess(123));

  Tokenize();

  const auto& raw = context_.storage->raw_table();
  ASSERT_EQ(raw.row_count(), 2u);
  const auto& args = context_.storage->arg_table();
  ASSERT_EQ(args.row_count(), 6u);
  // Order is by row and then by StringIds.
  ASSERT_EQ(args.key()[0], context_.storage->InternString("comm"));
  ASSERT_EQ(args.key()[1], context_.storage->InternString("pid"));
  ASSERT_EQ(args.key()[2], context_.storage->InternString("oom_score_adj"));
  ASSERT_EQ(args.key()[3], context_.storage->InternString("clone_flags"));
  ASSERT_EQ(args.key()[4], context_.storage->InternString("ip"));
  ASSERT_EQ(args.key()[5], context_.storage->InternString("buf"));
  ASSERT_STREQ(args.string_value().GetString(0).c_str(), task_newtask);
  ASSERT_EQ(args.int_value()[1], 123);
  ASSERT_EQ(args.int_value()[2], 15);
  ASSERT_EQ(args.int_value()[3], 12);
  ASSERT_EQ(args.int_value()[4], 20);
  ASSERT_STREQ(args.string_value().GetString(5).c_str(), buf_value);

  // TODO(taylori): Add test ftrace event with all field types
  // and test here.
}

TEST_F(ProtoTraceParserTest, LoadGenericFtrace) {
  auto* packet = trace_.add_packet();
  packet->set_timestamp(100);

  auto* bundle = packet->set_ftrace_events();
  bundle->set_cpu(4);

  auto* ftrace = bundle->add_event();
  ftrace->set_timestamp(100);
  ftrace->set_pid(10);

  auto* generic = ftrace->set_generic();
  generic->set_event_name("Test");

  auto* field = generic->add_field();
  field->set_name("meta1");
  field->set_str_value("value1");

  field = generic->add_field();
  field->set_name("meta2");
  field->set_int_value(-2);

  field = generic->add_field();
  field->set_name("meta3");
  field->set_uint_value(3);

  Tokenize();

  const auto& raw = storage_->raw_table();

  ASSERT_EQ(raw.row_count(), 1u);
  ASSERT_EQ(raw.ts()[raw.row_count() - 1], 100);
  ASSERT_EQ(storage_->thread_table().tid()[raw.utid()[raw.row_count() - 1]],
            10u);
  ASSERT_EQ(raw.name().GetString(raw.row_count() - 1), "Test");

  auto set_id = raw.arg_set_id()[raw.row_count() - 1];

  const auto& args = storage_->arg_table();
  RowMap rm = args.FilterToRowMap({args.arg_set_id().eq(set_id)});

  auto row = rm.Get(0);

  ASSERT_EQ(args.key().GetString(row), "meta1");
  ASSERT_EQ(args.string_value().GetString(row++), "value1");

  ASSERT_EQ(args.key().GetString(row), "meta2");
  ASSERT_EQ(args.int_value()[row++], -2);

  ASSERT_EQ(args.key().GetString(row), "meta3");
  ASSERT_EQ(args.int_value()[row++], 3);
}

TEST_F(ProtoTraceParserTest, LoadMultipleEvents) {
  auto* bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);

  auto* event = bundle->add_event();
  event->set_timestamp(1000);
  event->set_pid(12);

  static const char kProcName1[] = "proc1";
  static const char kProcName2[] = "proc2";
  auto* sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(10);
  sched_switch->set_prev_comm(kProcName2);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProcName1);
  sched_switch->set_next_pid(100);
  sched_switch->set_next_prio(1024);

  event = bundle->add_event();
  event->set_timestamp(1001);
  event->set_pid(12);

  sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(100);
  sched_switch->set_prev_comm(kProcName1);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProcName2);
  sched_switch->set_next_pid(10);
  sched_switch->set_next_prio(512);

  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1000, 10, base::StringView(kProcName2), 256,
                              32, 100, base::StringView(kProcName1), 1024));

  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1001, 100, base::StringView(kProcName1), 256,
                              32, 10, base::StringView(kProcName2), 512));

  Tokenize();
}

TEST_F(ProtoTraceParserTest, LoadMultiplePackets) {
  auto* bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);

  auto* event = bundle->add_event();
  event->set_timestamp(1000);
  event->set_pid(12);

  static const char kProcName1[] = "proc1";
  static const char kProcName2[] = "proc2";
  auto* sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(10);
  sched_switch->set_prev_comm(kProcName2);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProcName1);
  sched_switch->set_next_pid(100);
  sched_switch->set_next_prio(1024);

  bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);

  event = bundle->add_event();
  event->set_timestamp(1001);
  event->set_pid(12);

  sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(100);
  sched_switch->set_prev_comm(kProcName1);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProcName2);
  sched_switch->set_next_pid(10);
  sched_switch->set_next_prio(512);

  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1000, 10, base::StringView(kProcName2), 256,
                              32, 100, base::StringView(kProcName1), 1024));

  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1001, 100, base::StringView(kProcName1), 256,
                              32, 10, base::StringView(kProcName2), 512));
  Tokenize();
}

TEST_F(ProtoTraceParserTest, RepeatedLoadSinglePacket) {
  auto* bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);
  auto* event = bundle->add_event();
  event->set_timestamp(1000);
  event->set_pid(12);
  static const char kProcName1[] = "proc1";
  static const char kProcName2[] = "proc2";
  auto* sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(10);
  sched_switch->set_prev_comm(kProcName2);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProcName1);
  sched_switch->set_next_pid(100);
  sched_switch->set_next_prio(1024);
  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1000, 10, base::StringView(kProcName2), 256,
                              32, 100, base::StringView(kProcName1), 1024));
  Tokenize();

  bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(10);
  event = bundle->add_event();
  event->set_timestamp(1001);
  event->set_pid(12);
  sched_switch = event->set_sched_switch();
  sched_switch->set_prev_pid(100);
  sched_switch->set_prev_comm(kProcName1);
  sched_switch->set_prev_prio(256);
  sched_switch->set_prev_state(32);
  sched_switch->set_next_comm(kProcName2);
  sched_switch->set_next_pid(10);
  sched_switch->set_next_prio(512);

  EXPECT_CALL(*sched_,
              PushSchedSwitch(10, 1001, 100, base::StringView(kProcName1), 256,
                              32, 10, base::StringView(kProcName2), 512));
  Tokenize();
}

TEST_F(ProtoTraceParserTest, LoadCpuFreq) {
  auto* bundle = trace_.add_packet()->set_ftrace_events();
  bundle->set_cpu(12);
  auto* event = bundle->add_event();
  event->set_timestamp(1000);
  event->set_pid(12);
  auto* cpu_freq = event->set_cpu_frequency();
  cpu_freq->set_cpu_id(10);
  cpu_freq->set_state(2000);

  EXPECT_CALL(*event_, PushCounter(1000, DoubleEq(2000), TrackId{0}));
  Tokenize();

  EXPECT_EQ(context_.storage->cpu_counter_track_table().cpu()[0], 10u);
}

TEST_F(ProtoTraceParserTest, LoadMemInfo) {
  auto* packet = trace_.add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  auto* meminfo = bundle->add_meminfo();
  meminfo->set_key(protos::pbzero::MEMINFO_MEM_TOTAL);
  uint32_t value = 10;
  meminfo->set_value(value);

  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts),
                                   DoubleEq(value * 1024.0), TrackId{0u}));
  Tokenize();

  EXPECT_EQ(context_.storage->track_table().row_count(), 1u);
}

TEST_F(ProtoTraceParserTest, LoadVmStats) {
  auto* packet = trace_.add_packet();
  uint64_t ts = 1000;
  packet->set_timestamp(ts);
  auto* bundle = packet->set_sys_stats();
  auto* meminfo = bundle->add_vmstat();
  meminfo->set_key(protos::pbzero::VMSTAT_COMPACT_SUCCESS);
  uint32_t value = 10;
  meminfo->set_value(value);

  EXPECT_CALL(*event_, PushCounter(static_cast<int64_t>(ts), DoubleEq(value),
                                   TrackId{0u}));
  Tokenize();

  EXPECT_EQ(context_.storage->track_table().row_count(), 1u);
}

TEST_F(ProtoTraceParserTest, LoadProcessPacket) {
  auto* tree = trace_.add_packet()->set_process_tree();
  auto* process = tree->add_processes();
  static const char kProcName1[] = "proc1";

  process->add_cmdline(kProcName1);
  process->set_pid(1);
  process->set_ppid(3);

  EXPECT_CALL(*process_,
              SetProcessMetadata(1, Eq(3u), base::StringView(kProcName1)));
  Tokenize();
}

TEST_F(ProtoTraceParserTest, LoadProcessPacket_FirstCmdline) {
  auto* tree = trace_.add_packet()->set_process_tree();
  auto* process = tree->add_processes();
  static const char kProcName1[] = "proc1";
  static const char kProcName2[] = "proc2";

  process->add_cmdline(kProcName1);
  process->add_cmdline(kProcName2);
  process->set_pid(1);
  process->set_ppid(3);

  EXPECT_CALL(*process_,
              SetProcessMetadata(1, Eq(3u), base::StringView(kProcName1)));
  Tokenize();
}

TEST_F(ProtoTraceParserTest, LoadThreadPacket) {
  auto* tree = trace_.add_packet()->set_process_tree();
  auto* thread = tree->add_threads();
  thread->set_tid(1);
  thread->set_tgid(2);

  EXPECT_CALL(*process_, UpdateThread(1, 2));
  Tokenize();
}

TEST_F(ProtoTraceParserTest, ProcessNameFromProcessDescriptor) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* process_desc = packet->set_process_descriptor();
    process_desc->set_pid(15);
    process_desc->set_process_name("OldProcessName");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* process_desc = packet->set_process_descriptor();
    process_desc->set_pid(15);
    process_desc->set_process_name("NewProcessName");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    auto* process_desc = packet->set_process_descriptor();
    process_desc->set_pid(16);
    process_desc->set_process_name("DifferentProcessName");
  }

  EXPECT_CALL(*process_, GetOrCreateProcess(15))
      .WillRepeatedly(testing::Return(1u));
  EXPECT_CALL(*process_, GetOrCreateProcess(16)).WillOnce(testing::Return(2u));

  EXPECT_CALL(*process_, SetProcessNameIfUnset(
                             1u, storage_->InternString("OldProcessName")));
  // Packet with same thread, but different name should update the name.
  EXPECT_CALL(*process_, SetProcessNameIfUnset(
                             1u, storage_->InternString("NewProcessName")));
  EXPECT_CALL(*process_,
              SetProcessNameIfUnset(
                  2u, storage_->InternString("DifferentProcessName")));

  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, ThreadNameFromThreadDescriptor) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_thread_name("OldThreadName");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_thread_name("NewThreadName");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(11);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_thread_name("DifferentThreadName");
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(testing::Return(1u));
  EXPECT_CALL(*process_, UpdateThread(11, 15)).WillOnce(testing::Return(2u));

  EXPECT_CALL(*process_, SetThreadNameIfUnset(
                             1u, storage_->InternString("OldThreadName")));
  // Packet with same thread, but different name should update the name.
  EXPECT_CALL(*process_, SetThreadNameIfUnset(
                             1u, storage_->InternString("NewThreadName")));
  EXPECT_CALL(
      *process_,
      SetThreadNameIfUnset(2u, storage_->InternString("DifferentThreadName")));

  Tokenize();
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutInternedData) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1020.
    event->set_thread_time_delta_us(5);  // absolute: 2010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1005);
    event->set_thread_time_absolute_us(2003);
    event->add_category_iids(2);
    event->add_category_iids(3);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('X');
    legacy_event->set_duration_us(23);         // absolute end: 1028.
    legacy_event->set_thread_duration_us(12);  // absolute end: 2015.
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  MockBoundInserter inserter;

  StringId unknown_cat = storage_->InternString("unknown(1)");

  constexpr TrackId track{0u};
  constexpr TrackId thread_time_track{1u};

  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  // Only the begin thread time can be imported into the counter table.
  EXPECT_CALL(*event_, PushCounter(1005000, testing::DoubleEq(2003000),
                                   thread_time_track));
  EXPECT_CALL(*slice_,
              Scoped(1005000, track, kNullStringId, kNullStringId, 23000, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(0u)));
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, Begin(1010000, track, unknown_cat, kNullStringId, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, End(1020000, track, unknown_cat, kNullStringId, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));

  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(storage_->thread_slices().slice_count(), 2u);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[0], 0u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[0], 2003000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[0], 12000);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[1], 1u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[1], 2005000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[1], 5000);
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutInternedDataWithTypes) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1020.
    event->set_thread_time_delta_us(5);  // absolute: 2010.
    event->add_category_iids(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_END);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1015);
    event->set_thread_time_absolute_us(2007);
    event->add_category_iids(2);
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  MockBoundInserter inserter;

  StringId unknown_cat1 = storage_->InternString("unknown(1)");
  StringId unknown_cat2 = storage_->InternString("unknown(2)");

  constexpr TrackId track{0u};
  constexpr TrackId thread_time_track{1u};

  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, Begin(1010000, track, unknown_cat1, kNullStringId, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(0u)));
  EXPECT_CALL(*event_, PushCounter(1015000, testing::DoubleEq(2007000),
                                   thread_time_track));
  EXPECT_CALL(*slice_,
              Scoped(1015000, track, unknown_cat2, kNullStringId, 0, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(1u)));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*slice_, End(1020000, track, unknown_cat1, kNullStringId, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(0u)));

  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(storage_->thread_slices().slice_count(), 2u);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[0], 0u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[0], 2005000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[0], 5000);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[1], 1u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[1], 2007000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[1], 0);
}

TEST_F(ProtoTraceParserTest, TrackEventWithInternedData) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_reference_thread_instruction_count(3000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1010.
    event->set_thread_time_delta_us(5);             // absolute: 2005.
    event->set_thread_instruction_count_delta(20);  // absolute: 3020.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1040);
    event->set_thread_time_absolute_us(2030);
    event->set_thread_instruction_count_absolute(3100);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('I');
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1050);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('i');
    legacy_event->set_instant_event_scope(
        protos::pbzero::TrackEvent::LegacyEvent::SCOPE_PROCESS);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1020.
    event->set_thread_time_delta_us(5);             // absolute: 2010.
    event->set_thread_instruction_count_delta(20);  // absolute: 3040.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1005);
    event->set_thread_time_absolute_us(2003);
    event->set_thread_instruction_count_absolute(3010);
    event->add_category_iids(2);
    event->add_category_iids(3);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(4);
    legacy_event->set_phase('X');
    legacy_event->set_duration_us(23);               // absolute end: 1028.
    legacy_event->set_thread_duration_us(12);        // absolute end: 2015.
    legacy_event->set_thread_instruction_delta(50);  // absolute end: 3060.
    legacy_event->set_bind_id(9999);
    legacy_event->set_bind_to_enclosing(true);
    legacy_event->set_flow_direction(
        protos::pbzero::TrackEvent::LegacyEvent::FLOW_INOUT);

    auto* interned_data = packet->set_interned_data();
    auto cat2 = interned_data->add_event_categories();
    cat2->set_iid(2);
    cat2->set_name("cat2");
    auto cat3 = interned_data->add_event_categories();
    cat3->set_iid(3);
    cat3->set_name("cat3");
    auto ev2 = interned_data->add_event_names();
    ev2->set_iid(4);
    ev2->set_name("ev2");
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 2u;
  storage_->mutable_thread_table()->Insert(row);

  constexpr TrackId thread_1_track{0u};
  constexpr TrackId thread_time_track{1u};
  constexpr TrackId thread_instruction_count_track{2u};
  constexpr TrackId process_2_track{3u};

  StringId cat_2_3 = storage_->InternString("cat2,cat3");
  StringId ev_2 = storage_->InternString("ev2");
  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  MockBoundInserter inserter;
  // Only the begin timestamp counters can be imported into the counter table.
  EXPECT_CALL(*event_, PushCounter(1005000, testing::DoubleEq(2003000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1005000, testing::DoubleEq(3010),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, Scoped(1005000, thread_1_track, cat_2_3, ev_2, 23000, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(0u)));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::UnsignedInteger(9999u), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::Boolean(true), _));
  EXPECT_CALL(inserter, AddArg(_, _, _, _));

  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(3020),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, Begin(1010000, thread_1_track, cat_1, ev_1, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));

  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(3040),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, End(1020000, thread_1_track, cat_1, ev_1, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));

  EXPECT_CALL(*event_, PushCounter(1040000, testing::DoubleEq(2030000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1040000, testing::DoubleEq(3100),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, Scoped(1040000, thread_1_track, cat_1, ev_1, 0, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(2u)));

  EXPECT_CALL(*slice_, Scoped(1050000, process_2_track, cat_1, ev_1, 0, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(3u)));
  // Second slice should have a legacy_event.passthrough_utid arg.
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::UnsignedInteger(1u), _));

  context_.sorter->ExtractEventsForced();

  EXPECT_EQ(storage_->thread_slices().slice_count(), 3u);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[0], 0u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[0], 2003000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[0], 12000);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_counts()[0], 3010);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_deltas()[0], 50);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[1], 1u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[1], 2005000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[1], 5000);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_counts()[1], 3020);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_deltas()[1], 20);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[2], 2u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[2], 2030000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[2], 0);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_counts()[2], 3100);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_deltas()[2], 0);
}

TEST_F(ProtoTraceParserTest, TrackEventAsyncEvents) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
    thread_desc->set_reference_thread_instruction_count(3000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1010.
    event->set_thread_time_delta_us(5);             // absolute: 2005.
    event->set_thread_instruction_count_delta(20);  // absolute: 3020.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('b');
    legacy_event->set_global_id(10);
    legacy_event->set_use_async_tts(true);

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);              // absolute: 1020.
    event->set_thread_time_delta_us(5);             // absolute: 2010.
    event->set_thread_instruction_count_delta(20);  // absolute: 3040.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('e');
    legacy_event->set_global_id(10);
    legacy_event->set_use_async_tts(true);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1015);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('n');
    legacy_event->set_global_id(10);

    auto* interned_data = packet->set_interned_data();
    auto ev2 = interned_data->add_event_names();
    ev2->set_iid(2);
    ev2->set_name("ev2");
  }
  {
    // Different category but same global_id -> separate track.
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1018);
    event->add_category_iids(2);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('n');
    legacy_event->set_global_id(15);

    auto* interned_data = packet->set_interned_data();
    auto cat2 = interned_data->add_event_categories();
    cat2->set_iid(2);
    cat2->set_name("cat2");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_absolute_us(1030);
    event->add_category_iids(2);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(2);
    legacy_event->set_phase('n');
    legacy_event->set_local_id(15);
    legacy_event->set_id_scope("scope1");
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId cat_2 = storage_->InternString("cat2");
  StringId ev_2 = storage_->InternString("ev2");

  TrackId thread_time_track{2u};
  TrackId thread_instruction_count_track{3u};

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(2005000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1010000, testing::DoubleEq(3020),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, Begin(1010000, TrackId{1}, cat_1, ev_1, _))
      .WillOnce(Return(0u));
  EXPECT_CALL(*slice_, Scoped(1015000, TrackId{1}, cat_1, ev_2, 0, _));
  EXPECT_CALL(*slice_, Scoped(1018000, TrackId{4}, cat_2, ev_2, 0, _));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(2010000),
                                   thread_time_track));
  EXPECT_CALL(*event_, PushCounter(1020000, testing::DoubleEq(3040),
                                   thread_instruction_count_track));
  EXPECT_CALL(*slice_, End(1020000, TrackId{1}, cat_1, ev_1, _))
      .WillOnce(Return(0u));
  EXPECT_CALL(*slice_, Scoped(1030000, TrackId{5}, cat_2, ev_2, 0, _));

  context_.sorter->ExtractEventsForced();

  // First track is for the thread; second first async, third and fourth for
  // thread time and instruction count, others are the async event tracks.
  EXPECT_EQ(storage_->track_table().row_count(), 6u);
  EXPECT_EQ(storage_->track_table().name()[1], ev_1);
  EXPECT_EQ(storage_->track_table().name()[4], ev_2);
  EXPECT_EQ(storage_->track_table().name()[5], ev_2);

  EXPECT_EQ(storage_->process_track_table().row_count(), 3u);
  EXPECT_EQ(storage_->process_track_table().upid()[0], 1u);
  EXPECT_EQ(storage_->process_track_table().upid()[1], 1u);
  EXPECT_EQ(storage_->process_track_table().upid()[2], 1u);

  EXPECT_EQ(storage_->virtual_track_slices().slice_count(), 1u);
  EXPECT_EQ(storage_->virtual_track_slices().slice_ids()[0], 0u);
  EXPECT_EQ(storage_->virtual_track_slices().thread_timestamp_ns()[0], 2005000);
  EXPECT_EQ(storage_->virtual_track_slices().thread_duration_ns()[0], 5000);
  EXPECT_EQ(storage_->virtual_track_slices().thread_instruction_counts()[0],
            3020);
  EXPECT_EQ(storage_->virtual_track_slices().thread_instruction_deltas()[0],
            20);
}

// TODO(eseckler): Also test instant events on separate tracks.
TEST_F(ProtoTraceParserTest, TrackEventWithTrackDescriptors) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  // Sequence 1.
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    packet->set_timestamp(1000000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(1234);
    track_desc->set_name("Thread track 1");
    auto* thread_desc = track_desc->set_thread();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(1000000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(5678);
    track_desc->set_name("Async track 1");
  }
  {
    // Async event started on "Async track 1".
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(1010000);
    auto* event = packet->set_track_event();
    event->set_track_uuid(5678);
    event->set_thread_time_absolute_us(2005);
    event->set_thread_instruction_count_absolute(3020);
    event->add_category_iids(1);
    event->set_name_iid(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_use_async_tts(true);

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    // Instant event on "Thread track 1".
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(1015000);
    auto* event = packet->set_track_event();
    event->set_track_uuid(1234);
    event->set_thread_time_absolute_us(2007);
    event->add_category_iids(2);
    event->set_name_iid(2);
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(2);
    cat1->set_name("cat2");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(2);
    ev1->set_name("ev2");
  }

  // Sequence 2.
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    packet->set_timestamp(1000000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(4321);
    track_desc->set_name("Thread track 2");
    auto* thread_desc = track_desc->set_thread();
    thread_desc->set_pid(15);
    thread_desc->set_tid(17);
  }
  {
    // Async event completed on "Async track 1".
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_timestamp(1020000);
    auto* event = packet->set_track_event();
    event->set_track_uuid(5678);
    event->set_thread_time_absolute_us(2010);
    event->set_thread_instruction_count_absolute(3040);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_END);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_use_async_tts(true);
  }
  {
    // Instant event on "Thread track 2".
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_timestamp(1016000);
    auto* event = packet->set_track_event();
    event->set_track_uuid(4321);
    event->set_thread_time_absolute_us(2008);
    event->add_category_iids(1);
    event->set_name_iid(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat3");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev3");
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));
  EXPECT_CALL(*process_, UpdateThread(17, 15)).WillRepeatedly(Return(2));

  tables::ThreadTable::Row t1(16);
  t1.upid = 1u;
  storage_->mutable_thread_table()->Insert(t1);

  tables::ThreadTable::Row t2(16);
  t2.upid = 2u;
  storage_->mutable_thread_table()->Insert(t2);

  Tokenize();

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId cat_2 = storage_->InternString("cat2");
  StringId ev_2 = storage_->InternString("ev2");
  StringId cat_3 = storage_->InternString("cat3");
  StringId ev_3 = storage_->InternString("ev3");

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*slice_, Begin(1010000, TrackId{1}, cat_1, ev_1, _))
      .WillOnce(Return(0u));

  EXPECT_CALL(*event_,
              PushCounter(1015000, testing::DoubleEq(2007000), TrackId{4}));
  EXPECT_CALL(*slice_, Scoped(1015000, TrackId{0}, cat_2, ev_2, 0, _))
      .WillOnce(Return(1u));

  EXPECT_CALL(*event_,
              PushCounter(1016000, testing::DoubleEq(2008000), TrackId{5}));
  EXPECT_CALL(*slice_, Scoped(1016000, TrackId{3}, cat_3, ev_3, 0, _))
      .WillOnce(Return(2u));

  EXPECT_CALL(*slice_,
              End(1020000, TrackId{1}, kNullStringId, kNullStringId, _))
      .WillOnce(Return(0u));

  context_.sorter->ExtractEventsForced();

  // First track is "Thread track 1"; second is "Async track 1", third is global
  // default track (parent of async track), fourth is "Thread track 2", fifth &
  // sixth are thread time tracks for thread 1 and 2.
  EXPECT_EQ(storage_->track_table().row_count(), 6u);
  EXPECT_EQ(storage_->track_table().name().GetString(0), "Thread track 1");
  EXPECT_EQ(storage_->track_table().name().GetString(1), "Async track 1");
  EXPECT_EQ(storage_->track_table().name().GetString(2), "Default Track");
  EXPECT_EQ(storage_->track_table().name().GetString(3), "Thread track 2");
  EXPECT_EQ(storage_->thread_track_table().row_count(), 2u);
  EXPECT_EQ(storage_->thread_track_table().utid()[0], 1u);
  EXPECT_EQ(storage_->thread_track_table().utid()[1], 2u);

  EXPECT_EQ(storage_->virtual_track_slices().slice_count(), 1u);
  EXPECT_EQ(storage_->virtual_track_slices().slice_ids()[0], 0u);
  EXPECT_EQ(storage_->virtual_track_slices().thread_timestamp_ns()[0], 2005000);
  EXPECT_EQ(storage_->virtual_track_slices().thread_duration_ns()[0], 5000);
  EXPECT_EQ(storage_->virtual_track_slices().thread_instruction_counts()[0],
            3020);
  EXPECT_EQ(storage_->virtual_track_slices().thread_instruction_deltas()[0],
            20);

  EXPECT_EQ(storage_->thread_slices().slice_count(), 2u);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[0], 1u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[0], 2007000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[0], 0);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_counts()[0], 0);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_deltas()[0], 0);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[1], 2u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[1], 2008000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[1], 0);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_counts()[1], 0);
  EXPECT_EQ(storage_->thread_slices().thread_instruction_deltas()[1], 0);
}

TEST_F(ProtoTraceParserTest, TrackEventWithResortedCounterDescriptor) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  // Descriptors with timestamps after the event below. They will be tokenized
  // in the order they appear here, but then resorted before parsing to appear
  // after the events below.
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    packet->set_timestamp(3000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(1);
    auto* thread_desc = track_desc->set_thread();
    thread_desc->set_pid(5);
    thread_desc->set_tid(1);
    thread_desc->set_thread_name("t1");
    // Default to track for "t1" and an extra counter for thread time.
    auto* track_event_defaults =
        packet->set_trace_packet_defaults()->set_track_event_defaults();
    track_event_defaults->set_track_uuid(1);
    // Thread-time counter track defined below.
    track_event_defaults->add_extra_counter_track_uuids(10);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(3000);
    auto* track_desc = packet->set_track_descriptor();
    track_desc->set_uuid(10);
    track_desc->set_parent_uuid(1);
    auto* counter = track_desc->set_counter();
    counter->set_type(
        protos::pbzero::CounterDescriptor::COUNTER_THREAD_TIME_NS);
    counter->set_unit_multiplier(1000);  // provided in us.
    counter->set_is_incremental(true);
  }
  {
    // Event with timestamps before the descriptors above. The thread time
    // counter values should still be imported as counter values and as args for
    // JSON export. Should appear on default track "t1" with
    // extra_counter_values for "c1".
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_sequence_flags(
        protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    packet->set_timestamp(1000);
    auto* event = packet->set_track_event();
    event->add_categories("cat1");
    event->set_name("ev1");
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    event->add_extra_counter_values(1000);  // absolute: 1000000.
  }
  {
    // End for "ev1".
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_timestamp(1100);
    auto* event = packet->set_track_event();
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_END);
    event->add_extra_counter_values(10);  // absolute: 1010000.
  }

  EXPECT_CALL(*process_, UpdateThread(1, 5)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row t1(16);
  t1.upid = 1u;
  storage_->mutable_thread_table()->Insert(t1);

  Tokenize();

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*event_,
              PushCounter(1000, testing::DoubleEq(1000000), TrackId{1}));
  EXPECT_CALL(*slice_, Begin(1000, TrackId{0}, cat_1, ev_1, _))
      .WillOnce(Return(0u));

  EXPECT_CALL(*event_,
              PushCounter(1100, testing::DoubleEq(1010000), TrackId{1}));
  EXPECT_CALL(*slice_, End(1100, TrackId{0}, kNullStringId, kNullStringId, _))
      .WillOnce(Return(0u));

  EXPECT_CALL(*process_,
              SetThreadNameIfUnset(1u, storage_->InternString("t1")));

  context_.sorter->ExtractEventsForced();

  // First track is thread time track, second is "t1".
  EXPECT_EQ(storage_->track_table().row_count(), 2u);
  EXPECT_EQ(storage_->thread_track_table().row_count(), 1u);
  EXPECT_EQ(storage_->thread_track_table().utid()[0], 1u);

  // Counter values should also be imported into thread slices.
  EXPECT_EQ(storage_->thread_slices().slice_count(), 1u);
  EXPECT_EQ(storage_->thread_slices().slice_ids()[0], 0u);
  EXPECT_EQ(storage_->thread_slices().thread_timestamp_ns()[0], 1000000);
  EXPECT_EQ(storage_->thread_slices().thread_duration_ns()[0], 10000);
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutIncrementalStateReset) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    // Event should be discarded because delta timestamps require valid
    // incremental state + thread descriptor.
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    // Event should be discarded because it specifies
    // SEQ_NEEDS_INCREMENTAL_STATE.
    auto* packet = trace_.add_packet();
    packet->set_timestamp(2000000);
    packet->set_trusted_packet_sequence_id(1);
    packet->set_sequence_flags(
        protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    auto* event = packet->set_track_event();
    event->add_categories("cat");
    event->set_name("ev1");
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
  }
  {
    // Event should be accepted because it does not specify
    // SEQ_NEEDS_INCREMENTAL_STATE and uses absolute timestamps.
    auto* packet = trace_.add_packet();
    packet->set_timestamp(2100000);
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->add_categories("cat1");
    event->set_name("ev2");
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
  }

  Tokenize();

  StringId cat1 = storage_->InternString("cat1");
  StringId ev2 = storage_->InternString("ev2");

  EXPECT_CALL(*slice_, Scoped(2100000, TrackId{0}, cat1, ev2, 0, _))
      .WillOnce(Return(0u));
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithoutThreadDescriptor) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    // Event should be discarded because it specifies delta timestamps and no
    // thread descriptor was seen yet.
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->set_thread_time_delta_us(5);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    // Events that specify SEQ_NEEDS_INCREMENTAL_STATE should be accepted even
    // if there's no valid thread descriptor.
    auto* packet = trace_.add_packet();
    packet->set_timestamp(2000000);
    packet->set_trusted_packet_sequence_id(1);
    packet->set_sequence_flags(
        protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE);
    auto* event = packet->set_track_event();
    event->add_categories("cat1");
    event->set_name("ev1");
    event->set_type(protos::pbzero::TrackEvent::TYPE_INSTANT);
  }

  Tokenize();

  StringId cat1 = storage_->InternString("cat1");
  StringId ev1 = storage_->InternString("ev1");

  EXPECT_CALL(*slice_, Scoped(2000000, TrackId{0}, cat1, ev1, 0, _))
      .WillOnce(Return(0u));
  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithDataLoss) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');
  }
  {
    // Event should be dropped because data loss occurred before.
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_previous_packet_dropped(true);  // Data loss occurred.
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    // Event should be dropped because incremental state is invalid.
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    // Event should be dropped because no new thread descriptor was seen yet.
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(2000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 2010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId unknown_cat = storage_->InternString("unknown(1)");
  constexpr TrackId track{0u};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  EXPECT_CALL(*slice_, Begin(1010000, track, unknown_cat, kNullStringId, _));
  EXPECT_CALL(*slice_, End(2010000, track, unknown_cat, kNullStringId, _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventMultipleSequences) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(17);
    thread_desc->set_reference_timestamp_us(995);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1005.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev2 = interned_data->add_event_names();
    ev2->set_iid(1);
    ev2->set_name("ev2");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1020.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(2);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1015.
    event->add_category_iids(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(*process_, UpdateThread(17, 15))
      .WillRepeatedly(Return(2));

  tables::ThreadTable::Row t1(16);
  t1.upid = 1u;
  storage_->mutable_thread_table()->Insert(t1);

  tables::ThreadTable::Row t2(17);
  t2.upid = 1u;
  storage_->mutable_thread_table()->Insert(t2);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_2 = storage_->InternString("ev2");
  StringId ev_1 = storage_->InternString("ev1");

  constexpr TrackId thread_2_track{0u};
  constexpr TrackId thread_1_track{1u};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  EXPECT_CALL(*slice_, Begin(1005000, thread_2_track, cat_1, ev_2, _));
  EXPECT_CALL(*slice_, Begin(1010000, thread_1_track, cat_1, ev_1, _));
  EXPECT_CALL(*slice_, End(1015000, thread_2_track, cat_1, ev_2, _));
  EXPECT_CALL(*slice_, End(1020000, thread_1_track, cat_1, ev_1, _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithDebugAnnotations) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));
  MockBoundInserter inserter;

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->add_category_iids(1);
    auto* annotation1 = event->add_debug_annotations();
    annotation1->set_name_iid(1);
    annotation1->set_uint_value(10u);
    auto* annotation2 = event->add_debug_annotations();
    annotation2->set_name_iid(2);
    auto* nested = annotation2->set_nested_value();
    nested->set_nested_type(protos::pbzero::DebugAnnotation::NestedValue::DICT);
    nested->add_dict_keys("child1");
    nested->add_dict_keys("child2");
    auto* child1 = nested->add_dict_values();
    child1->set_nested_type(
        protos::pbzero::DebugAnnotation::NestedValue::UNSPECIFIED);
    child1->set_bool_value(true);
    auto* child2 = nested->add_dict_values();
    child2->set_nested_type(
        protos::pbzero::DebugAnnotation::NestedValue::ARRAY);
    auto* child21 = child2->add_array_values();
    child21->set_nested_type(
        protos::pbzero::DebugAnnotation::NestedValue::UNSPECIFIED);
    child21->set_string_value("child21");
    auto* child22 = child2->add_array_values();
    child22->set_nested_type(
        protos::pbzero::DebugAnnotation::NestedValue::UNSPECIFIED);
    child22->set_double_value(2.2);
    auto* child23 = child2->add_array_values();
    child23->set_nested_type(
        protos::pbzero::DebugAnnotation::NestedValue::UNSPECIFIED);
    child23->set_int_value(23);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
    auto an1 = interned_data->add_debug_annotation_names();
    an1->set_iid(1);
    an1->set_name("an1");
    auto an2 = interned_data->add_debug_annotation_names();
    an2->set_iid(2);
    an2->set_name("an2");
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1020.
    event->add_category_iids(1);
    auto* annotation3 = event->add_debug_annotations();
    annotation3->set_name_iid(3);
    annotation3->set_int_value(-3);
    auto* annotation4 = event->add_debug_annotations();
    annotation4->set_name_iid(4);
    annotation4->set_bool_value(true);
    auto* annotation5 = event->add_debug_annotations();
    annotation5->set_name_iid(5);
    annotation5->set_double_value(-5.5);
    auto* annotation6 = event->add_debug_annotations();
    annotation6->set_name_iid(6);
    annotation6->set_pointer_value(20u);
    auto* annotation7 = event->add_debug_annotations();
    annotation7->set_name_iid(7);
    annotation7->set_string_value("val7");
    auto* annotation8 = event->add_debug_annotations();
    annotation8->set_name_iid(8);
    annotation8->set_legacy_json_value(
        "{\"val8\": {\"a\": 42, \"b\": \"val8b\"}, \"arr8\": [1, 2, 3]}");
    auto* annotation9 = event->add_debug_annotations();
    annotation9->set_name_iid(9);
    annotation9->set_int_value(15);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('E');

    auto* interned_data = packet->set_interned_data();
    auto an3 = interned_data->add_debug_annotation_names();
    an3->set_iid(3);
    an3->set_name("an3");
    auto an4 = interned_data->add_debug_annotation_names();
    an4->set_iid(4);
    an4->set_name("an4");
    auto an5 = interned_data->add_debug_annotation_names();
    an5->set_iid(5);
    an5->set_name("an5");
    auto an6 = interned_data->add_debug_annotation_names();
    an6->set_iid(6);
    an6->set_name("an6");
    auto an7 = interned_data->add_debug_annotation_names();
    an7->set_iid(7);
    an7->set_name("an7");
    auto an8 = interned_data->add_debug_annotation_names();
    an8->set_iid(8);
    an8->set_name("an8");
    auto an9 = interned_data->add_debug_annotation_names();
    an9->set_iid(9);
    an9->set_name("an8.foo");
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId debug_an_1 = storage_->InternString("debug.an1");
  StringId debug_an_2_child_1 = storage_->InternString("debug.an2.child1");
  StringId debug_an_2_child_2 = storage_->InternString("debug.an2.child2");
  StringId debug_an_2_child_2_0 = storage_->InternString("debug.an2.child2[0]");
  StringId child21 = storage_->InternString("child21");
  StringId debug_an_2_child_2_1 = storage_->InternString("debug.an2.child2[1]");
  StringId debug_an_2_child_2_2 = storage_->InternString("debug.an2.child2[2]");
  StringId debug_an_3 = storage_->InternString("debug.an3");
  StringId debug_an_4 = storage_->InternString("debug.an4");
  StringId debug_an_5 = storage_->InternString("debug.an5");
  StringId debug_an_6 = storage_->InternString("debug.an6");
  StringId debug_an_7 = storage_->InternString("debug.an7");
  StringId val_7 = storage_->InternString("val7");
  StringId debug_an_8_val8_a = storage_->InternString("debug.an8.val8.a");
  StringId debug_an_8_val8_b = storage_->InternString("debug.an8.val8.b");
  StringId val_8b = storage_->InternString("val8b");
  StringId debug_an_8_arr8 = storage_->InternString("debug.an8.arr8");
  StringId debug_an_8_arr8_0 = storage_->InternString("debug.an8.arr8[0]");
  StringId debug_an_8_arr8_1 = storage_->InternString("debug.an8.arr8[1]");
  StringId debug_an_8_arr8_2 = storage_->InternString("debug.an8.arr8[2]");
  StringId debug_an_8_foo = storage_->InternString("debug.an8_foo");

  constexpr TrackId track{0u};

  EXPECT_CALL(*slice_, Begin(1010000, track, cat_1, ev_1, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));
  EXPECT_CALL(inserter, AddArg(debug_an_1, debug_an_1,
                               Variadic::UnsignedInteger(10u), _));

  EXPECT_CALL(inserter, AddArg(debug_an_2_child_1, debug_an_2_child_1,
                               Variadic::Boolean(true), _));

  EXPECT_CALL(inserter, AddArg(debug_an_2_child_2, debug_an_2_child_2_0,
                               Variadic::String(child21), _));

  EXPECT_CALL(inserter, AddArg(debug_an_2_child_2, debug_an_2_child_2_1,
                               Variadic::Real(2.2), _));

  EXPECT_CALL(inserter, AddArg(debug_an_2_child_2, debug_an_2_child_2_2,
                               Variadic::Integer(23), _));

  EXPECT_CALL(*slice_, End(1020000, track, cat_1, ev_1, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));

  EXPECT_CALL(inserter,
              AddArg(debug_an_3, debug_an_3, Variadic::Integer(-3), _));
  EXPECT_CALL(inserter,
              AddArg(debug_an_4, debug_an_4, Variadic::Boolean(true), _));
  EXPECT_CALL(inserter,
              AddArg(debug_an_5, debug_an_5, Variadic::Real(-5.5), _));
  EXPECT_CALL(inserter,
              AddArg(debug_an_6, debug_an_6, Variadic::Pointer(20u), _));
  EXPECT_CALL(inserter,
              AddArg(debug_an_7, debug_an_7, Variadic::String(val_7), _));
  EXPECT_CALL(inserter, AddArg(debug_an_8_val8_a, debug_an_8_val8_a,
                               Variadic::Integer(42), _));
  EXPECT_CALL(inserter, AddArg(debug_an_8_val8_b, debug_an_8_val8_b,
                               Variadic::String(val_8b), _));
  EXPECT_CALL(inserter, AddArg(debug_an_8_arr8, debug_an_8_arr8_0,
                               Variadic::Integer(1), _));
  EXPECT_CALL(inserter, AddArg(debug_an_8_arr8, debug_an_8_arr8_1,
                               Variadic::Integer(2), _));
  EXPECT_CALL(inserter, AddArg(debug_an_8_arr8, debug_an_8_arr8_2,
                               Variadic::Integer(3), _));
  EXPECT_CALL(inserter,
              AddArg(debug_an_8_foo, debug_an_8_foo, Variadic::Integer(15), _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithTaskExecution) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->add_category_iids(1);
    auto* task_execution = event->set_task_execution();
    task_execution->set_posted_from_iid(1);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('B');

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
    auto loc1 = interned_data->add_source_locations();
    loc1->set_iid(1);
    loc1->set_file_name("file1");
    loc1->set_function_name("func1");
    loc1->set_line_number(42);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  constexpr TrackId track{0u};

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId file_1 = storage_->InternString("file1");
  StringId func_1 = storage_->InternString("func1");

  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  MockBoundInserter inserter;
  EXPECT_CALL(*slice_, Begin(1010000, track, cat_1, ev_1, _))
      .WillOnce(DoAll(InvokeArgument<4>(&inserter), Return(1u)));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(file_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(func_1), _));
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::UnsignedInteger(42), _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, TrackEventWithLogMessage) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->add_category_iids(1);

    auto* log_message = event->set_log_message();
    log_message->set_body_iid(1);
    log_message->set_source_location_iid(1);

    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    legacy_event->set_phase('I');

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");

    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");

    auto body = interned_data->add_log_message_body();
    body->set_iid(1);
    body->set_body("body1");

    auto loc1 = interned_data->add_source_locations();
    loc1->set_iid(1);
    loc1->set_file_name("file1");
    loc1->set_function_name("func1");
    loc1->set_line_number(1);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId body_1 = storage_->InternString("body1");

  constexpr TrackId track{0};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.

  MockBoundInserter inserter;
  EXPECT_CALL(*slice_, Scoped(1010000, track, cat_1, ev_1, 0, _))
      .WillOnce(DoAll(InvokeArgument<5>(&inserter), Return(1u)));

  // Call with logMessageBody (body1 in this case).
  EXPECT_CALL(inserter, AddArg(_, _, Variadic::String(body_1), _));

  context_.sorter->ExtractEventsForced();

  EXPECT_GT(context_.storage->android_log_table().row_count(), 0u);
  EXPECT_EQ(context_.storage->android_log_table().ts()[0], 1010000);
  EXPECT_EQ(context_.storage->android_log_table().msg()[0], body_1);
}

TEST_F(ProtoTraceParserTest, TrackEventParseLegacyEventIntoRawTable) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);   // absolute: 1010.
    event->set_thread_time_delta_us(5);  // absolute: 2005.
    event->add_category_iids(1);

    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
    // Represents a phase that isn't parsed into regular trace processor tables.
    legacy_event->set_phase('?');
    legacy_event->set_duration_us(23);
    legacy_event->set_thread_duration_us(15);
    legacy_event->set_global_id(99u);
    legacy_event->set_id_scope("scope1");
    legacy_event->set_use_async_tts('?');
    legacy_event->set_bind_id(98);
    legacy_event->set_bind_to_enclosing(true);
    legacy_event->set_flow_direction(
        protos::pbzero::TrackEvent::LegacyEvent::FLOW_INOUT);

    auto* annotation1 = event->add_debug_annotations();
    annotation1->set_name_iid(1);
    annotation1->set_uint_value(10u);

    auto* interned_data = packet->set_interned_data();
    auto cat1 = interned_data->add_event_categories();
    cat1->set_iid(1);
    cat1->set_name("cat1");
    auto ev1 = interned_data->add_event_names();
    ev1->set_iid(1);
    ev1->set_name("ev1");
    auto an1 = interned_data->add_debug_annotation_names();
    an1->set_iid(1);
    an1->set_name("an1");
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));
  // Only the begin thread time can be imported into the counter table.
  EXPECT_CALL(*event_,
              PushCounter(1010000, testing::DoubleEq(2005000), TrackId{1}));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  StringId cat_1 = storage_->InternString("cat1");
  StringId ev_1 = storage_->InternString("ev1");
  StringId scope_1 = storage_->InternString("scope1");
  StringId question = storage_->InternString("?");
  StringId debug_an_1 = storage_->InternString("debug.an1");

  context_.sorter->ExtractEventsForced();

  ::testing::Mock::VerifyAndClearExpectations(storage_);

  // Verify raw_table and args contents.
  const auto& raw_table = storage_->raw_table();
  EXPECT_EQ(raw_table.row_count(), 1u);
  EXPECT_EQ(raw_table.ts()[0], 1010000);
  EXPECT_EQ(raw_table.name()[0],
            storage_->InternString("track_event.legacy_event"));
  EXPECT_EQ(raw_table.cpu()[0], 0u);
  EXPECT_EQ(raw_table.utid()[0], 1u);
  EXPECT_EQ(raw_table.arg_set_id()[0], 1u);

  EXPECT_GE(storage_->arg_table().row_count(), 13u);

  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.category"),
                     Variadic::String(cat_1)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.name"),
                     Variadic::String(ev_1)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.phase"),
                     Variadic::String(question)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.duration_ns"),
                     Variadic::Integer(23000)));
  EXPECT_TRUE(HasArg(1u,
                     storage_->InternString("legacy_event.thread_timestamp_ns"),
                     Variadic::Integer(2005000)));
  EXPECT_TRUE(HasArg(1u,
                     storage_->InternString("legacy_event.thread_duration_ns"),
                     Variadic::Integer(15000)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.use_async_tts"),
                     Variadic::Boolean(true)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.global_id"),
                     Variadic::UnsignedInteger(99u)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.id_scope"),
                     Variadic::String(scope_1)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.bind_id"),
                     Variadic::UnsignedInteger(98u)));
  EXPECT_TRUE(HasArg(1u,
                     storage_->InternString("legacy_event.bind_to_enclosing"),
                     Variadic::Boolean(true)));
  EXPECT_TRUE(HasArg(1u, storage_->InternString("legacy_event.flow_direction"),
                     Variadic::String(storage_->InternString("inout"))));
  EXPECT_TRUE(HasArg(1u, debug_an_1, Variadic::UnsignedInteger(10u)));
}

TEST_F(ProtoTraceParserTest, TrackEventLegacyTimestampsWithClockSnapshot) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  clock_->AddSnapshot(
      {{protos::pbzero::ClockSnapshot::Clock::BOOTTIME, 0},
       {protos::pbzero::ClockSnapshot::Clock::MONOTONIC, 1000000}});

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);
    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);  // MONOTONIC.
  }
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* event = packet->set_track_event();
    event->set_timestamp_delta_us(10);  // absolute: 1010 (mon), 10 (boot).
    event->add_category_iids(1);
    event->set_type(protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN);
    auto* legacy_event = event->set_legacy_event();
    legacy_event->set_name_iid(1);
  }

  Tokenize();

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  tables::ThreadTable::Row row(16);
  row.upid = 1u;
  storage_->mutable_thread_table()->Insert(row);

  constexpr TrackId track{0u};
  InSequence in_sequence;  // Below slices should be sorted by timestamp.
  StringId unknown_cat = storage_->InternString("unknown(1)");

  // Timestamp should be adjusted to trace time (BOOTTIME).
  EXPECT_CALL(*slice_, Begin(10000, track, unknown_cat, kNullStringId, _));

  context_.sorter->ExtractEventsForced();
}

TEST_F(ProtoTraceParserTest, ParseEventWithClockIdButWithoutClockSnapshot) {
  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_timestamp(1000);
    packet->set_timestamp_clock_id(3);
    packet->set_trusted_packet_sequence_id(1);
    auto* bundle = packet->set_chrome_events();
    auto* metadata = bundle->add_metadata();
    metadata->set_name("test");
    metadata->set_int_value(23);
  }

  util::Status status = Tokenize();
  EXPECT_TRUE(status.ok());
  context_.sorter->ExtractEventsForced();

  // Metadata should have created a raw event.
  const auto& raw_table = storage_->raw_table();
  EXPECT_EQ(raw_table.row_count(), 1u);
}

TEST_F(ProtoTraceParserTest, ParseChromeMetadataEventIntoRawTable) {
  static const char kStringName[] = "string_name";
  static const char kStringValue[] = "string_value";
  static const char kIntName[] = "int_name";
  static const int kIntValue = 123;

  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_timestamp(1000);
    packet->set_timestamp_clock_id(3);
    packet->set_trusted_packet_sequence_id(1);
    auto* bundle = packet->set_chrome_events();
    auto* metadata = bundle->add_metadata();
    metadata->set_name(kStringName);
    metadata->set_string_value(kStringValue);
    metadata = bundle->add_metadata();
    metadata->set_name(kIntName);
    metadata->set_int_value(kIntValue);
  }

  Tokenize();
  context_.sorter->ExtractEventsForced();

  // Verify raw_table and args contents.
  const auto& raw_table = storage_->raw_table();
  EXPECT_EQ(raw_table.row_count(), 1u);
  EXPECT_EQ(raw_table.name()[0],
            storage_->InternString("chrome_event.metadata"));
  EXPECT_EQ(raw_table.arg_set_id()[0], 1u);

  EXPECT_EQ(storage_->arg_table().row_count(), 2u);
  EXPECT_TRUE(HasArg(1u, storage_->InternString(kStringName),
                     Variadic::String(storage_->InternString(kStringValue))));
  EXPECT_TRUE(HasArg(1u, storage_->InternString(kIntName),
                     Variadic::Integer(kIntValue)));
}

TEST_F(ProtoTraceParserTest, ParseChromeLegacyFtraceIntoRawTable) {
  static const char kDataPart0[] = "aaa";
  static const char kDataPart1[] = "bbb";
  static const char kFullData[] = "aaabbb";

  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* bundle = packet->set_chrome_events();
    bundle->add_legacy_ftrace_output(kDataPart0);
    bundle->add_legacy_ftrace_output(kDataPart1);
  }

  Tokenize();

  context_.sorter->ExtractEventsForced();

  // Verify raw_table and args contents.
  const auto& raw_table = storage_->raw_table();
  EXPECT_EQ(raw_table.row_count(), 1u);
  EXPECT_EQ(raw_table.name()[0],
            storage_->InternString("chrome_event.legacy_system_trace"));
  EXPECT_EQ(raw_table.arg_set_id()[0], 1u);

  EXPECT_EQ(storage_->arg_table().row_count(), 1u);
  EXPECT_TRUE(HasArg(1u, storage_->InternString("data"),
                     Variadic::String(storage_->InternString(kFullData))));
}

TEST_F(ProtoTraceParserTest, ParseChromeLegacyJsonIntoRawTable) {
  static const char kUserTraceEvent[] = "{\"user\":1}";

  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* bundle = packet->set_chrome_events();
    auto* user_trace = bundle->add_legacy_json_trace();
    user_trace->set_type(protos::pbzero::ChromeLegacyJsonTrace::USER_TRACE);
    user_trace->set_data(kUserTraceEvent);
  }

  Tokenize();

  context_.sorter->ExtractEventsForced();

  // Verify raw_table and args contents.
  const auto& raw_table = storage_->raw_table();
  EXPECT_EQ(raw_table.row_count(), 1u);
  EXPECT_EQ(raw_table.name()[0],
            storage_->InternString("chrome_event.legacy_user_trace"));
  EXPECT_EQ(raw_table.arg_set_id()[0], 1u);

  EXPECT_EQ(storage_->arg_table().row_count(), 1u);
  EXPECT_TRUE(
      HasArg(1u, storage_->InternString("data"),
             Variadic::String(storage_->InternString(kUserTraceEvent))));
}

TEST_F(ProtoTraceParserTest, LoadChromeBenchmarkMetadata) {
  static const char kName[] = "name";
  static const char kTag1[] = "tag1";
  static const char kTag2[] = "tag2";

  context_.sorter.reset(new TraceSorter(
      CreateParser(), std::numeric_limits<int64_t>::max() /*window size*/));

  auto* metadata = trace_.add_packet()->set_chrome_benchmark_metadata();
  metadata->set_benchmark_name(kName);
  metadata->add_story_tags(kTag1);
  metadata->add_story_tags(kTag2);

  Tokenize();

  base::StringView benchmark = metadata::kNames[metadata::benchmark_name];
  base::StringView tags = metadata::kNames[metadata::benchmark_story_tags];

  context_.sorter->ExtractEventsForced();

  const auto& meta_keys = storage_->metadata_table().name();
  const auto& meta_values = storage_->metadata_table().str_value();
  EXPECT_EQ(storage_->metadata_table().row_count(), 3u);

  std::vector<std::pair<base::StringView, base::StringView>> meta_entries;
  for (uint32_t i = 0; i < storage_->metadata_table().row_count(); i++) {
    meta_entries.emplace_back(
        std::make_pair(meta_keys.GetString(i), meta_values.GetString(i)));
  }
  EXPECT_THAT(meta_entries,
              UnorderedElementsAreArray({std::make_pair(benchmark, kName),
                                         std::make_pair(tags, kTag1),
                                         std::make_pair(tags, kTag2)}));
}

TEST_F(ProtoTraceParserTest, AndroidPackagesList) {
  auto* packet = trace_.add_packet();
  auto* pkg_list = packet->set_packages_list();

  pkg_list->set_read_error(false);
  pkg_list->set_parse_error(true);
  {
    auto* pkg = pkg_list->add_packages();
    pkg->set_name("com.test.app");
    pkg->set_uid(1000);
    pkg->set_debuggable(false);
    pkg->set_profileable_from_shell(true);
    pkg->set_version_code(42);
  }
  {
    auto* pkg = pkg_list->add_packages();
    pkg->set_name("com.test.app2");
    pkg->set_uid(1001);
    pkg->set_debuggable(false);
    pkg->set_profileable_from_shell(false);
    pkg->set_version_code(43);
  }

  Tokenize();

  // Packet-level errors reflected in stats storage.
  const auto& stats = context_.storage->stats();
  EXPECT_FALSE(stats[stats::packages_list_has_read_errors].value);
  EXPECT_TRUE(stats[stats::packages_list_has_parse_errors].value);

  // Expect two metadata rows, each with an int_value of a separate arg set id.
  // The relevant arg sets have the info about the packages. To simplify test
  // structure, make an assumption that metadata storage is filled in in the
  // FIFO order of seen packages.
  const auto& package_list = context_.storage->package_list_table();
  ASSERT_EQ(package_list.row_count(), 2u);

  EXPECT_STREQ(storage_->GetString(package_list.package_name()[0]).c_str(),
               "com.test.app");
  EXPECT_EQ(package_list.uid()[0], 1000u);
  EXPECT_EQ(package_list.debuggable()[0], false);
  EXPECT_EQ(package_list.profileable_from_shell()[0], true);
  EXPECT_EQ(package_list.version_code()[0], 42);

  EXPECT_STREQ(storage_->GetString(package_list.package_name()[1]).c_str(),
               "com.test.app2");
  EXPECT_EQ(package_list.uid()[1], 1001u);
  EXPECT_EQ(package_list.debuggable()[1], false);
  EXPECT_EQ(package_list.profileable_from_shell()[1], false);
  EXPECT_EQ(package_list.version_code()[1], 43);
}

TEST_F(ProtoTraceParserTest, AndroidPackagesListDuplicate) {
  auto* packet = trace_.add_packet();
  auto* pkg_list = packet->set_packages_list();

  pkg_list->set_read_error(false);
  pkg_list->set_parse_error(true);
  {
    auto* pkg = pkg_list->add_packages();
    pkg->set_name("com.test.app");
    pkg->set_uid(1000);
    pkg->set_debuggable(false);
    pkg->set_profileable_from_shell(true);
    pkg->set_version_code(42);
  }
  {
    auto* pkg = pkg_list->add_packages();
    pkg->set_name("com.test.app");
    pkg->set_uid(1000);
    pkg->set_debuggable(false);
    pkg->set_profileable_from_shell(true);
    pkg->set_version_code(42);
  }

  Tokenize();

  // Packet-level errors reflected in stats storage.
  const auto& stats = context_.storage->stats();
  EXPECT_FALSE(stats[stats::packages_list_has_read_errors].value);
  EXPECT_TRUE(stats[stats::packages_list_has_parse_errors].value);

  // Expect two metadata rows, each with an int_value of a separate arg set id.
  // The relevant arg sets have the info about the packages. To simplify test
  // structure, make an assumption that metadata storage is filled in in the
  // FIFO order of seen packages.
  const auto& package_list = context_.storage->package_list_table();
  ASSERT_EQ(package_list.row_count(), 1u);

  EXPECT_STREQ(storage_->GetString(package_list.package_name()[0]).c_str(),
               "com.test.app");
  EXPECT_EQ(package_list.uid()[0], 1000u);
  EXPECT_EQ(package_list.debuggable()[0], false);
  EXPECT_EQ(package_list.profileable_from_shell()[0], true);
  EXPECT_EQ(package_list.version_code()[0], 42);
}

TEST_F(ProtoTraceParserTest, ParseCPUProfileSamplesIntoTable) {
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);

    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1);
    thread_desc->set_reference_thread_time_us(2);

    auto* interned_data = packet->set_interned_data();

    auto mapping = interned_data->add_mappings();
    mapping->set_iid(1);
    mapping->set_build_id(1);

    auto build_id = interned_data->add_build_ids();
    build_id->set_iid(1);
    build_id->set_str("3BBCFBD372448A727265C3E7C4D954F91");

    auto frame = interned_data->add_frames();
    frame->set_iid(1);
    frame->set_rel_pc(0x42);
    frame->set_mapping_id(1);

    auto frame2 = interned_data->add_frames();
    frame2->set_iid(2);
    frame2->set_rel_pc(0x4242);
    frame2->set_mapping_id(1);

    auto callstack = interned_data->add_callstacks();
    callstack->set_iid(1);
    callstack->add_frame_ids(1);

    auto callstack2 = interned_data->add_callstacks();
    callstack2->set_iid(42);
    callstack2->add_frame_ids(2);
  }

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);

    auto* samples = packet->set_streaming_profile_packet();
    samples->add_callstack_iid(42);
    samples->add_timestamp_delta_us(10);

    samples->add_callstack_iid(1);
    samples->add_timestamp_delta_us(15);
    samples->set_process_priority(20);
  }

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    auto* samples = packet->set_streaming_profile_packet();

    samples->add_callstack_iid(42);
    samples->add_timestamp_delta_us(42);
    samples->set_process_priority(30);
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15))
      .WillRepeatedly(Return(1));

  Tokenize();

  // Verify cpu_profile_samples.
  const auto& samples = storage_->cpu_profile_stack_sample_table();
  EXPECT_EQ(samples.row_count(), 3u);

  EXPECT_EQ(samples.ts()[0], 11000);
  EXPECT_EQ(samples.callsite_id()[0], CallsiteId{0});
  EXPECT_EQ(samples.utid()[0], 1u);
  EXPECT_EQ(samples.process_priority()[0], 20);

  EXPECT_EQ(samples.ts()[1], 26000);
  EXPECT_EQ(samples.callsite_id()[1], CallsiteId{1});
  EXPECT_EQ(samples.utid()[1], 1u);
  EXPECT_EQ(samples.process_priority()[1], 20);

  EXPECT_EQ(samples.ts()[2], 68000);
  EXPECT_EQ(samples.callsite_id()[2], CallsiteId{0});
  EXPECT_EQ(samples.utid()[2], 1u);
  EXPECT_EQ(samples.process_priority()[2], 30);

  // Breakpad build_ids should not be modified/mangled.
  ASSERT_STREQ(
      context_.storage
          ->GetString(storage_->stack_profile_mapping_table().build_id()[0])
          .c_str(),
      "3BBCFBD372448A727265C3E7C4D954F91");
}

TEST_F(ProtoTraceParserTest, CPUProfileSamplesTimestampsAreClockMonotonic) {
  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(0);

    // 1000 us monotonic == 10000 us boottime.
    auto* clock_snapshot = packet->set_clock_snapshot();
    auto* clock_boot = clock_snapshot->add_clocks();
    clock_boot->set_clock_id(protos::pbzero::ClockSnapshot::Clock::BOOTTIME);
    clock_boot->set_timestamp(10000000);
    auto* clock_monotonic = clock_snapshot->add_clocks();
    clock_monotonic->set_clock_id(
        protos::pbzero::ClockSnapshot::Clock::MONOTONIC);
    clock_monotonic->set_timestamp(1000000);
  }

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);
    packet->set_incremental_state_cleared(true);

    auto* thread_desc = packet->set_thread_descriptor();
    thread_desc->set_pid(15);
    thread_desc->set_tid(16);
    thread_desc->set_reference_timestamp_us(1000);
    thread_desc->set_reference_thread_time_us(2000);

    auto* interned_data = packet->set_interned_data();

    auto mapping = interned_data->add_mappings();
    mapping->set_iid(1);
    mapping->set_build_id(1);

    auto build_id = interned_data->add_build_ids();
    build_id->set_iid(1);
    build_id->set_str("3BBCFBD372448A727265C3E7C4D954F91");

    auto frame = interned_data->add_frames();
    frame->set_iid(1);
    frame->set_rel_pc(0x42);
    frame->set_mapping_id(1);

    auto callstack = interned_data->add_callstacks();
    callstack->set_iid(1);
    callstack->add_frame_ids(1);
  }

  {
    auto* packet = trace_.add_packet();
    packet->set_trusted_packet_sequence_id(1);

    auto* samples = packet->set_streaming_profile_packet();
    samples->add_callstack_iid(1);
    samples->add_timestamp_delta_us(15);
  }

  EXPECT_CALL(*process_, UpdateThread(16, 15)).WillRepeatedly(Return(1));

  Tokenize();

  const auto& samples = storage_->cpu_profile_stack_sample_table();
  EXPECT_EQ(samples.row_count(), 1u);

  // Should have been translated to boottime, i.e. 10015 us absolute.
  EXPECT_EQ(samples.ts()[0], 10015000);
  EXPECT_EQ(samples.callsite_id()[0], CallsiteId{0});
  EXPECT_EQ(samples.utid()[0], 1u);
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
