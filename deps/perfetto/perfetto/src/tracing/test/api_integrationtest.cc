/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <fcntl.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

// We also want to test legacy trace events.
#define PERFETTO_ENABLE_LEGACY_TRACE_EVENTS 1

#include "perfetto/tracing.h"
#include "test/gtest_and_gmock.h"

// Deliberately not pulling any non-public perfetto header to spot accidental
// header public -> non-public dependency while building this file.

// These two are the only headers allowed here, see comments in
// api_test_support.h.
#include "src/tracing/test/api_test_support.h"
#include "src/tracing/test/tracing_module.h"

#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/trace_config.h"

// xxx.pbzero.h includes are for the writing path (the code that pretends to be
// production code).
// yyy.gen.h includes are for the test readback path (the code in the test that
// checks that the results are valid).
#include "protos/perfetto/common/track_event_descriptor.gen.h"
#include "protos/perfetto/config/track_event/track_event_config.gen.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "protos/perfetto/trace/interned_data/interned_data.gen.h"
#include "protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_common.gen.h"
#include "protos/perfetto/trace/test_event.gen.h"
#include "protos/perfetto/trace/test_event.pbzero.h"
#include "protos/perfetto/trace/trace.gen.h"
#include "protos/perfetto/trace/trace_packet.gen.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"
#include "protos/perfetto/trace/track_event/chrome_process_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/chrome_process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/debug_annotation.gen.h"
#include "protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "protos/perfetto/trace/track_event/log_message.gen.h"
#include "protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "protos/perfetto/trace/track_event/process_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/source_location.gen.h"
#include "protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.gen.h"
#include "protos/perfetto/trace/track_event/track_event.gen.h"

// Events in categories starting with "dynamic" will use dynamic category
// lookup.
PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES("dynamic");

// Trace categories used in the tests.
PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("test")
        .SetDescription("This is a test category")
        .SetTags("tag"),
    perfetto::Category("foo"),
    perfetto::Category("bar"),
    perfetto::Category("cat").SetTags("slow"),
    perfetto::Category("cat.verbose").SetTags("debug"),
    perfetto::Category::Group("foo,bar"),
    perfetto::Category::Group("baz,bar,quux"),
    perfetto::Category::Group("red,green,blue,foo"),
    perfetto::Category::Group("red,green,blue,yellow"),
    perfetto::Category(TRACE_DISABLED_BY_DEFAULT("cat")));
PERFETTO_TRACK_EVENT_STATIC_STORAGE();

// For testing interning of complex objects.
using SourceLocation = std::tuple<const char* /* file_name */,
                                  const char* /* function_name */,
                                  uint32_t /* line_number */>;

namespace std {
template <>
struct hash<SourceLocation> {
  size_t operator()(const SourceLocation& value) const {
    auto hasher = hash<size_t>();
    return hasher(reinterpret_cast<size_t>(get<0>(value))) ^
           hasher(reinterpret_cast<size_t>(get<1>(value))) ^
           hasher(get<2>(value));
  }
};
}  // namespace std

// Represents an opaque (from Perfetto's point of view) thread identifier (e.g.,
// base::PlatformThreadId in Chromium).
struct MyThreadId {
  MyThreadId(int pid_, int tid_) : pid(pid_), tid(tid_) {}

  const int pid = 0;
  const int tid = 0;
};

// Represents an opaque timestamp (e.g., base::TimeTicks in Chromium).
class MyTimestamp {
 public:
  explicit MyTimestamp(uint64_t ts_) : ts(ts_) {}

  const uint64_t ts;
};

namespace perfetto {
namespace legacy {

template <>
bool ConvertThreadId(const MyThreadId& thread,
                     uint64_t* track_uuid_out,
                     int32_t* pid_override_out,
                     int32_t* tid_override_out) {
  if (!thread.pid && !thread.tid)
    return false;
  if (!thread.pid) {
    // Thread in current process.
    *track_uuid_out = perfetto::ThreadTrack::ForThread(
                          static_cast<base::PlatformThreadId>(thread.tid))
                          .uuid;
  } else {
    // Thread in another process.
    *pid_override_out = thread.pid;
    *tid_override_out = thread.tid;
  }
  return true;
}

template <>
uint64_t ConvertTimestampToTraceTimeNs(const MyTimestamp& timestamp) {
  return timestamp.ts;
}

}  // namespace legacy
}  // namespace perfetto

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::StrEq;

// ------------------------------
// Declarations of helper classes
// ------------------------------
static constexpr auto kWaitEventTimeout = std::chrono::seconds(5);

struct WaitableTestEvent {
  std::mutex mutex_;
  std::condition_variable cv_;
  bool notified_ = false;

  bool notified() {
    std::unique_lock<std::mutex> lock(mutex_);
    return notified_;
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, kWaitEventTimeout, [this] { return notified_; })) {
      fprintf(stderr, "Timed out while waiting for event\n");
      abort();
    }
  }

  void Notify() {
    std::unique_lock<std::mutex> lock(mutex_);
    notified_ = true;
    cv_.notify_one();
  }
};

class MockDataSource;

// We can't easily use gmock here because instances of data sources are lazily
// created by the service and are not owned by the test fixture.
struct TestDataSourceHandle {
  WaitableTestEvent on_create;
  WaitableTestEvent on_setup;
  WaitableTestEvent on_start;
  WaitableTestEvent on_stop;
  MockDataSource* instance;
  perfetto::DataSourceConfig config;
  bool handle_stop_asynchronously = false;
  std::function<void()> on_start_callback;
  std::function<void()> on_stop_callback;
  std::function<void()> async_stop_closure;
};

class MockDataSource : public perfetto::DataSource<MockDataSource> {
 public:
  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;
  TestDataSourceHandle* handle_ = nullptr;
};

class MockDataSource2 : public perfetto::DataSource<MockDataSource2> {
 public:
  void OnSetup(const SetupArgs&) override {}
  void OnStart(const StartArgs&) override {}
  void OnStop(const StopArgs&) override {}
};

// Used to verify that track event data sources in different namespaces register
// themselves correctly in the muxer.
class MockTracingMuxer : public perfetto::internal::TracingMuxer {
 public:
  struct DataSource {
    const perfetto::DataSourceDescriptor dsd;
    perfetto::internal::DataSourceStaticState* static_state;
  };

  MockTracingMuxer() : TracingMuxer(nullptr), prev_instance_(instance_) {
    instance_ = this;
  }
  ~MockTracingMuxer() override { instance_ = prev_instance_; }

  bool RegisterDataSource(
      const perfetto::DataSourceDescriptor& dsd,
      DataSourceFactory,
      perfetto::internal::DataSourceStaticState* static_state) override {
    data_sources.emplace_back(DataSource{dsd, static_state});
    return true;
  }

  std::unique_ptr<perfetto::TraceWriterBase> CreateTraceWriter(
      perfetto::internal::DataSourceState*,
      perfetto::BufferExhaustedPolicy) override {
    return nullptr;
  }

  void DestroyStoppedTraceWritersForCurrentThread() override {}

  std::vector<DataSource> data_sources;

 private:
  TracingMuxer* prev_instance_;
};

struct TestIncrementalState {
  TestIncrementalState() { constructed = true; }
  // Note: a virtual destructor is not required for incremental state.
  ~TestIncrementalState() { destroyed = true; }

  int count = 100;
  static bool constructed;
  static bool destroyed;
};

bool TestIncrementalState::constructed;
bool TestIncrementalState::destroyed;

struct TestIncrementalDataSourceTraits
    : public perfetto::DefaultDataSourceTraits {
  using IncrementalStateType = TestIncrementalState;
};

class TestIncrementalDataSource
    : public perfetto::DataSource<TestIncrementalDataSource,
                                  TestIncrementalDataSourceTraits> {
 public:
  void OnSetup(const SetupArgs&) override {}
  void OnStart(const StartArgs&) override {}
  void OnStop(const StopArgs&) override {}
};

// A convenience wrapper around TracingSession that allows to do block on
//
struct TestTracingSessionHandle {
  perfetto::TracingSession* get() { return session.get(); }
  std::unique_ptr<perfetto::TracingSession> session;
  WaitableTestEvent on_stop;
};

class MyDebugAnnotation : public perfetto::DebugAnnotation {
 public:
  ~MyDebugAnnotation() override = default;

  void Add(
      perfetto::protos::pbzero::DebugAnnotation* annotation) const override {
    annotation->set_legacy_json_value(R"({"key": 123})");
  }
};

// -------------------------
// Declaration of test class
// -------------------------
class PerfettoApiTest : public ::testing::Test {
 public:
  static PerfettoApiTest* instance;

  void SetUp() override {
    instance = this;

    perfetto::TracingInitArgs args;
    args.backends = perfetto::kInProcessBackend;
    perfetto::Tracing::Initialize(args);
    RegisterDataSource<MockDataSource>("my_data_source");
    perfetto::TrackEvent::Register();

    // Make sure our data source always has a valid handle.
    data_sources_["my_data_source"];
  }

  void TearDown() override { instance = nullptr; }

  template <typename DataSourceType>
  TestDataSourceHandle* RegisterDataSource(std::string name) {
    EXPECT_EQ(data_sources_.count(name), 0u);
    TestDataSourceHandle* handle = &data_sources_[name];
    perfetto::DataSourceDescriptor dsd;
    dsd.set_name(name);
    DataSourceType::Register(dsd);
    return handle;
  }

  TestTracingSessionHandle* NewTrace(const perfetto::TraceConfig& cfg,
                                     int fd = -1) {
    sessions_.emplace_back();
    TestTracingSessionHandle* handle = &sessions_.back();
    handle->session =
        perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend);
    handle->session->SetOnStopCallback([handle] { handle->on_stop.Notify(); });
    handle->session->Setup(cfg, fd);
    return handle;
  }

  TestTracingSessionHandle* NewTraceWithCategories(
      std::vector<std::string> categories) {
    perfetto::TraceConfig cfg;
    cfg.set_duration_ms(500);
    cfg.add_buffers()->set_size_kb(1024);
    auto* ds_cfg = cfg.add_data_sources()->mutable_config();
    ds_cfg->set_name("track_event");

    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_disabled_categories("*");
    for (const auto& category : categories)
      te_cfg.add_enabled_categories(category);
    ds_cfg->set_track_event_config_raw(te_cfg.SerializeAsString());

    return NewTrace(cfg);
  }

  std::vector<std::string> ReadLogMessagesFromTrace(
      perfetto::TracingSession* tracing_session) {
    std::vector<char> raw_trace = tracing_session->ReadTraceBlocking();
    EXPECT_GE(raw_trace.size(), 0u);

    // Read back the trace, maintaining interning tables as we go.
    std::vector<std::string> log_messages;
    std::map<uint64_t, std::string> log_message_bodies;
    std::map<uint64_t, perfetto::protos::gen::SourceLocation> source_locations;
    perfetto::protos::gen::Trace parsed_trace;
    EXPECT_TRUE(
        parsed_trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

    for (const auto& packet : parsed_trace.packet()) {
      if (!packet.has_track_event())
        continue;

      if (packet.has_interned_data()) {
        const auto& interned_data = packet.interned_data();
        for (const auto& it : interned_data.log_message_body()) {
          EXPECT_GE(it.iid(), 1u);
          EXPECT_EQ(log_message_bodies.find(it.iid()),
                    log_message_bodies.end());
          log_message_bodies[it.iid()] = it.body();
        }
        for (const auto& it : interned_data.source_locations()) {
          EXPECT_GE(it.iid(), 1u);
          EXPECT_EQ(source_locations.find(it.iid()), source_locations.end());
          source_locations[it.iid()] = it;
        }
      }
      const auto& track_event = packet.track_event();
      if (track_event.type() !=
          perfetto::protos::gen::TrackEvent::TYPE_SLICE_BEGIN)
        continue;

      EXPECT_TRUE(track_event.has_log_message());
      const auto& log = track_event.log_message();
      if (log.source_location_iid()) {
        std::stringstream msg;
        const auto& source_location =
            source_locations[log.source_location_iid()];
        msg << source_location.function_name() << "("
            << source_location.file_name() << ":"
            << source_location.line_number()
            << "): " << log_message_bodies[log.body_iid()];
        log_messages.emplace_back(msg.str());
      } else {
        log_messages.emplace_back(log_message_bodies[log.body_iid()]);
      }
    }
    return log_messages;
  }

  std::vector<std::string> ReadSlicesFromTrace(
      perfetto::TracingSession* tracing_session) {
    std::vector<char> raw_trace = tracing_session->ReadTraceBlocking();
    EXPECT_GE(raw_trace.size(), 0u);

    // Read back the trace, maintaining interning tables as we go.
    std::vector<std::string> slices;
    std::map<uint64_t, std::string> categories;
    std::map<uint64_t, std::string> event_names;
    std::map<uint64_t, std::string> debug_annotation_names;
    perfetto::protos::gen::Trace parsed_trace;
    EXPECT_TRUE(
        parsed_trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

    bool incremental_state_was_cleared = false;
    uint32_t sequence_id = 0;
    for (const auto& packet : parsed_trace.packet()) {
      if (packet.sequence_flags() & perfetto::protos::pbzero::TracePacket::
                                        SEQ_INCREMENTAL_STATE_CLEARED) {
        incremental_state_was_cleared = true;
        categories.clear();
        event_names.clear();
        debug_annotation_names.clear();
      }

      if (!packet.has_track_event())
        continue;

      // Make sure we only see track events on one sequence.
      if (packet.trusted_packet_sequence_id()) {
        if (!sequence_id)
          sequence_id = packet.trusted_packet_sequence_id();
        EXPECT_EQ(sequence_id, packet.trusted_packet_sequence_id());
      }

      // Update incremental state.
      if (packet.has_interned_data()) {
        const auto& interned_data = packet.interned_data();
        for (const auto& it : interned_data.event_categories()) {
          EXPECT_EQ(categories.find(it.iid()), categories.end());
          categories[it.iid()] = it.name();
        }
        for (const auto& it : interned_data.event_names()) {
          EXPECT_EQ(event_names.find(it.iid()), event_names.end());
          event_names[it.iid()] = it.name();
        }
        for (const auto& it : interned_data.debug_annotation_names()) {
          EXPECT_EQ(debug_annotation_names.find(it.iid()),
                    debug_annotation_names.end());
          debug_annotation_names[it.iid()] = it.name();
        }
      }
      const auto& track_event = packet.track_event();
      std::string slice;
      switch (track_event.type()) {
        case perfetto::protos::gen::TrackEvent::TYPE_SLICE_BEGIN:
          slice += "B";
          break;
        case perfetto::protos::gen::TrackEvent::TYPE_SLICE_END:
          slice += "E";
          break;
        case perfetto::protos::gen::TrackEvent::TYPE_INSTANT:
          slice += "I";
          break;
        case perfetto::protos::gen::TrackEvent::TYPE_UNSPECIFIED: {
          EXPECT_TRUE(track_event.has_legacy_event());
          EXPECT_FALSE(track_event.type());
          auto legacy_event = track_event.legacy_event();
          slice += "Legacy_" +
                   std::string(1, static_cast<char>(legacy_event.phase()));
          break;
        }
        case perfetto::protos::gen::TrackEvent::TYPE_COUNTER:
          slice += "C";
          break;
        default:
          ADD_FAILURE();
      }
      if (track_event.has_legacy_event()) {
        auto legacy_event = track_event.legacy_event();
        std::stringstream id;
        if (legacy_event.has_unscoped_id()) {
          id << "(unscoped_id=" << legacy_event.unscoped_id() << ")";
        } else if (legacy_event.has_local_id()) {
          id << "(local_id=" << legacy_event.local_id() << ")";
        } else if (legacy_event.has_global_id()) {
          id << "(global_id=" << legacy_event.global_id() << ")";
        } else if (legacy_event.has_bind_id()) {
          id << "(bind_id=" << legacy_event.bind_id() << ")";
        }
        if (legacy_event.has_id_scope())
          id << "(id_scope=\"" << legacy_event.id_scope() << "\")";
        if (legacy_event.use_async_tts())
          id << "(use_async_tts)";
        if (legacy_event.bind_to_enclosing())
          id << "(bind_to_enclosing)";
        if (legacy_event.has_flow_direction())
          id << "(flow_direction=" << legacy_event.flow_direction() << ")";
        if (legacy_event.has_pid_override())
          id << "(pid_override=" << legacy_event.pid_override() << ")";
        if (legacy_event.has_tid_override())
          id << "(tid_override=" << legacy_event.tid_override() << ")";
        slice += id.str();
      }
      size_t category_count = 0;
      for (const auto& it : track_event.category_iids())
        slice += (category_count++ ? "," : ":") + categories[it];
      for (const auto& it : track_event.categories())
        slice += (category_count++ ? ",$" : ":$") + it;
      if (track_event.has_name_iid())
        slice += "." + event_names[track_event.name_iid()];

      if (track_event.debug_annotations_size()) {
        slice += "(";
        bool first_annotation = true;
        for (const auto& it : track_event.debug_annotations()) {
          if (!first_annotation) {
            slice += ",";
          }
          slice += debug_annotation_names[it.name_iid()] + "=";
          std::stringstream value;
          if (it.has_bool_value()) {
            value << "(bool)" << it.bool_value();
          } else if (it.has_uint_value()) {
            value << "(uint)" << it.uint_value();
          } else if (it.has_int_value()) {
            value << "(int)" << it.int_value();
          } else if (it.has_double_value()) {
            value << "(double)" << it.double_value();
          } else if (it.has_string_value()) {
            value << "(string)" << it.string_value();
          } else if (it.has_pointer_value()) {
            value << "(pointer)" << std::hex << it.pointer_value();
          } else if (it.has_legacy_json_value()) {
            value << "(json)" << it.legacy_json_value();
          } else if (it.has_nested_value()) {
            value << "(nested)" << it.nested_value().string_value();
          }
          slice += value.str();
          first_annotation = false;
        }
        slice += ")";
      }

      slices.push_back(slice);
    }
    EXPECT_TRUE(incremental_state_was_cleared);
    return slices;
  }

  std::map<std::string, TestDataSourceHandle> data_sources_;
  std::list<TestTracingSessionHandle> sessions_;  // Needs stable pointers.
};

// ---------------------------------------------
// Definitions for non-inlineable helper methods
// ---------------------------------------------
PerfettoApiTest* PerfettoApiTest::instance;

void MockDataSource::OnSetup(const SetupArgs& args) {
  EXPECT_EQ(handle_, nullptr);
  auto it = PerfettoApiTest::instance->data_sources_.find(args.config->name());

  // We should not see an OnSetup for a data source that we didn't register
  // before via PerfettoApiTest::RegisterDataSource().
  EXPECT_NE(it, PerfettoApiTest::instance->data_sources_.end());
  handle_ = &it->second;
  handle_->config = *args.config;  // Deliberate copy.
  handle_->on_setup.Notify();
}

void MockDataSource::OnStart(const StartArgs&) {
  EXPECT_NE(handle_, nullptr);
  if (handle_->on_start_callback)
    handle_->on_start_callback();
  handle_->on_start.Notify();
}

void MockDataSource::OnStop(const StopArgs& args) {
  EXPECT_NE(handle_, nullptr);
  if (handle_->handle_stop_asynchronously)
    handle_->async_stop_closure = args.HandleStopAsynchronously();
  if (handle_->on_stop_callback)
    handle_->on_stop_callback();
  handle_->on_stop.Notify();
}

// -------------
// Test fixtures
// -------------

TEST_F(PerfettoApiTest, TrackEventStartStopAndDestroy) {
  // This test used to cause a use after free as the tracing session got
  // destroyed. It needed to be run approximately 2000 times to catch it so test
  // with --gtest_repeat=3000 (less if running under GDB).

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  // Create five new trace sessions.
  std::vector<std::unique_ptr<perfetto::TracingSession>> sessions;
  for (size_t i = 0; i < 5; ++i) {
    sessions.push_back(
        perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend));
    sessions[i]->Setup(cfg);
    sessions[i]->Start();
    sessions[i]->Stop();
  }
}

TEST_F(PerfettoApiTest, TrackEventStartStopAndStopBlocking) {
  // This test used to cause a deadlock (due to StopBlocking() after the session
  // already stopped). This usually occurred within 1 or 2 runs of the test so
  // use --gtest_repeat=10

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  // Create five new trace sessions.
  std::vector<std::unique_ptr<perfetto::TracingSession>> sessions;
  for (size_t i = 0; i < 5; ++i) {
    sessions.push_back(
        perfetto::Tracing::NewTrace(perfetto::BackendType::kInProcessBackend));
    sessions[i]->Setup(cfg);
    sessions[i]->Start();
    sessions[i]->Stop();
  }
  for (auto& session : sessions) {
    session->StopBlocking();
  }
}

// This is a build-only regression test that checks you can have a track event
// inside a template.
template <typename T>
void TestTrackEventInsideTemplate(T) {
  TRACE_EVENT_BEGIN("cat", "Name");
}

TEST_F(PerfettoApiTest, TrackEvent) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"test"});
  tracing_session->get()->StartBlocking();

  // Emit one complete track event.
  TRACE_EVENT_BEGIN("test", "TestEvent");
  TRACE_EVENT_END("test");
  perfetto::TrackEvent::Flush();

  tracing_session->on_stop.Wait();
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);

  // Read back the trace, maintaining interning tables as we go.
  perfetto::protos::gen::Trace trace;
  std::map<uint64_t, std::string> categories;
  std::map<uint64_t, std::string> event_names;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  auto now = perfetto::TrackEvent::GetTraceTimeNs();
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
  auto clock_id = perfetto::protos::pbzero::ClockSnapshot::Clock::BOOTTIME;
#else
  auto clock_id = perfetto::protos::pbzero::ClockSnapshot::Clock::MONOTONIC;
#endif
  EXPECT_EQ(clock_id, perfetto::TrackEvent::GetTraceClockId());

  bool incremental_state_was_cleared = false;
  bool begin_found = false;
  bool end_found = false;
  bool process_descriptor_found = false;
  uint32_t sequence_id = 0;
  int32_t cur_pid = perfetto::test::GetCurrentProcessId();
  for (const auto& packet : trace.packet()) {
    if (packet.has_track_descriptor()) {
      const auto& desc = packet.track_descriptor();
      if (desc.has_process()) {
        EXPECT_FALSE(process_descriptor_found);
        const auto& pd = desc.process();
        EXPECT_EQ(cur_pid, pd.pid());
        process_descriptor_found = true;
      }
    }
    if (packet.sequence_flags() &
        perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED) {
      EXPECT_TRUE(packet.has_trace_packet_defaults());
      incremental_state_was_cleared = true;
      categories.clear();
      event_names.clear();
    }

    if (!packet.has_track_event())
      continue;
    EXPECT_TRUE(
        packet.sequence_flags() &
        (perfetto::protos::pbzero::TracePacket::SEQ_INCREMENTAL_STATE_CLEARED |
         perfetto::protos::pbzero::TracePacket::SEQ_NEEDS_INCREMENTAL_STATE));
    const auto& track_event = packet.track_event();

    // Make sure we only see track events on one sequence.
    if (packet.trusted_packet_sequence_id()) {
      if (!sequence_id)
        sequence_id = packet.trusted_packet_sequence_id();
      EXPECT_EQ(sequence_id, packet.trusted_packet_sequence_id());
    }

    // Update incremental state.
    if (packet.has_interned_data()) {
      const auto& interned_data = packet.interned_data();
      for (const auto& it : interned_data.event_categories()) {
        EXPECT_EQ(categories.find(it.iid()), categories.end());
        categories[it.iid()] = it.name();
      }
      for (const auto& it : interned_data.event_names()) {
        EXPECT_EQ(event_names.find(it.iid()), event_names.end());
        event_names[it.iid()] = it.name();
      }
    }

    EXPECT_GT(packet.timestamp(), 0u);
    EXPECT_LE(packet.timestamp(), now);
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX) && \
    !PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
    EXPECT_FALSE(packet.has_timestamp_clock_id());
#else
    constexpr auto kClockMonotonic =
        perfetto::protos::pbzero::ClockSnapshot::Clock::MONOTONIC;
    EXPECT_EQ(packet.timestamp_clock_id(),
              static_cast<uint32_t>(kClockMonotonic));
#endif
    if (track_event.type() ==
        perfetto::protos::gen::TrackEvent::TYPE_SLICE_BEGIN) {
      EXPECT_FALSE(begin_found);
      EXPECT_EQ(track_event.category_iids().size(), 1u);
      EXPECT_GE(track_event.category_iids()[0], 1u);
      EXPECT_EQ("test", categories[track_event.category_iids()[0]]);
      EXPECT_EQ("TestEvent", event_names[track_event.name_iid()]);
      begin_found = true;
    } else if (track_event.type() ==
               perfetto::protos::gen::TrackEvent::TYPE_SLICE_END) {
      EXPECT_FALSE(end_found);
      EXPECT_EQ(track_event.category_iids().size(), 0u);
      EXPECT_EQ(0u, track_event.name_iid());
      end_found = true;
    }
  }
  EXPECT_TRUE(incremental_state_was_cleared);
  EXPECT_TRUE(process_descriptor_found);
  EXPECT_TRUE(begin_found);
  EXPECT_TRUE(end_found);

  // Dummy instantiation of test template.
  TestTrackEventInsideTemplate(true);
}

TEST_F(PerfettoApiTest, TrackEventCategories) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"bar"});
  tracing_session->get()->StartBlocking();

  // Emit some track events.
  TRACE_EVENT_BEGIN("foo", "NotEnabled");
  TRACE_EVENT_END("foo");
  TRACE_EVENT_BEGIN("bar", "Enabled");
  TRACE_EVENT_END("bar");

  tracing_session->get()->StopBlocking();
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  std::string trace(raw_trace.data(), raw_trace.size());
  // TODO(skyostil): Come up with a nicer way to verify trace contents.
  EXPECT_THAT(trace, HasSubstr("Enabled"));
  EXPECT_THAT(trace, Not(HasSubstr("NotEnabled")));
}

TEST_F(PerfettoApiTest, TrackEventRegistrationWithModule) {
  MockTracingMuxer muxer;

  // Each track event namespace registers its own data source.
  perfetto::TrackEvent::Register();
  EXPECT_EQ(1u, muxer.data_sources.size());

  tracing_module::InitializeCategories();
  EXPECT_EQ(2u, muxer.data_sources.size());

  // Both data sources have the same name but distinct static data (i.e.,
  // individual instance states).
  EXPECT_EQ("track_event", muxer.data_sources[0].dsd.name());
  EXPECT_EQ("track_event", muxer.data_sources[1].dsd.name());
  EXPECT_NE(muxer.data_sources[0].static_state,
            muxer.data_sources[1].static_state);
}

TEST_F(PerfettoApiTest, TrackEventDescriptor) {
  MockTracingMuxer muxer;

  perfetto::TrackEvent::Register();
  EXPECT_EQ(1u, muxer.data_sources.size());
  EXPECT_EQ("track_event", muxer.data_sources[0].dsd.name());

  perfetto::protos::gen::TrackEventDescriptor desc;
  auto desc_raw = muxer.data_sources[0].dsd.track_event_descriptor_raw();
  EXPECT_TRUE(desc.ParseFromArray(desc_raw.data(), desc_raw.size()));

  // Check that the advertised categories match PERFETTO_DEFINE_CATEGORIES (see
  // above).
  EXPECT_EQ(6, desc.available_categories_size());
  EXPECT_EQ("test", desc.available_categories()[0].name());
  EXPECT_EQ("This is a test category",
            desc.available_categories()[0].description());
  EXPECT_EQ("tag", desc.available_categories()[0].tags()[0]);
  EXPECT_EQ("foo", desc.available_categories()[1].name());
  EXPECT_EQ("bar", desc.available_categories()[2].name());
  EXPECT_EQ("cat", desc.available_categories()[3].name());
  EXPECT_EQ("slow", desc.available_categories()[3].tags()[0]);
  EXPECT_EQ("cat.verbose", desc.available_categories()[4].name());
  EXPECT_EQ("debug", desc.available_categories()[4].tags()[0]);
  EXPECT_EQ("disabled-by-default-cat", desc.available_categories()[5].name());
  EXPECT_EQ("slow", desc.available_categories()[5].tags()[0]);
}

TEST_F(PerfettoApiTest, TrackEventSharedIncrementalState) {
  tracing_module::InitializeCategories();

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  perfetto::internal::TrackEventIncrementalState* main_state = nullptr;
  perfetto::TrackEvent::Trace(
      [&main_state](perfetto::TrackEvent::TraceContext ctx) {
        main_state = ctx.GetIncrementalState();
      });
  perfetto::internal::TrackEventIncrementalState* module_state =
      tracing_module::GetIncrementalState();

  // Both track event data sources should use the same incremental state (thanks
  // to sharing TLS).
  EXPECT_NE(nullptr, main_state);
  EXPECT_EQ(main_state, module_state);
  tracing_session->get()->StopBlocking();
}

TEST_F(PerfettoApiTest, TrackEventCategoriesWithModule) {
  // Check that categories defined in two different category registries are
  // enabled and disabled correctly.
  tracing_module::InitializeCategories();

  // Create a new trace session. Only the "foo" category is enabled. It also
  // exists both locally and in the existing module.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  // Emit some track events locally and from the test module.
  TRACE_EVENT_BEGIN("foo", "FooEventFromMain");
  TRACE_EVENT_END("foo");
  tracing_module::EmitTrackEvents();
  tracing_module::EmitTrackEvents2();
  TRACE_EVENT_BEGIN("bar", "DisabledEventFromMain");
  TRACE_EVENT_END("bar");

  tracing_session->get()->StopBlocking();
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  std::string trace(raw_trace.data(), raw_trace.size());
  EXPECT_THAT(trace, HasSubstr("FooEventFromMain"));
  EXPECT_THAT(trace, Not(HasSubstr("DisabledEventFromMain")));
  EXPECT_THAT(trace, HasSubstr("FooEventFromModule"));
  EXPECT_THAT(trace, Not(HasSubstr("DisabledEventFromModule")));
  EXPECT_THAT(trace, HasSubstr("FooEventFromModule2"));
  EXPECT_THAT(trace, Not(HasSubstr("DisabledEventFromModule2")));

  perfetto::protos::gen::Trace parsed_trace;
  ASSERT_TRUE(parsed_trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  uint32_t sequence_id = 0;
  for (const auto& packet : parsed_trace.packet()) {
    if (!packet.has_track_event())
      continue;

    // Make sure we only see track events on one sequence. This means all track
    // event modules are sharing the same trace writer (by using the same TLS
    // index).
    if (packet.trusted_packet_sequence_id()) {
      if (!sequence_id)
        sequence_id = packet.trusted_packet_sequence_id();
      EXPECT_EQ(sequence_id, packet.trusted_packet_sequence_id());
    }
  }
}

TEST_F(PerfettoApiTest, TrackEventDynamicCategories) {
  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  // Session #1 enabled the "dynamic" category.
  auto* tracing_session = NewTraceWithCategories({"dynamic"});
  tracing_session->get()->StartBlocking();

  // Session #2 enables "dynamic_2".
  auto* tracing_session2 = NewTraceWithCategories({"dynamic_2"});
  tracing_session2->get()->StartBlocking();

  // Test naming dynamic categories with std::string.
  perfetto::DynamicCategory dynamic{"dynamic"};
  TRACE_EVENT_BEGIN(dynamic, "EventInDynamicCategory");
  perfetto::DynamicCategory dynamic_disabled{"dynamic_disabled"};
  TRACE_EVENT_BEGIN(dynamic_disabled, "EventInDisabledDynamicCategory");

  // Test naming dynamic categories statically.
  TRACE_EVENT_BEGIN("dynamic", "EventInStaticallyNamedDynamicCategory");

  perfetto::DynamicCategory dynamic_2{"dynamic_2"};
  TRACE_EVENT_BEGIN(dynamic_2, "EventInSecondDynamicCategory");
  TRACE_EVENT_BEGIN("dynamic_2", "EventInSecondStaticallyNamedDynamicCategory");

  std::thread thread([] {
    // Make sure the category name can actually be computed at runtime.
    std::string name = "dyn";
    if (perfetto::base::GetThreadId())
      name += "amic";
    perfetto::DynamicCategory cat{name};
    TRACE_EVENT_BEGIN(cat, "DynamicFromOtherThread");
    perfetto::DynamicCategory cat2{"dynamic_disabled"};
    TRACE_EVENT_BEGIN(cat2, "EventInDisabledDynamicCategory");
  });
  thread.join();

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  std::string trace(raw_trace.data(), raw_trace.size());
  EXPECT_THAT(trace, HasSubstr("EventInDynamicCategory"));
  EXPECT_THAT(trace, Not(HasSubstr("EventInDisabledDynamicCategory")));
  EXPECT_THAT(trace, HasSubstr("DynamicFromOtherThread"));
  EXPECT_THAT(trace, Not(HasSubstr("EventInSecondDynamicCategory")));
  EXPECT_THAT(trace, HasSubstr("EventInStaticallyNamedDynamicCategory"));
  EXPECT_THAT(trace,
              Not(HasSubstr("EventInSecondStaticallyNamedDynamicCategory")));

  tracing_session2->get()->StopBlocking();
  raw_trace = tracing_session2->get()->ReadTraceBlocking();
  trace = std::string(raw_trace.data(), raw_trace.size());
  EXPECT_THAT(trace, Not(HasSubstr("EventInDynamicCategory")));
  EXPECT_THAT(trace, Not(HasSubstr("EventInDisabledDynamicCategory")));
  EXPECT_THAT(trace, Not(HasSubstr("DynamicFromOtherThread")));
  EXPECT_THAT(trace, HasSubstr("EventInSecondDynamicCategory"));
  EXPECT_THAT(trace, Not(HasSubstr("EventInStaticallyNamedDynamicCategory")));
  EXPECT_THAT(trace, HasSubstr("EventInSecondStaticallyNamedDynamicCategory"));
}

TEST_F(PerfettoApiTest, TrackEventConcurrentSessions) {
  // Check that categories that are enabled and disabled in two parallel tracing
  // sessions don't interfere.

  // Session #1 enables the "foo" category.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  // Session #2 enables the "bar" category.
  auto* tracing_session2 = NewTraceWithCategories({"bar"});
  tracing_session2->get()->StartBlocking();

  // Emit some track events under both categories.
  TRACE_EVENT_BEGIN("foo", "Session1_First");
  TRACE_EVENT_END("foo");
  TRACE_EVENT_BEGIN("bar", "Session2_First");
  TRACE_EVENT_END("bar");

  tracing_session->get()->StopBlocking();
  TRACE_EVENT_BEGIN("foo", "Session1_Second");
  TRACE_EVENT_END("foo");
  TRACE_EVENT_BEGIN("bar", "Session2_Second");
  TRACE_EVENT_END("bar");

  tracing_session2->get()->StopBlocking();
  TRACE_EVENT_BEGIN("foo", "Session1_Third");
  TRACE_EVENT_END("foo");
  TRACE_EVENT_BEGIN("bar", "Session2_Third");
  TRACE_EVENT_END("bar");

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  std::string trace(raw_trace.data(), raw_trace.size());
  EXPECT_THAT(trace, HasSubstr("Session1_First"));
  EXPECT_THAT(trace, Not(HasSubstr("Session1_Second")));
  EXPECT_THAT(trace, Not(HasSubstr("Session1_Third")));
  EXPECT_THAT(trace, Not(HasSubstr("Session2_First")));
  EXPECT_THAT(trace, Not(HasSubstr("Session2_Second")));
  EXPECT_THAT(trace, Not(HasSubstr("Session2_Third")));

  std::vector<char> raw_trace2 = tracing_session2->get()->ReadTraceBlocking();
  std::string trace2(raw_trace2.data(), raw_trace2.size());
  EXPECT_THAT(trace2, Not(HasSubstr("Session1_First")));
  EXPECT_THAT(trace2, Not(HasSubstr("Session1_Second")));
  EXPECT_THAT(trace2, Not(HasSubstr("Session1_Third")));
  EXPECT_THAT(trace2, HasSubstr("Session2_First"));
  EXPECT_THAT(trace2, HasSubstr("Session2_Second"));
  EXPECT_THAT(trace2, Not(HasSubstr("Session2_Third")));
}

TEST_F(PerfettoApiTest, TrackEventProcessAndThreadDescriptors) {
  // Thread and process descriptors can be set before tracing is enabled.
  perfetto::TrackEvent::SetProcessDescriptor(
      [](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name("hello.exe");
        desc->set_chrome_process()->set_process_priority(1);
      });

  // Erased tracks shouldn't show up anywhere.
  perfetto::Track erased(1234u);
  perfetto::TrackEvent::SetTrackDescriptor(
      erased, [](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name("ErasedTrack");
      });
  perfetto::TrackEvent::EraseTrackDescriptor(erased);

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();
  TRACE_EVENT_INSTANT("test", "MainThreadEvent");

  std::thread thread([&] {
    perfetto::TrackEvent::SetThreadDescriptor(
        [](perfetto::protos::pbzero::TrackDescriptor* desc) {
          desc->set_name("TestThread");
        });
    TRACE_EVENT_INSTANT("test", "ThreadEvent");
  });
  thread.join();

  // Update the process descriptor while tracing is enabled. It should be
  // immediately reflected in the trace.
  perfetto::TrackEvent::SetProcessDescriptor(
      [](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name("goodbye.exe");
      });
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();

  // After tracing ends, setting the descriptor has no immediate effect.
  perfetto::TrackEvent::SetProcessDescriptor(
      [](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name("noop.exe");
      });

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  std::vector<perfetto::protos::gen::TrackDescriptor> descs;
  std::vector<perfetto::protos::gen::TrackDescriptor> thread_descs;
  constexpr uint32_t kMainThreadSequence = 2;
  for (const auto& packet : trace.packet()) {
    if (packet.has_track_descriptor()) {
      if (packet.trusted_packet_sequence_id() == kMainThreadSequence) {
        descs.push_back(packet.track_descriptor());
      } else {
        thread_descs.push_back(packet.track_descriptor());
      }
    }
  }

  // The main thread records the initial process name as well as the one that's
  // set during tracing. Additionally it records a thread descriptor for the
  // main thread.

  EXPECT_EQ(3u, descs.size());

  // Default track for the main thread.
  EXPECT_EQ(0, descs[0].process().pid());
  EXPECT_NE(0, descs[0].thread().pid());

  // First process descriptor.
  EXPECT_NE(0, descs[1].process().pid());
  EXPECT_EQ("hello.exe", descs[1].name());

  // Second process descriptor.
  EXPECT_NE(0, descs[2].process().pid());
  EXPECT_EQ("goodbye.exe", descs[2].name());

  // The child thread records only its own thread descriptor (twice, since it
  // was mutated).
  EXPECT_EQ(2u, thread_descs.size());
  EXPECT_EQ("TestThread", thread_descs[0].name());
  EXPECT_NE(0, thread_descs[0].thread().pid());
  EXPECT_NE(0, thread_descs[0].thread().tid());
  EXPECT_EQ("TestThread", thread_descs[1].name());
  EXPECT_NE(0, thread_descs[1].thread().pid());
  EXPECT_NE(0, thread_descs[1].thread().tid());
}

TEST_F(PerfettoApiTest, TrackEventCustomTrack) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"bar"});
  tracing_session->get()->StartBlocking();

  // Declare a custom track and give it a name.
  uint64_t async_id = 123;
  perfetto::TrackEvent::SetTrackDescriptor(
      perfetto::Track(async_id),
      [](perfetto::protos::pbzero::TrackDescriptor* desc) {
        desc->set_name("MyCustomTrack");
      });

  // Start events on one thread and end them on another.
  TRACE_EVENT_BEGIN("bar", "AsyncEvent", perfetto::Track(async_id), "debug_arg",
                    123);

  TRACE_EVENT_BEGIN("bar", "SubEvent", perfetto::Track(async_id),
                    [](perfetto::EventContext) {});
  const auto main_thread_track =
      perfetto::Track(async_id, perfetto::ThreadTrack::Current());
  std::thread thread([&] {
    TRACE_EVENT_END("bar", perfetto::Track(async_id));
    TRACE_EVENT_END("bar", perfetto::Track(async_id), "arg1", false, "arg2",
                    true);
    const auto thread_track =
        perfetto::Track(async_id, perfetto::ThreadTrack::Current());
    // Thread-scoped tracks will have different uuids on different thread even
    // if the id matches.
    ASSERT_NE(main_thread_track.uuid, thread_track.uuid);
  });
  thread.join();

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  // Check that the track uuids match on the begin and end events.
  const auto track = perfetto::Track(async_id);
  constexpr uint32_t kMainThreadSequence = 2;
  int event_count = 0;
  bool found_descriptor = false;
  for (const auto& packet : trace.packet()) {
    if (packet.has_track_descriptor() &&
        !packet.track_descriptor().has_process() &&
        !packet.track_descriptor().has_thread()) {
      auto td = packet.track_descriptor();
      EXPECT_EQ("MyCustomTrack", td.name());
      EXPECT_EQ(track.uuid, td.uuid());
      EXPECT_EQ(perfetto::ProcessTrack::Current().uuid, td.parent_uuid());
      found_descriptor = true;
      continue;
    }

    if (!packet.has_track_event())
      continue;
    auto track_event = packet.track_event();
    if (track_event.type() ==
        perfetto::protos::gen::TrackEvent::TYPE_SLICE_BEGIN) {
      EXPECT_EQ(kMainThreadSequence, packet.trusted_packet_sequence_id());
      EXPECT_EQ(track.uuid, track_event.track_uuid());
    } else {
      EXPECT_NE(kMainThreadSequence, packet.trusted_packet_sequence_id());
      EXPECT_EQ(track.uuid, track_event.track_uuid());
    }
    event_count++;
  }
  EXPECT_TRUE(found_descriptor);
  EXPECT_EQ(4, event_count);
  perfetto::TrackEvent::EraseTrackDescriptor(track);
}

TEST_F(PerfettoApiTest, TrackEventCustomTrackAndTimestamp) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"bar"});
  tracing_session->get()->StartBlocking();

  // Custom track.
  perfetto::Track track(789);

  auto empty_lambda = [](perfetto::EventContext) {};
  constexpr uint64_t kBeginEventTime = 10;
  constexpr uint64_t kEndEventTime = 15;
  TRACE_EVENT_BEGIN("bar", "Event", track, kBeginEventTime, empty_lambda);
  TRACE_EVENT_END("bar", track, kEndEventTime, empty_lambda);

  constexpr uint64_t kInstantEventTime = 1;
  TRACE_EVENT_INSTANT("bar", "InstantEvent", track, kInstantEventTime,
                      empty_lambda);

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  int event_count = 0;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_track_event())
      continue;
    event_count++;
    switch (packet.track_event().type()) {
      case perfetto::protos::gen::TrackEvent::TYPE_SLICE_BEGIN:
        EXPECT_EQ(packet.timestamp(), kBeginEventTime);
        break;
      case perfetto::protos::gen::TrackEvent::TYPE_SLICE_END:
        EXPECT_EQ(packet.timestamp(), kEndEventTime);
        break;
      case perfetto::protos::gen::TrackEvent::TYPE_INSTANT:
        EXPECT_EQ(packet.timestamp(), kInstantEventTime);
        break;
      case perfetto::protos::gen::TrackEvent::TYPE_COUNTER:
      case perfetto::protos::gen::TrackEvent::TYPE_UNSPECIFIED:
        ADD_FAILURE();
    }
  }
  EXPECT_EQ(event_count, 3);
  perfetto::TrackEvent::EraseTrackDescriptor(track);
}

TEST_F(PerfettoApiTest, TrackEventAnonymousCustomTrack) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"bar"});
  tracing_session->get()->StartBlocking();

  // Emit an async event without giving it an explicit descriptor.
  uint64_t async_id = 4004;
  auto track = perfetto::Track(async_id, perfetto::ThreadTrack::Current());
  TRACE_EVENT_BEGIN("bar", "AsyncEvent", track);
  std::thread thread([&] { TRACE_EVENT_END("bar", track); });
  thread.join();

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  // Check that a descriptor for the track was emitted.
  bool found_descriptor = false;
  for (const auto& packet : trace.packet()) {
    if (packet.has_track_descriptor() &&
        !packet.track_descriptor().has_process() &&
        !packet.track_descriptor().has_thread()) {
      auto td = packet.track_descriptor();
      EXPECT_EQ(track.uuid, td.uuid());
      EXPECT_EQ(perfetto::ThreadTrack::Current().uuid, td.parent_uuid());
      found_descriptor = true;
    }
  }
  EXPECT_TRUE(found_descriptor);
}

TEST_F(PerfettoApiTest, TrackEventTypedArgs) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  auto random_value = random();
  TRACE_EVENT_BEGIN("foo", "EventWithTypedArg",
                    [random_value](perfetto::EventContext ctx) {
                      auto* log = ctx.event()->set_log_message();
                      log->set_source_location_iid(1);
                      log->set_body_iid(2);
                      auto* dbg = ctx.event()->add_debug_annotations();
                      dbg->set_name("random");
                      dbg->set_int_value(random_value);
                    });
  TRACE_EVENT_END("foo");

  tracing_session->get()->StopBlocking();
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  std::string trace(raw_trace.data(), raw_trace.size());

  perfetto::protos::gen::Trace parsed_trace;
  ASSERT_TRUE(parsed_trace.ParseFromArray(raw_trace.data(), raw_trace.size()));

  bool found_args = false;
  for (const auto& packet : parsed_trace.packet()) {
    if (!packet.has_track_event())
      continue;
    const auto& track_event = packet.track_event();
    if (track_event.type() !=
        perfetto::protos::gen::TrackEvent::TYPE_SLICE_BEGIN)
      continue;

    EXPECT_TRUE(track_event.has_log_message());
    const auto& log = track_event.log_message();
    EXPECT_EQ(1u, log.source_location_iid());
    EXPECT_EQ(2u, log.body_iid());

    const auto& dbg = track_event.debug_annotations()[0];
    EXPECT_EQ("random", dbg.name());
    EXPECT_EQ(random_value, dbg.int_value());

    found_args = true;
  }
  EXPECT_TRUE(found_args);
}

struct InternedLogMessageBody
    : public perfetto::TrackEventInternedDataIndex<
          InternedLogMessageBody,
          perfetto::protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
          std::string> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const std::string& value) {
    auto l = interned_data->add_log_message_body();
    l->set_iid(iid);
    l->set_body(value.data(), value.size());
    commit_count++;
  }

  static int commit_count;
};

int InternedLogMessageBody::commit_count = 0;

TEST_F(PerfettoApiTest, TrackEventTypedArgsWithInterning) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  std::stringstream large_message;
  for (size_t i = 0; i < 512; i++)
    large_message << i << ". Something wicked this way comes. ";

  size_t body_iid;
  InternedLogMessageBody::commit_count = 0;
  TRACE_EVENT_BEGIN("foo", "EventWithState", [&](perfetto::EventContext ctx) {
    EXPECT_EQ(0, InternedLogMessageBody::commit_count);
    body_iid = InternedLogMessageBody::Get(&ctx, "Alas, poor Yorick!");
    auto log = ctx.event()->set_log_message();
    log->set_body_iid(body_iid);
    EXPECT_EQ(1, InternedLogMessageBody::commit_count);

    auto body_iid2 = InternedLogMessageBody::Get(&ctx, "Alas, poor Yorick!");
    EXPECT_EQ(body_iid, body_iid2);
    EXPECT_EQ(1, InternedLogMessageBody::commit_count);
  });
  TRACE_EVENT_END("foo");

  TRACE_EVENT_BEGIN("foo", "EventWithState", [&](perfetto::EventContext ctx) {
    // Check that very large amounts of interned data works.
    auto log = ctx.event()->set_log_message();
    log->set_body_iid(InternedLogMessageBody::Get(&ctx, large_message.str()));
    EXPECT_EQ(2, InternedLogMessageBody::commit_count);
  });
  TRACE_EVENT_END("foo");

  // Make sure interned data persists across trace points.
  TRACE_EVENT_BEGIN("foo", "EventWithState", [&](perfetto::EventContext ctx) {
    auto body_iid2 = InternedLogMessageBody::Get(&ctx, "Alas, poor Yorick!");
    EXPECT_EQ(body_iid, body_iid2);

    auto body_iid3 = InternedLogMessageBody::Get(&ctx, "I knew him, Horatio");
    EXPECT_NE(body_iid, body_iid3);
    auto log = ctx.event()->set_log_message();
    log->set_body_iid(body_iid3);
    EXPECT_EQ(3, InternedLogMessageBody::commit_count);
  });
  TRACE_EVENT_END("foo");

  tracing_session->get()->StopBlocking();
  auto log_messages = ReadLogMessagesFromTrace(tracing_session->get());
  EXPECT_THAT(log_messages,
              ElementsAre("Alas, poor Yorick!", large_message.str(),
                          "I knew him, Horatio"));
}

struct InternedLogMessageBodySmall
    : public perfetto::TrackEventInternedDataIndex<
          InternedLogMessageBodySmall,
          perfetto::protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
          const char*,
          perfetto::SmallInternedDataTraits> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const char* value) {
    auto l = interned_data->add_log_message_body();
    l->set_iid(iid);
    l->set_body(value);
  }
};

TEST_F(PerfettoApiTest, TrackEventTypedArgsWithInterningByValue) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  size_t body_iid;
  TRACE_EVENT_BEGIN("foo", "EventWithState", [&](perfetto::EventContext ctx) {
    body_iid = InternedLogMessageBodySmall::Get(&ctx, "This above all:");
    auto log = ctx.event()->set_log_message();
    log->set_body_iid(body_iid);

    auto body_iid2 = InternedLogMessageBodySmall::Get(&ctx, "This above all:");
    EXPECT_EQ(body_iid, body_iid2);

    auto body_iid3 =
        InternedLogMessageBodySmall::Get(&ctx, "to thine own self be true");
    EXPECT_NE(body_iid, body_iid3);
  });
  TRACE_EVENT_END("foo");

  tracing_session->get()->StopBlocking();
  auto log_messages = ReadLogMessagesFromTrace(tracing_session->get());
  EXPECT_THAT(log_messages, ElementsAre("This above all:"));
}

struct InternedLogMessageBodyHashed
    : public perfetto::TrackEventInternedDataIndex<
          InternedLogMessageBodyHashed,
          perfetto::protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
          std::string,
          perfetto::HashedInternedDataTraits> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const std::string& value) {
    auto l = interned_data->add_log_message_body();
    l->set_iid(iid);
    l->set_body(value.data(), value.size());
  }
};

TEST_F(PerfettoApiTest, TrackEventTypedArgsWithInterningByHashing) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  size_t body_iid;
  TRACE_EVENT_BEGIN("foo", "EventWithState", [&](perfetto::EventContext ctx) {
    // Test using a dynamically created interned value.
    body_iid = InternedLogMessageBodyHashed::Get(
        &ctx, std::string("Though this ") + "be madness,");
    auto log = ctx.event()->set_log_message();
    log->set_body_iid(body_iid);

    auto body_iid2 =
        InternedLogMessageBodyHashed::Get(&ctx, "Though this be madness,");
    EXPECT_EQ(body_iid, body_iid2);

    auto body_iid3 =
        InternedLogMessageBodyHashed::Get(&ctx, "yet there is method in’t");
    EXPECT_NE(body_iid, body_iid3);
  });
  TRACE_EVENT_END("foo");

  tracing_session->get()->StopBlocking();
  auto log_messages = ReadLogMessagesFromTrace(tracing_session->get());
  EXPECT_THAT(log_messages, ElementsAre("Though this be madness,"));
}

struct InternedSourceLocation
    : public perfetto::TrackEventInternedDataIndex<
          InternedSourceLocation,
          perfetto::protos::pbzero::InternedData::kSourceLocationsFieldNumber,
          SourceLocation> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const SourceLocation& value) {
    auto l = interned_data->add_source_locations();
    auto file_name = std::get<0>(value);
    auto function_name = std::get<1>(value);
    auto line_number = std::get<2>(value);
    l->set_iid(iid);
    l->set_file_name(file_name);
    l->set_function_name(function_name);
    l->set_line_number(line_number);
  }
};

TEST_F(PerfettoApiTest, TrackEventTypedArgsWithInterningComplexValue) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  TRACE_EVENT_BEGIN("foo", "EventWithState", [&](perfetto::EventContext ctx) {
    const SourceLocation location{"file.cc", "SomeFunction", 123};
    auto location_iid = InternedSourceLocation::Get(&ctx, location);
    auto body_iid = InternedLogMessageBody::Get(&ctx, "To be, or not to be");
    auto log = ctx.event()->set_log_message();
    log->set_source_location_iid(location_iid);
    log->set_body_iid(body_iid);

    auto location_iid2 = InternedSourceLocation::Get(&ctx, location);
    EXPECT_EQ(location_iid, location_iid2);

    const SourceLocation location2{"file.cc", "SomeFunction", 456};
    auto location_iid3 = InternedSourceLocation::Get(&ctx, location2);
    EXPECT_NE(location_iid, location_iid3);
  });
  TRACE_EVENT_END("foo");

  tracing_session->get()->StopBlocking();
  auto log_messages = ReadLogMessagesFromTrace(tracing_session->get());
  EXPECT_THAT(log_messages,
              ElementsAre("SomeFunction(file.cc:123): To be, or not to be"));
}

TEST_F(PerfettoApiTest, TrackEventScoped) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"test"});
  tracing_session->get()->StartBlocking();

  {
    uint64_t arg = 123;
    TRACE_EVENT("test", "TestEventWithArgs", [&](perfetto::EventContext ctx) {
      ctx.event()->set_log_message()->set_body_iid(arg);
    });
  }

  // Ensure a single line if statement counts as a valid scope for the macro.
  if (true)
    TRACE_EVENT("test", "SingleLineTestEvent");

  {
    // Make sure you can have multiple scoped events in the same scope.
    TRACE_EVENT("test", "TestEvent");
    TRACE_EVENT("test", "AnotherEvent");
    TRACE_EVENT("foo", "DisabledEvent");
  }
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(
      slices,
      ElementsAre("B:test.TestEventWithArgs", "E", "B:test.SingleLineTestEvent",
                  "E", "B:test.TestEvent", "B:test.AnotherEvent", "E", "E"));
}

TEST_F(PerfettoApiTest, TrackEventInstant) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"test"});
  tracing_session->get()->StartBlocking();

  TRACE_EVENT_INSTANT("test", "TestEvent");
  TRACE_EVENT_INSTANT("test", "AnotherEvent");
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(slices, ElementsAre("I:test.TestEvent", "I:test.AnotherEvent"));
}

TEST_F(PerfettoApiTest, TrackEventDebugAnnotations) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"test"});
  tracing_session->get()->StartBlocking();

  enum MyEnum { ENUM_FOO, ENUM_BAR };
  enum MySignedEnum { SIGNED_ENUM_FOO = -1, SIGNED_ENUM_BAR };

  TRACE_EVENT_BEGIN("test", "E", "bool_arg", false);
  TRACE_EVENT_BEGIN("test", "E", "int_arg", -123);
  TRACE_EVENT_BEGIN("test", "E", "uint_arg", 456u);
  TRACE_EVENT_BEGIN("test", "E", "float_arg", 3.14159262f);
  TRACE_EVENT_BEGIN("test", "E", "double_arg", 6.22);
  TRACE_EVENT_BEGIN("test", "E", "str_arg", "hello", "str_arg2",
                    std::string("tracing"));
  TRACE_EVENT_BEGIN("test", "E", "ptr_arg",
                    reinterpret_cast<void*>(0xbaadf00d));
  TRACE_EVENT_BEGIN("test", "E", "size_t_arg", size_t{42});
  TRACE_EVENT_BEGIN("test", "E", "ptrdiff_t_arg", ptrdiff_t{-7});
  TRACE_EVENT_BEGIN("test", "E", "enum_arg", ENUM_BAR);
  TRACE_EVENT_BEGIN("test", "E", "signed_enum_arg", SIGNED_ENUM_FOO);
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(
      slices,
      ElementsAre(
          "B:test.E(bool_arg=(bool)0)", "B:test.E(int_arg=(int)-123)",
          "B:test.E(uint_arg=(uint)456)", "B:test.E(float_arg=(double)3.14159)",
          "B:test.E(double_arg=(double)6.22)",
          "B:test.E(str_arg=(string)hello,str_arg2=(string)tracing)",
          "B:test.E(ptr_arg=(pointer)baadf00d)",
          "B:test.E(size_t_arg=(uint)42)", "B:test.E(ptrdiff_t_arg=(int)-7)",
          "B:test.E(enum_arg=(uint)1)", "B:test.E(signed_enum_arg=(int)-1)"));
}

TEST_F(PerfettoApiTest, TrackEventCustomDebugAnnotations) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"test"});
  tracing_session->get()->StartBlocking();

  std::unique_ptr<MyDebugAnnotation> owned_annotation(new MyDebugAnnotation());

  TRACE_EVENT_BEGIN("test", "E", "custom_arg", MyDebugAnnotation());
  TRACE_EVENT_BEGIN("test", "E", "normal_arg", "x", "custom_arg",
                    std::move(owned_annotation));
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(
      slices,
      ElementsAre(
          R"(B:test.E(custom_arg=(json){"key": 123}))",
          R"(B:test.E(normal_arg=(string)x,custom_arg=(json){"key": 123}))"));
}

TEST_F(PerfettoApiTest, TrackEventCustomRawDebugAnnotations) {
  // Note: this class is also testing a non-moveable and non-copiable argument.
  class MyRawDebugAnnotation : public perfetto::DebugAnnotation {
   public:
    MyRawDebugAnnotation() { msg_->set_string_value("nested_value"); }
    ~MyRawDebugAnnotation() = default;

    // |msg_| already deletes these implicitly, but let's be explicit for safety
    // against future changes.
    MyRawDebugAnnotation(const MyRawDebugAnnotation&) = delete;
    MyRawDebugAnnotation(MyRawDebugAnnotation&&) = delete;

    void Add(perfetto::protos::pbzero::DebugAnnotation* annotation) const {
      auto ranges = msg_.GetRanges();
      annotation->AppendScatteredBytes(
          perfetto::protos::pbzero::DebugAnnotation::kNestedValueFieldNumber,
          &ranges[0], ranges.size());
    }

   private:
    mutable protozero::HeapBuffered<
        perfetto::protos::pbzero::DebugAnnotation::NestedValue>
        msg_;
  };

  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"test"});
  tracing_session->get()->StartBlocking();

  TRACE_EVENT_BEGIN("test", "E", "raw_arg", MyRawDebugAnnotation());
  TRACE_EVENT_BEGIN("test", "E", "plain_arg", 42, "raw_arg",
                    MyRawDebugAnnotation());
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(
      slices,
      ElementsAre("B:test.E(raw_arg=(nested)nested_value)",
                  "B:test.E(plain_arg=(int)42,raw_arg=(nested)nested_value)"));
}

TEST_F(PerfettoApiTest, TrackEventComputedName) {
  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // New macros require perfetto::StaticString{} annotation.
  for (int i = 0; i < 3; i++)
    TRACE_EVENT_BEGIN("test", perfetto::StaticString{i % 2 ? "Odd" : "Even"});

  // Legacy macros assume all arguments are static strings.
  for (int i = 0; i < 3; i++)
    TRACE_EVENT_BEGIN0("test", i % 2 ? "Odd" : "Even");

  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(slices, ElementsAre("B:test.Even", "B:test.Odd", "B:test.Even",
                                  "B:test.Even", "B:test.Odd", "B:test.Even"));
}

TEST_F(PerfettoApiTest, TrackEventArgumentsNotEvaluatedWhenDisabled) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"foo"});
  tracing_session->get()->StartBlocking();

  bool called = false;
  auto ArgumentFunction = [&] {
    called = true;
    return 123;
  };

  TRACE_EVENT_BEGIN("test", "DisabledEvent", "arg", ArgumentFunction());
  { TRACE_EVENT("test", "DisabledScopedEvent", "arg", ArgumentFunction()); }
  perfetto::TrackEvent::Flush();

  tracing_session->get()->StopBlocking();
  EXPECT_FALSE(called);

  ArgumentFunction();
  EXPECT_TRUE(called);
}

TEST_F(PerfettoApiTest, TrackEventConfig) {
  auto check_config = [&](perfetto::protos::gen::TrackEventConfig te_cfg) {
    perfetto::TraceConfig cfg;
    cfg.set_duration_ms(500);
    cfg.add_buffers()->set_size_kb(1024);
    auto* ds_cfg = cfg.add_data_sources()->mutable_config();
    ds_cfg->set_name("track_event");
    ds_cfg->set_track_event_config_raw(te_cfg.SerializeAsString());

    auto* tracing_session = NewTrace(cfg);
    tracing_session->get()->StartBlocking();

    TRACE_EVENT_BEGIN("foo", "FooEvent");
    TRACE_EVENT_BEGIN("bar", "BarEvent");
    TRACE_EVENT_BEGIN("foo,bar", "MultiFooBar");
    TRACE_EVENT_BEGIN("baz,bar,quux", "MultiBar");
    TRACE_EVENT_BEGIN("red,green,blue,foo", "MultiFoo");
    TRACE_EVENT_BEGIN("red,green,blue,yellow", "MultiNone");
    TRACE_EVENT_BEGIN("cat", "SlowEvent");
    TRACE_EVENT_BEGIN("cat.verbose", "DebugEvent");
    TRACE_EVENT_BEGIN("test", "TagEvent");
    TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("cat"), "SlowDisabledEvent");
    TRACE_EVENT_BEGIN("dynamic,foo", "DynamicGroupFooEvent");
    perfetto::DynamicCategory dyn{"dynamic,bar"};
    TRACE_EVENT_BEGIN(dyn, "DynamicGroupBarEvent");

    perfetto::TrackEvent::Flush();
    tracing_session->get()->StopBlocking();
    auto slices = ReadSlicesFromTrace(tracing_session->get());
    tracing_session->session.reset();
    return slices;
  };

  // Empty config should enable all categories except slow ones.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    auto slices = check_config(te_cfg);
    EXPECT_THAT(
        slices,
        ElementsAre("B:foo.FooEvent", "B:bar.BarEvent", "B:foo,bar.MultiFooBar",
                    "B:baz,bar,quux.MultiBar", "B:red,green,blue,foo.MultiFoo",
                    "B:test.TagEvent", "B:$dynamic,$foo.DynamicGroupFooEvent",
                    "B:$dynamic,$bar.DynamicGroupBarEvent"));
  }

  // Enable exactly one category.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_disabled_categories("*");
    te_cfg.add_enabled_categories("foo");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(slices, ElementsAre("B:foo.FooEvent", "B:foo,bar.MultiFooBar",
                                    "B:red,green,blue,foo.MultiFoo",
                                    "B:$dynamic,$foo.DynamicGroupFooEvent"));
  }

  // Enable two categories.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_disabled_categories("*");
    te_cfg.add_enabled_categories("foo");
    te_cfg.add_enabled_categories("baz");
    te_cfg.add_enabled_categories("bar");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(
        slices,
        ElementsAre("B:foo.FooEvent", "B:bar.BarEvent", "B:foo,bar.MultiFooBar",
                    "B:baz,bar,quux.MultiBar", "B:red,green,blue,foo.MultiFoo",
                    "B:$dynamic,$foo.DynamicGroupFooEvent",
                    "B:$dynamic,$bar.DynamicGroupBarEvent"));
  }

  // Enabling all categories with a pattern doesn't enable slow ones.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_enabled_categories("*");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(
        slices,
        ElementsAre("B:foo.FooEvent", "B:bar.BarEvent", "B:foo,bar.MultiFooBar",
                    "B:baz,bar,quux.MultiBar", "B:red,green,blue,foo.MultiFoo",
                    "B:test.TagEvent", "B:$dynamic,$foo.DynamicGroupFooEvent",
                    "B:$dynamic,$bar.DynamicGroupBarEvent"));
  }

  // Enable with a pattern.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_disabled_categories("*");
    te_cfg.add_enabled_categories("fo*");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(slices, ElementsAre("B:foo.FooEvent", "B:foo,bar.MultiFooBar",
                                    "B:red,green,blue,foo.MultiFoo",
                                    "B:$dynamic,$foo.DynamicGroupFooEvent"));
  }

  // Enable with a tag.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_disabled_categories("*");
    te_cfg.add_enabled_tags("tag");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(slices, ElementsAre("B:test.TagEvent"));
  }

  // Enable just slow categories.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_disabled_categories("*");
    te_cfg.add_enabled_tags("slow");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(slices,
                ElementsAre("B:cat.SlowEvent",
                            "B:disabled-by-default-cat.SlowDisabledEvent"));
  }

  // Enable everything including slow/debug categories.
  {
    perfetto::protos::gen::TrackEventConfig te_cfg;
    te_cfg.add_enabled_categories("*");
    te_cfg.add_enabled_tags("slow");
    te_cfg.add_enabled_tags("debug");
    auto slices = check_config(te_cfg);
    EXPECT_THAT(slices,
                ElementsAre("B:foo.FooEvent", "B:bar.BarEvent",
                            "B:foo,bar.MultiFooBar", "B:baz,bar,quux.MultiBar",
                            "B:red,green,blue,foo.MultiFoo", "B:cat.SlowEvent",
                            "B:cat.verbose.DebugEvent", "B:test.TagEvent",
                            "B:disabled-by-default-cat.SlowDisabledEvent",
                            "B:$dynamic,$foo.DynamicGroupFooEvent",
                            "B:$dynamic,$bar.DynamicGroupBarEvent"));
  }
}

TEST_F(PerfettoApiTest, OneDataSourceOneEvent) {
  auto* data_source = &data_sources_["my_data_source"];

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  ds_cfg->set_legacy_config("test config");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);

  MockDataSource::Trace([](MockDataSource::TraceContext) {
    FAIL() << "Should not be called because the trace was not started";
  });
  MockDataSource::CallIfEnabled([](uint32_t) {
    FAIL() << "Should not be called because the trace was not started";
  });

  tracing_session->get()->Start();
  data_source->on_setup.Wait();
  EXPECT_EQ(data_source->config.legacy_config(), "test config");
  data_source->on_start.Wait();

  // Emit one trace event.
  std::atomic<int> trace_lambda_calls{0};
  MockDataSource::Trace(
      [&trace_lambda_calls](MockDataSource::TraceContext ctx) {
        auto packet = ctx.NewTracePacket();
        packet->set_timestamp(42);
        packet->set_for_testing()->set_str("event 1");
        trace_lambda_calls++;
        packet->Finalize();

        // The SMB scraping logic will skip the last packet because it cannot
        // guarantee it's finalized. Create an empty packet so we get the
        // previous one and this empty one is ignored.
        packet = ctx.NewTracePacket();
      });

  uint32_t active_instances = 0;
  MockDataSource::CallIfEnabled([&active_instances](uint32_t instances) {
    active_instances = instances;
  });
  EXPECT_EQ(1u, active_instances);

  data_source->on_stop.Wait();
  tracing_session->on_stop.Wait();
  EXPECT_EQ(trace_lambda_calls, 1);

  MockDataSource::Trace([](MockDataSource::TraceContext) {
    FAIL() << "Should not be called because the trace is now stopped";
  });
  MockDataSource::CallIfEnabled([](uint32_t) {
    FAIL() << "Should not be called because the trace is now stopped";
  });

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);

  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));
  bool test_packet_found = false;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_for_testing())
      continue;
    EXPECT_FALSE(test_packet_found);
    EXPECT_EQ(packet.timestamp(), 42U);
    EXPECT_EQ(packet.for_testing().str(), "event 1");
    test_packet_found = true;
  }
  EXPECT_TRUE(test_packet_found);
}

TEST_F(PerfettoApiTest, BlockingStartAndStop) {
  auto* data_source = &data_sources_["my_data_source"];

  // Register a second data source to get a bit more coverage.
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("my_data_source2");
  MockDataSource2::Register(dsd);

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source2");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);

  tracing_session->get()->StartBlocking();
  EXPECT_TRUE(data_source->on_setup.notified());
  EXPECT_TRUE(data_source->on_start.notified());

  tracing_session->get()->StopBlocking();
  EXPECT_TRUE(data_source->on_stop.notified());
  EXPECT_TRUE(tracing_session->on_stop.notified());
}

TEST_F(PerfettoApiTest, BlockingStartAndStopOnEmptySession) {
  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("non_existent_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();
  tracing_session->get()->StopBlocking();
  EXPECT_TRUE(tracing_session->on_stop.notified());
}

TEST_F(PerfettoApiTest, WriteEventsAfterDeferredStop) {
  auto* data_source = &data_sources_["my_data_source"];
  data_source->handle_stop_asynchronously = true;

  // Setup the trace config and start the tracing session.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // Stop and wait for the producer to have seen the stop event.
  WaitableTestEvent consumer_stop_signal;
  tracing_session->get()->SetOnStopCallback(
      [&consumer_stop_signal] { consumer_stop_signal.Notify(); });
  tracing_session->get()->Stop();
  data_source->on_stop.Wait();

  // At this point tracing should be still allowed because of the
  // HandleStopAsynchronously() call.
  bool lambda_called = false;

  // This usleep is here just to prevent that we accidentally pass the test
  // just by virtue of hitting some race. We should be able to trace up until
  // 5 seconds after seeing the stop when using the deferred stop mechanism.
  usleep(250 * 1000);

  MockDataSource::Trace([&lambda_called](MockDataSource::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_for_testing()->set_str("event written after OnStop");
    packet->Finalize();
    ctx.Flush();
    lambda_called = true;
  });
  ASSERT_TRUE(lambda_called);

  // Now call the async stop closure. This acks the stop to the service and
  // disallows further Trace() calls.
  EXPECT_TRUE(data_source->async_stop_closure);
  data_source->async_stop_closure();

  // Wait that the stop is propagated to the consumer.
  consumer_stop_signal.Wait();

  MockDataSource::Trace([](MockDataSource::TraceContext) {
    FAIL() << "Should not be called after the stop is acked";
  });

  // Check the contents of the trace.
  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);
  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));
  int test_packet_found = 0;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_for_testing())
      continue;
    EXPECT_EQ(packet.for_testing().str(), "event written after OnStop");
    test_packet_found++;
  }
  EXPECT_EQ(test_packet_found, 1);
}

TEST_F(PerfettoApiTest, RepeatedStartAndStop) {
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");

  for (int i = 0; i < 5; i++) {
    auto* tracing_session = NewTrace(cfg);
    tracing_session->get()->Start();
    std::atomic<bool> stop_called{false};
    tracing_session->get()->SetOnStopCallback(
        [&stop_called] { stop_called = true; });
    tracing_session->get()->StopBlocking();
    EXPECT_TRUE(stop_called);
  }
}

TEST_F(PerfettoApiTest, SetupWithFile) {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  char temp_file[] = "/data/local/tmp/perfetto-XXXXXXXX";
#else
  char temp_file[] = "/tmp/perfetto-XXXXXXXX";
#endif
  int fd = mkstemp(temp_file);
  ASSERT_TRUE(fd >= 0);

  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");
  // Write a trace into |fd|.
  auto* tracing_session = NewTrace(cfg, fd);
  tracing_session->get()->StartBlocking();
  tracing_session->get()->StopBlocking();
  // Check that |fd| didn't get closed.
  EXPECT_EQ(0, fcntl(fd, F_GETFD, 0));
  // Check that the trace got written.
  EXPECT_GT(lseek(fd, 0, SEEK_END), 0);
  EXPECT_EQ(0, close(fd));
  // Clean up.
  EXPECT_EQ(0, unlink(temp_file));
}

TEST_F(PerfettoApiTest, MultipleRegistrations) {
  // Attempt to register the same data source again.
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("my_data_source");
  EXPECT_TRUE(MockDataSource::Register(dsd));

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // Emit one trace event.
  std::atomic<int> trace_lambda_calls{0};
  MockDataSource::Trace([&trace_lambda_calls](MockDataSource::TraceContext) {
    trace_lambda_calls++;
  });

  // Make sure the data source got called only once.
  tracing_session->get()->StopBlocking();
  EXPECT_EQ(trace_lambda_calls, 1);
}

TEST_F(PerfettoApiTest, CustomIncrementalState) {
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("incr_data_source");
  TestIncrementalDataSource::Register(dsd);

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(500);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("incr_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();

  // First emit a no-op trace event that initializes the incremental state as a
  // side effect.
  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext) {});
  EXPECT_TRUE(TestIncrementalState::constructed);

  // Check that the incremental state is carried across trace events.
  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext ctx) {
        auto* state = ctx.GetIncrementalState();
        EXPECT_TRUE(state);
        EXPECT_EQ(100, state->count);
        state->count++;
      });

  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext ctx) {
        auto* state = ctx.GetIncrementalState();
        EXPECT_EQ(101, state->count);
      });

  // Make sure the incremental state gets cleaned up between sessions.
  tracing_session->get()->StopBlocking();
  tracing_session = NewTrace(cfg);
  tracing_session->get()->StartBlocking();
  TestIncrementalDataSource::Trace(
      [](TestIncrementalDataSource::TraceContext ctx) {
        auto* state = ctx.GetIncrementalState();
        EXPECT_TRUE(TestIncrementalState::destroyed);
        EXPECT_TRUE(state);
        EXPECT_EQ(100, state->count);
      });
  tracing_session->get()->StopBlocking();
}

// Regression test for b/139110180. Checks that GetDataSourceLocked() can be
// called from OnStart() and OnStop() callbacks without deadlocking.
TEST_F(PerfettoApiTest, GetDataSourceLockedFromCallbacks) {
  auto* data_source = &data_sources_["my_data_source"];

  // Setup the trace config.
  perfetto::TraceConfig cfg;
  cfg.set_duration_ms(1);
  cfg.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  ds_cfg->set_name("my_data_source");

  // Create a new trace session.
  auto* tracing_session = NewTrace(cfg);

  data_source->on_start_callback = [] {
    MockDataSource::Trace([](MockDataSource::TraceContext ctx) {
      ctx.NewTracePacket()->set_for_testing()->set_str("on-start");
      auto ds = ctx.GetDataSourceLocked();
      ASSERT_TRUE(!!ds);
      ctx.NewTracePacket()->set_for_testing()->set_str("on-start-locked");
    });
  };

  data_source->on_stop_callback = [] {
    MockDataSource::Trace([](MockDataSource::TraceContext ctx) {
      ctx.NewTracePacket()->set_for_testing()->set_str("on-stop");
      auto ds = ctx.GetDataSourceLocked();
      ASSERT_TRUE(!!ds);
      ctx.NewTracePacket()->set_for_testing()->set_str("on-stop-locked");
      ctx.Flush();
    });
  };

  tracing_session->get()->Start();
  data_source->on_stop.Wait();
  tracing_session->on_stop.Wait();

  std::vector<char> raw_trace = tracing_session->get()->ReadTraceBlocking();
  ASSERT_GE(raw_trace.size(), 0u);

  perfetto::protos::gen::Trace trace;
  ASSERT_TRUE(trace.ParseFromArray(raw_trace.data(), raw_trace.size()));
  int packets_found = 0;
  for (const auto& packet : trace.packet()) {
    if (!packet.has_for_testing())
      continue;
    packets_found |= packet.for_testing().str() == "on-start" ? 1 : 0;
    packets_found |= packet.for_testing().str() == "on-start-locked" ? 2 : 0;
    packets_found |= packet.for_testing().str() == "on-stop" ? 4 : 0;
    packets_found |= packet.for_testing().str() == "on-stop-locked" ? 8 : 0;
  }
  EXPECT_EQ(packets_found, 1 | 2 | 4 | 8);
}

TEST_F(PerfettoApiTest, LegacyTraceEvents) {
  // Create a new trace session.
  auto* tracing_session =
      NewTraceWithCategories({"cat", TRACE_DISABLED_BY_DEFAULT("cat")});
  tracing_session->get()->StartBlocking();

  // Basic events.
  TRACE_EVENT_INSTANT0("cat", "LegacyEvent", TRACE_EVENT_SCOPE_GLOBAL);
  TRACE_EVENT_BEGIN1("cat", "LegacyEvent", "arg", 123);
  TRACE_EVENT_END2("cat", "LegacyEvent", "arg", "string", "arg2", 0.123f);

  // Scoped event.
  { TRACE_EVENT0("cat", "ScopedLegacyEvent"); }

  // Event with flow (and disabled category).
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("cat"), "LegacyFlowEvent",
                         0xdadacafe, TRACE_EVENT_FLAG_FLOW_IN);

  // Event with timestamp.
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0("cat", "LegacyInstantEvent",
                                      TRACE_EVENT_SCOPE_GLOBAL,
                                      MyTimestamp{123456789ul});

  // Event with id, thread id and timestamp (and dynamic name).
  TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      "cat", std::string("LegacyWithIdTidAndTimestamp").c_str(), 1,
      MyThreadId(123, 456), MyTimestamp{3});

  // Event with id.
  TRACE_COUNTER1("cat", "LegacyCounter", 1234);
  TRACE_COUNTER_ID1("cat", "LegacyCounterWithId", 1234, 9000);

  // Metadata event.
  TRACE_EVENT_METADATA1("cat", "LegacyMetadata", "obsolete", true);

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(
      slices,
      ElementsAre(
          "I:cat.LegacyEvent", "B:cat.LegacyEvent(arg=(int)123)",
          "E.LegacyEvent(arg=(string)string,arg2=(double)0.123)",
          "B:cat.ScopedLegacyEvent", "E",
          "B(bind_id=3671771902)(flow_direction=1):disabled-by-default-cat."
          "LegacyFlowEvent",
          "I:cat.LegacyInstantEvent",
          "Legacy_S(unscoped_id=1)(pid_override=123)(tid_override=456):cat."
          "LegacyWithIdTidAndTimestamp",
          "Legacy_C:cat.LegacyCounter(value=(int)1234)",
          "Legacy_C(unscoped_id=1234):cat.LegacyCounterWithId(value=(int)9000)",
          "Legacy_M:cat.LegacyMetadata"));
}

TEST_F(PerfettoApiTest, LegacyTraceEventsWithCustomAnnotation) {
  // Create a new trace session.
  auto* tracing_session = NewTraceWithCategories({"cat"});
  tracing_session->get()->StartBlocking();

  MyDebugAnnotation annotation;
  TRACE_EVENT_BEGIN1("cat", "LegacyEvent", "arg", annotation);

  std::unique_ptr<MyDebugAnnotation> owned_annotation(new MyDebugAnnotation());
  TRACE_EVENT_BEGIN1("cat", "LegacyEvent", "arg", std::move(owned_annotation));

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(slices,
              ElementsAre("B:cat.LegacyEvent(arg=(json){\"key\": 123})",
                          "B:cat.LegacyEvent(arg=(json){\"key\": 123})"));
}

TEST_F(PerfettoApiTest, LegacyTraceEventsWithConcurrentSessions) {
  // Make sure that a uniquely owned debug annotation can be written into
  // multiple concurrent tracing sessions.

  auto* tracing_session = NewTraceWithCategories({"cat"});
  tracing_session->get()->StartBlocking();

  auto* tracing_session2 = NewTraceWithCategories({"cat"});
  tracing_session2->get()->StartBlocking();

  std::unique_ptr<MyDebugAnnotation> owned_annotation(new MyDebugAnnotation());
  TRACE_EVENT_BEGIN1("cat", "LegacyEvent", "arg", std::move(owned_annotation));

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(slices,
              ElementsAre("B:cat.LegacyEvent(arg=(json){\"key\": 123})"));

  tracing_session2->get()->StopBlocking();
  slices = ReadSlicesFromTrace(tracing_session2->get());
  EXPECT_THAT(slices,
              ElementsAre("B:cat.LegacyEvent(arg=(json){\"key\": 123})"));
}

TEST_F(PerfettoApiTest, LegacyTraceEventsWithId) {
  auto* tracing_session = NewTraceWithCategories({"cat"});
  tracing_session->get()->StartBlocking();

  TRACE_EVENT_ASYNC_BEGIN0("cat", "UnscopedId", 0x1000);
  TRACE_EVENT_ASYNC_BEGIN0("cat", "LocalId", TRACE_ID_LOCAL(0x2000));
  TRACE_EVENT_ASYNC_BEGIN0("cat", "GlobalId", TRACE_ID_GLOBAL(0x3000));
  TRACE_EVENT_ASYNC_BEGIN0(
      "cat", "WithScope",
      TRACE_ID_WITH_SCOPE("scope string", TRACE_ID_GLOBAL(0x4000)));

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(slices, ElementsAre("Legacy_S(unscoped_id=4096):cat.UnscopedId",
                                  "Legacy_S(local_id=8192):cat.LocalId",
                                  "Legacy_S(global_id=12288):cat.GlobalId",
                                  "Legacy_S(global_id=16384)(id_scope=\"scope "
                                  "string\"):cat.WithScope"));
}

TEST_F(PerfettoApiTest, LegacyTraceEventsWithFlow) {
  auto* tracing_session = NewTraceWithCategories({"cat"});
  tracing_session->get()->StartBlocking();

  const uint64_t flow_id = 1234;
  {
    TRACE_EVENT_WITH_FLOW1("cat", "LatencyInfo.Flow", TRACE_ID_GLOBAL(flow_id),
                           TRACE_EVENT_FLAG_FLOW_OUT, "step", "Begin");
  }

  {
    TRACE_EVENT_WITH_FLOW2("cat", "LatencyInfo.Flow", TRACE_ID_GLOBAL(flow_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "Middle", "value", false);
  }

  {
    TRACE_EVENT_WITH_FLOW1("cat", "LatencyInfo.Flow", TRACE_ID_GLOBAL(flow_id),
                           TRACE_EVENT_FLAG_FLOW_IN, "step", "End");
  }

  perfetto::TrackEvent::Flush();
  tracing_session->get()->StopBlocking();
  auto slices = ReadSlicesFromTrace(tracing_session->get());
  EXPECT_THAT(slices,
              ElementsAre("B(bind_id=1234)(flow_direction=2):cat.LatencyInfo."
                          "Flow(step=(string)Begin)",
                          "E",
                          "B(bind_id=1234)(flow_direction=3):cat.LatencyInfo."
                          "Flow(step=(string)Middle,value=(bool)0)",
                          "E",
                          "B(bind_id=1234)(flow_direction=1):cat.LatencyInfo."
                          "Flow(step=(string)End)",
                          "E"));
}

TEST_F(PerfettoApiTest, LegacyCategoryGroupEnabledState) {
  bool foo_status;
  bool bar_status;
  bool dynamic_status;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("foo", &foo_status);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("bar", &bar_status);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("dynamic", &dynamic_status);
  EXPECT_FALSE(foo_status);
  EXPECT_FALSE(bar_status);
  EXPECT_FALSE(dynamic_status);

  const uint8_t* foo_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("foo");
  const uint8_t* bar_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("bar");
  EXPECT_FALSE(*foo_enabled);
  EXPECT_FALSE(*bar_enabled);

  auto* tracing_session = NewTraceWithCategories({"foo", "dynamic"});
  tracing_session->get()->StartBlocking();
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("foo", &foo_status);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("bar", &bar_status);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("dynamic", &dynamic_status);
  EXPECT_TRUE(foo_status);
  EXPECT_FALSE(bar_status);
  EXPECT_TRUE(dynamic_status);

  EXPECT_TRUE(*foo_enabled);
  EXPECT_FALSE(*bar_enabled);

  tracing_session->get()->StopBlocking();
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("foo", &foo_status);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("bar", &bar_status);
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("dynamic", &dynamic_status);
  EXPECT_FALSE(foo_status);
  EXPECT_FALSE(bar_status);
  EXPECT_FALSE(dynamic_status);
  EXPECT_FALSE(*foo_enabled);
  EXPECT_FALSE(*bar_enabled);
}

TEST_F(PerfettoApiTest, CategoryEnabledState) {
  perfetto::DynamicCategory dynamic{"dynamic"};
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("foo"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("bar"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("red,green,blue,foo"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("dynamic"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("dynamic_2"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED(dynamic));

  auto* tracing_session = NewTraceWithCategories({"foo", "dynamic"});
  tracing_session->get()->StartBlocking();
  EXPECT_TRUE(TRACE_EVENT_CATEGORY_ENABLED("foo"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("bar"));
  EXPECT_TRUE(TRACE_EVENT_CATEGORY_ENABLED("red,green,blue,foo"));
  EXPECT_TRUE(TRACE_EVENT_CATEGORY_ENABLED("dynamic"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("dynamic_2"));
  EXPECT_TRUE(TRACE_EVENT_CATEGORY_ENABLED(dynamic));

  tracing_session->get()->StopBlocking();
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("foo"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("bar"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("red,green,blue,foo"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("dynamic"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED("dynamic_2"));
  EXPECT_FALSE(TRACE_EVENT_CATEGORY_ENABLED(dynamic));
}

}  // namespace

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MockDataSource);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(MockDataSource2);
PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(TestIncrementalDataSource,
                                           TestIncrementalDataSourceTraits);
