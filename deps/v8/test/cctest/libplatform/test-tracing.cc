// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <limits>

#include "include/libplatform/v8-tracing.h"
#include "src/base/platform/platform.h"
#include "src/libplatform/default-platform.h"
#include "src/tracing/trace-event.h"
#include "test/cctest/cctest.h"

#ifdef V8_USE_PERFETTO
#include "perfetto/tracing.h"
#include "protos/perfetto/trace/trace.pb.h"
#include "src/libplatform/tracing/trace-event-listener.h"
#include "src/tracing/traced-value.h"
#endif  // V8_USE_PERFETTO

namespace v8 {
namespace platform {
namespace tracing {

TEST(TestTraceConfig) {
  LocalContext env;
  TraceConfig* trace_config = new TraceConfig();
  trace_config->AddIncludedCategory("v8");
  trace_config->AddIncludedCategory(TRACE_DISABLED_BY_DEFAULT("v8.runtime"));

  CHECK_EQ(trace_config->IsSystraceEnabled(), false);
  CHECK_EQ(trace_config->IsArgumentFilterEnabled(), false);
  CHECK_EQ(trace_config->IsCategoryGroupEnabled("v8"), true);
  CHECK_EQ(trace_config->IsCategoryGroupEnabled("v8.cpu_profile"), false);
  CHECK_EQ(trace_config->IsCategoryGroupEnabled(
               TRACE_DISABLED_BY_DEFAULT("v8.runtime")),
           true);
  CHECK_EQ(trace_config->IsCategoryGroupEnabled("v8,v8.cpu_profile"), true);
  CHECK_EQ(
      trace_config->IsCategoryGroupEnabled("v8,disabled-by-default-v8.runtime"),
      true);
  CHECK_EQ(trace_config->IsCategoryGroupEnabled("v8_cpu_profile"), false);

  delete trace_config;
}

// Perfetto doesn't use TraceObject.
#if !defined(V8_USE_PERFETTO)
TEST(TestTraceObject) {
  TraceObject trace_object;
  uint8_t category_enabled_flag = 41;
  trace_object.Initialize('X', &category_enabled_flag, "Test.Trace",
                          "Test.Scope", 42, 123, 0, nullptr, nullptr, nullptr,
                          nullptr, 0, 1729, 4104);
  CHECK_EQ('X', trace_object.phase());
  CHECK_EQ(category_enabled_flag, *trace_object.category_enabled_flag());
  CHECK_EQ(std::string("Test.Trace"), std::string(trace_object.name()));
  CHECK_EQ(std::string("Test.Scope"), std::string(trace_object.scope()));
  CHECK_EQ(0u, trace_object.duration());
  CHECK_EQ(0u, trace_object.cpu_duration());
}

class ConvertableToTraceFormatMock : public v8::ConvertableToTraceFormat {
 public:
  explicit ConvertableToTraceFormatMock(int value) : value_(value) {}
  void AppendAsTraceFormat(std::string* out) const override {
    *out += "[" + std::to_string(value_) + "," + std::to_string(value_) + "]";
  }

 private:
  int value_;

  DISALLOW_COPY_AND_ASSIGN(ConvertableToTraceFormatMock);
};

class MockTraceWriter : public TraceWriter {
 public:
  void AppendTraceEvent(TraceObject* trace_event) override {
    // TraceObject might not have been initialized.
    const char* name = trace_event->name() ? trace_event->name() : "";
    events_.push_back(name);
  }

  void Flush() override {}

  std::vector<std::string> events() { return events_; }

 private:
  std::vector<std::string> events_;
};
#endif  // !defined(V8_USE_PERFETTO)

// Perfetto doesn't use the ring buffer.
#if !defined(V8_USE_PERFETTO)
TEST(TestTraceBufferRingBuffer) {
  // We should be able to add kChunkSize * 2 + 1 trace events.
  const int HANDLES_COUNT = TraceBufferChunk::kChunkSize * 2 + 1;
  MockTraceWriter* writer = new MockTraceWriter();
  TraceBuffer* ring_buffer =
      TraceBuffer::CreateTraceBufferRingBuffer(2, writer);
  std::string names[HANDLES_COUNT];
  for (int i = 0; i < HANDLES_COUNT; ++i) {
    names[i] = "Test.EventNo" + std::to_string(i);
  }

  std::vector<uint64_t> handles(HANDLES_COUNT);
  uint8_t category_enabled_flag = 41;
  for (size_t i = 0; i < handles.size(); ++i) {
    TraceObject* trace_object = ring_buffer->AddTraceEvent(&handles[i]);
    CHECK_NOT_NULL(trace_object);
    trace_object->Initialize('X', &category_enabled_flag, names[i].c_str(),
                             "Test.Scope", 42, 123, 0, nullptr, nullptr,
                             nullptr, nullptr, 0, 1729, 4104);
    trace_object = ring_buffer->GetEventByHandle(handles[i]);
    CHECK_NOT_NULL(trace_object);
    CHECK_EQ('X', trace_object->phase());
    CHECK_EQ(names[i], std::string(trace_object->name()));
    CHECK_EQ(category_enabled_flag, *trace_object->category_enabled_flag());
  }

  // We should only be able to retrieve the last kChunkSize + 1.
  for (size_t i = 0; i < TraceBufferChunk::kChunkSize; ++i) {
    CHECK_NULL(ring_buffer->GetEventByHandle(handles[i]));
  }

  for (size_t i = TraceBufferChunk::kChunkSize; i < handles.size(); ++i) {
    TraceObject* trace_object = ring_buffer->GetEventByHandle(handles[i]);
    CHECK_NOT_NULL(trace_object);
    // The object properties should be correct.
    CHECK_EQ('X', trace_object->phase());
    CHECK_EQ(names[i], std::string(trace_object->name()));
    CHECK_EQ(category_enabled_flag, *trace_object->category_enabled_flag());
  }

  // Check Flush(), that the writer wrote the last kChunkSize  1 event names.
  ring_buffer->Flush();
  auto events = writer->events();
  CHECK_EQ(TraceBufferChunk::kChunkSize + 1, events.size());
  for (size_t i = TraceBufferChunk::kChunkSize; i < handles.size(); ++i) {
    CHECK_EQ(names[i], events[i - TraceBufferChunk::kChunkSize]);
  }
  delete ring_buffer;
}
#endif  // !defined(V8_USE_PERFETTO)

// Perfetto has an internal JSON exporter.
#if !defined(V8_USE_PERFETTO)
void PopulateJSONWriter(TraceWriter* writer) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  std::unique_ptr<v8::Platform> default_platform(
      v8::platform::NewDefaultPlatform());
  i::V8::SetPlatformForTesting(default_platform.get());
  auto tracing = std::make_unique<v8::platform::tracing::TracingController>();
  v8::platform::tracing::TracingController* tracing_controller = tracing.get();
  static_cast<v8::platform::DefaultPlatform*>(default_platform.get())
      ->SetTracingController(std::move(tracing));

  TraceBuffer* ring_buffer =
      TraceBuffer::CreateTraceBufferRingBuffer(1, writer);
  tracing_controller->Initialize(ring_buffer);
  TraceConfig* trace_config = new TraceConfig();
  trace_config->AddIncludedCategory("v8-cat");
  tracing_controller->StartTracing(trace_config);

  TraceObject trace_object;
  trace_object.InitializeForTesting(
      'X', tracing_controller->GetCategoryGroupEnabled("v8-cat"), "Test0",
      v8::internal::tracing::kGlobalScope, 42, 0x1234, 0, nullptr, nullptr,
      nullptr, nullptr, TRACE_EVENT_FLAG_HAS_ID, 11, 22, 100, 50, 33, 44);
  writer->AppendTraceEvent(&trace_object);
  trace_object.InitializeForTesting(
      'Y', tracing_controller->GetCategoryGroupEnabled("v8-cat"), "Test1",
      v8::internal::tracing::kGlobalScope, 43, 0x5678, 0, nullptr, nullptr,
      nullptr, nullptr, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      55, 66, 110, 55, 77, 88);
  writer->AppendTraceEvent(&trace_object);
  tracing_controller->StopTracing();
  i::V8::SetPlatformForTesting(old_platform);
}

TEST(TestJSONTraceWriter) {
  std::ostringstream stream;
  TraceWriter* writer = TraceWriter::CreateJSONTraceWriter(stream);
  PopulateJSONWriter(writer);
  std::string trace_str = stream.str();
  std::string expected_trace_str =
      "{\"traceEvents\":[{\"pid\":11,\"tid\":22,\"ts\":100,\"tts\":50,"
      "\"ph\":\"X\",\"cat\":\"v8-cat\",\"name\":\"Test0\",\"dur\":33,"
      "\"tdur\":44,\"id\":\"0x2a\",\"args\":{}},{\"pid\":55,\"tid\":66,"
      "\"ts\":110,\"tts\":55,\"ph\":\"Y\",\"cat\":\"v8-cat\",\"name\":"
      "\"Test1\",\"dur\":77,\"tdur\":88,\"bind_id\":\"0x5678\","
      "\"flow_in\":true,\"flow_out\":true,\"args\":{}}]}";

  CHECK_EQ(expected_trace_str, trace_str);
}

TEST(TestJSONTraceWriterWithCustomtag) {
  std::ostringstream stream;
  TraceWriter* writer = TraceWriter::CreateJSONTraceWriter(stream, "customTag");
  PopulateJSONWriter(writer);
  std::string trace_str = stream.str();
  std::string expected_trace_str =
      "{\"customTag\":[{\"pid\":11,\"tid\":22,\"ts\":100,\"tts\":50,"
      "\"ph\":\"X\",\"cat\":\"v8-cat\",\"name\":\"Test0\",\"dur\":33,"
      "\"tdur\":44,\"id\":\"0x2a\",\"args\":{}},{\"pid\":55,\"tid\":66,"
      "\"ts\":110,\"tts\":55,\"ph\":\"Y\",\"cat\":\"v8-cat\",\"name\":"
      "\"Test1\",\"dur\":77,\"tdur\":88,\"bind_id\":\"0x5678\","
      "\"flow_in\":true,\"flow_out\":true,\"args\":{}}]}";

  CHECK_EQ(expected_trace_str, trace_str);
}
#endif  // !defined(V8_USE_PERFETTO)

void GetJSONStrings(std::vector<std::string>* ret, const std::string& str,
                    const std::string& param, const std::string& start_delim,
                    const std::string& end_delim) {
  size_t pos = str.find(param);
  while (pos != std::string::npos) {
    size_t start_pos = str.find(start_delim, pos + param.length());
    size_t end_pos = str.find(end_delim, start_pos + 1);
    CHECK_NE(start_pos, std::string::npos);
    CHECK_NE(end_pos, std::string::npos);
    ret->push_back(str.substr(start_pos + 1, end_pos - start_pos - 1));
    pos = str.find(param, pos + 1);
  }
}

// With Perfetto the tracing controller doesn't observe events.
#if !defined(V8_USE_PERFETTO)
TEST(TestTracingController) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  std::unique_ptr<v8::Platform> default_platform(
      v8::platform::NewDefaultPlatform());
  i::V8::SetPlatformForTesting(default_platform.get());

  auto tracing = std::make_unique<v8::platform::tracing::TracingController>();
  v8::platform::tracing::TracingController* tracing_controller = tracing.get();
  static_cast<v8::platform::DefaultPlatform*>(default_platform.get())
      ->SetTracingController(std::move(tracing));

  MockTraceWriter* writer = new MockTraceWriter();
  TraceBuffer* ring_buffer =
      TraceBuffer::CreateTraceBufferRingBuffer(1, writer);
  tracing_controller->Initialize(ring_buffer);
  TraceConfig* trace_config = new TraceConfig();
  trace_config->AddIncludedCategory("v8");
  tracing_controller->StartTracing(trace_config);

  TRACE_EVENT0("v8", "v8.Test");
  // cat category is not included in default config
  TRACE_EVENT0("cat", "v8.Test2");
  TRACE_EVENT0("v8", "v8.Test3");
  tracing_controller->StopTracing();

  CHECK_EQ(2u, writer->events().size());
  CHECK_EQ(std::string("v8.Test"), writer->events()[0]);
  CHECK_EQ(std::string("v8.Test3"), writer->events()[1]);

  i::V8::SetPlatformForTesting(old_platform);
}

TEST(TestTracingControllerMultipleArgsAndCopy) {
  std::ostringstream stream, perfetto_stream;
  uint64_t aa = 11;
  unsigned int bb = 22;
  uint16_t cc = 33;
  unsigned char dd = 44;
  int64_t ee = -55;
  int ff = -66;
  int16_t gg = -77;
  signed char hh = -88;
  bool ii1 = true;
  bool ii2 = false;
  double jj1 = 99.0;
  double jj2 = 1e100;
  double jj3 = std::numeric_limits<double>::quiet_NaN();
  double jj4 = std::numeric_limits<double>::infinity();
  double jj5 = -std::numeric_limits<double>::infinity();
  void* kk = &aa;
  const char* ll = "100";
  std::string mm = "INIT";
  std::string mmm = "\"INIT\"";

  // Create a scope for the tracing controller to terminate the trace writer.
  {
    v8::Platform* old_platform = i::V8::GetCurrentPlatform();
    std::unique_ptr<v8::Platform> default_platform(
        v8::platform::NewDefaultPlatform());
    i::V8::SetPlatformForTesting(default_platform.get());

    auto tracing = std::make_unique<v8::platform::tracing::TracingController>();
    v8::platform::tracing::TracingController* tracing_controller =
        tracing.get();
    static_cast<v8::platform::DefaultPlatform*>(default_platform.get())
        ->SetTracingController(std::move(tracing));
    TraceWriter* writer = TraceWriter::CreateJSONTraceWriter(stream);

    TraceBuffer* ring_buffer =
        TraceBuffer::CreateTraceBufferRingBuffer(1, writer);
    tracing_controller->Initialize(ring_buffer);
    TraceConfig* trace_config = new TraceConfig();
    trace_config->AddIncludedCategory("v8");
    tracing_controller->StartTracing(trace_config);

    {
      TRACE_EVENT1("v8", "v8.Test.aa", "aa", aa);
      TRACE_EVENT1("v8", "v8.Test.bb", "bb", bb);
      TRACE_EVENT1("v8", "v8.Test.cc", "cc", cc);
      TRACE_EVENT1("v8", "v8.Test.dd", "dd", dd);
      TRACE_EVENT1("v8", "v8.Test.ee", "ee", ee);
      TRACE_EVENT1("v8", "v8.Test.ff", "ff", ff);
      TRACE_EVENT1("v8", "v8.Test.gg", "gg", gg);
      TRACE_EVENT1("v8", "v8.Test.hh", "hh", hh);
      TRACE_EVENT1("v8", "v8.Test.ii", "ii1", ii1);
      TRACE_EVENT1("v8", "v8.Test.ii", "ii2", ii2);
      TRACE_EVENT1("v8", "v8.Test.jj1", "jj1", jj1);
      TRACE_EVENT1("v8", "v8.Test.jj2", "jj2", jj2);
      TRACE_EVENT1("v8", "v8.Test.jj3", "jj3", jj3);
      TRACE_EVENT1("v8", "v8.Test.jj4", "jj4", jj4);
      TRACE_EVENT1("v8", "v8.Test.jj5", "jj5", jj5);
      TRACE_EVENT1("v8", "v8.Test.kk", "kk", kk);
      TRACE_EVENT1("v8", "v8.Test.ll", "ll", ll);
      TRACE_EVENT1("v8", "v8.Test.mm", "mm", TRACE_STR_COPY(mmm.c_str()));

      TRACE_EVENT2("v8", "v8.Test2.1", "aa", aa, "ll", ll);
      TRACE_EVENT2("v8", "v8.Test2.2", "mm1", TRACE_STR_COPY(mm.c_str()), "mm2",
                   TRACE_STR_COPY(mmm.c_str()));

      // Check copies are correct.
      TRACE_EVENT_COPY_INSTANT0("v8", mm.c_str(), TRACE_EVENT_SCOPE_THREAD);
      TRACE_EVENT_COPY_INSTANT2("v8", mm.c_str(), TRACE_EVENT_SCOPE_THREAD,
                                "mm1", mm.c_str(), "mm2", mmm.c_str());
      mm = "CHANGED";
      mmm = "CHANGED";

      TRACE_EVENT_INSTANT1("v8", "v8.Test", TRACE_EVENT_SCOPE_THREAD, "a1",
                           new ConvertableToTraceFormatMock(42));
      std::unique_ptr<ConvertableToTraceFormatMock> trace_event_arg(
          new ConvertableToTraceFormatMock(42));
      TRACE_EVENT_INSTANT2("v8", "v8.Test", TRACE_EVENT_SCOPE_THREAD, "a1",
                           std::move(trace_event_arg), "a2",
                           new ConvertableToTraceFormatMock(123));
    }

    tracing_controller->StopTracing();

    i::V8::SetPlatformForTesting(old_platform);
  }

  std::string trace_str = stream.str();

  std::vector<std::string> all_args, all_names, all_cats;
  GetJSONStrings(&all_args, trace_str, "\"args\"", "{", "}");
  GetJSONStrings(&all_names, trace_str, "\"name\"", "\"", "\"");
  GetJSONStrings(&all_cats, trace_str, "\"cat\"", "\"", "\"");

  CHECK_EQ(all_args.size(), 24u);
  CHECK_EQ(all_args[0], "\"aa\":11");
  CHECK_EQ(all_args[1], "\"bb\":22");
  CHECK_EQ(all_args[2], "\"cc\":33");
  CHECK_EQ(all_args[3], "\"dd\":44");
  CHECK_EQ(all_args[4], "\"ee\":-55");
  CHECK_EQ(all_args[5], "\"ff\":-66");
  CHECK_EQ(all_args[6], "\"gg\":-77");
  CHECK_EQ(all_args[7], "\"hh\":-88");
  CHECK_EQ(all_args[8], "\"ii1\":true");
  CHECK_EQ(all_args[9], "\"ii2\":false");
  CHECK_EQ(all_args[10], "\"jj1\":99.0");
  CHECK_EQ(all_args[11], "\"jj2\":1e+100");
  CHECK_EQ(all_args[12], "\"jj3\":\"NaN\"");
  CHECK_EQ(all_args[13], "\"jj4\":\"Infinity\"");
  CHECK_EQ(all_args[14], "\"jj5\":\"-Infinity\"");
  std::ostringstream pointer_stream;
  pointer_stream << "\"kk\":\"" << &aa << "\"";
  CHECK_EQ(all_args[15], pointer_stream.str());
  CHECK_EQ(all_args[16], "\"ll\":\"100\"");
  CHECK_EQ(all_args[17], "\"mm\":\"\\\"INIT\\\"\"");

  CHECK_EQ(all_names[18], "v8.Test2.1");
  CHECK_EQ(all_args[18], "\"aa\":11,\"ll\":\"100\"");
  CHECK_EQ(all_args[19], "\"mm1\":\"INIT\",\"mm2\":\"\\\"INIT\\\"\"");

  CHECK_EQ(all_names[20], "INIT");
  CHECK_EQ(all_names[21], "INIT");
  CHECK_EQ(all_args[21], "\"mm1\":\"INIT\",\"mm2\":\"\\\"INIT\\\"\"");
  CHECK_EQ(all_args[22], "\"a1\":[42,42]");
  CHECK_EQ(all_args[23], "\"a1\":[42,42],\"a2\":[123,123]");
}
#endif  // !defined(V8_USE_PERFETTO)

namespace {

class TraceStateObserverImpl : public TracingController::TraceStateObserver {
 public:
  void OnTraceEnabled() override { ++enabled_count; }
  void OnTraceDisabled() override { ++disabled_count; }

  int enabled_count = 0;
  int disabled_count = 0;
};

}  // namespace

TEST(TracingObservers) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  std::unique_ptr<v8::Platform> default_platform(
      v8::platform::NewDefaultPlatform());
  i::V8::SetPlatformForTesting(default_platform.get());

  auto tracing = std::make_unique<v8::platform::tracing::TracingController>();
  v8::platform::tracing::TracingController* tracing_controller = tracing.get();
  static_cast<v8::platform::DefaultPlatform*>(default_platform.get())
      ->SetTracingController(std::move(tracing));
#ifdef V8_USE_PERFETTO
  std::ostringstream sstream;
  tracing_controller->InitializeForPerfetto(&sstream);
#else
  MockTraceWriter* writer = new MockTraceWriter();
  v8::platform::tracing::TraceBuffer* ring_buffer =
      v8::platform::tracing::TraceBuffer::CreateTraceBufferRingBuffer(1,
                                                                      writer);
  tracing_controller->Initialize(ring_buffer);
#endif
  v8::platform::tracing::TraceConfig* trace_config =
      new v8::platform::tracing::TraceConfig();
  trace_config->AddIncludedCategory("v8");

  TraceStateObserverImpl observer;
  tracing_controller->AddTraceStateObserver(&observer);

  CHECK_EQ(0, observer.enabled_count);
  CHECK_EQ(0, observer.disabled_count);

  tracing_controller->StartTracing(trace_config);

  CHECK_EQ(1, observer.enabled_count);
  CHECK_EQ(0, observer.disabled_count);

  TraceStateObserverImpl observer2;
  tracing_controller->AddTraceStateObserver(&observer2);

  CHECK_EQ(1, observer2.enabled_count);
  CHECK_EQ(0, observer2.disabled_count);

  tracing_controller->RemoveTraceStateObserver(&observer2);

  CHECK_EQ(1, observer2.enabled_count);
  CHECK_EQ(0, observer2.disabled_count);

  tracing_controller->StopTracing();

  CHECK_EQ(1, observer.enabled_count);
  CHECK_EQ(1, observer.disabled_count);
  CHECK_EQ(1, observer2.enabled_count);
  CHECK_EQ(0, observer2.disabled_count);

  tracing_controller->RemoveTraceStateObserver(&observer);

  CHECK_EQ(1, observer.enabled_count);
  CHECK_EQ(1, observer.disabled_count);

  trace_config = new v8::platform::tracing::TraceConfig();
  tracing_controller->StartTracing(trace_config);
  tracing_controller->StopTracing();

  CHECK_EQ(1, observer.enabled_count);
  CHECK_EQ(1, observer.disabled_count);

  i::V8::SetPlatformForTesting(old_platform);
}

// With Perfetto the tracing controller doesn't observe events.
#if !defined(V8_USE_PERFETTO)
class TraceWritingThread : public base::Thread {
 public:
  TraceWritingThread(
      v8::platform::tracing::TracingController* tracing_controller)
      : base::Thread(base::Thread::Options("TraceWritingThread")),
        tracing_controller_(tracing_controller) {}

  void Run() override {
    running_.store(true);
    while (running_.load()) {
      TRACE_EVENT0("v8", "v8.Test");
      tracing_controller_->AddTraceEvent('A', nullptr, "v8", "", 1, 1, 0,
                                         nullptr, nullptr, nullptr, nullptr, 0);
      tracing_controller_->AddTraceEventWithTimestamp('A', nullptr, "v8", "", 1,
                                                      1, 0, nullptr, nullptr,
                                                      nullptr, nullptr, 0, 0);
    }
  }

  void Stop() { running_.store(false); }

 private:
  std::atomic_bool running_{false};
  v8::platform::tracing::TracingController* tracing_controller_;
};

TEST(AddTraceEventMultiThreaded) {
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  std::unique_ptr<v8::Platform> default_platform(
      v8::platform::NewDefaultPlatform());
  i::V8::SetPlatformForTesting(default_platform.get());

  auto tracing = std::make_unique<v8::platform::tracing::TracingController>();
  v8::platform::tracing::TracingController* tracing_controller = tracing.get();
  static_cast<v8::platform::DefaultPlatform*>(default_platform.get())
      ->SetTracingController(std::move(tracing));

  MockTraceWriter* writer = new MockTraceWriter();
  TraceBuffer* ring_buffer =
      TraceBuffer::CreateTraceBufferRingBuffer(1, writer);
  tracing_controller->Initialize(ring_buffer);
  TraceConfig* trace_config = new TraceConfig();
  trace_config->AddIncludedCategory("v8");
  tracing_controller->StartTracing(trace_config);

  TraceWritingThread thread(tracing_controller);
  thread.StartSynchronously();
  TRACE_EVENT0("v8", "v8.Test2");
  TRACE_EVENT0("v8", "v8.Test2");

  base::OS::Sleep(base::TimeDelta::FromMilliseconds(10));
  tracing_controller->StopTracing();

  thread.Stop();
  thread.Join();

  i::V8::SetPlatformForTesting(old_platform);
}
#endif  // !defined(V8_USE_PERFETTO)

#ifdef V8_USE_PERFETTO

using TrackEvent = ::perfetto::protos::TrackEvent;

class TestListener : public TraceEventListener {
 public:
  void ProcessPacket(const ::perfetto::protos::TracePacket& packet) {
    if (packet.incremental_state_cleared()) {
      categories_.clear();
      event_names_.clear();
      debug_annotation_names_.clear();
    }

    if (!packet.has_track_event()) return;

    // Update incremental state.
    if (packet.has_interned_data()) {
      const auto& interned_data = packet.interned_data();
      for (const auto& it : interned_data.event_categories()) {
        CHECK_EQ(categories_.find(it.iid()), categories_.end());
        categories_[it.iid()] = it.name();
      }
      for (const auto& it : interned_data.event_names()) {
        CHECK_EQ(event_names_.find(it.iid()), event_names_.end());
        event_names_[it.iid()] = it.name();
      }
      for (const auto& it : interned_data.debug_annotation_names()) {
        CHECK_EQ(debug_annotation_names_.find(it.iid()),
                 debug_annotation_names_.end());
        debug_annotation_names_[it.iid()] = it.name();
      }
    }
    const auto& track_event = packet.track_event();
    std::string slice;
    switch (track_event.type()) {
      case perfetto::protos::TrackEvent::TYPE_SLICE_BEGIN:
        slice += "B";
        break;
      case perfetto::protos::TrackEvent::TYPE_SLICE_END:
        slice += "E";
        break;
      case perfetto::protos::TrackEvent::TYPE_INSTANT:
        slice += "I";
        break;
      default:
      case perfetto::protos::TrackEvent::TYPE_UNSPECIFIED:
        CHECK(false);
    }
    slice += ":" +
             (track_event.category_iids_size()
                  ? categories_[track_event.category_iids().Get(0)]
                  : "") +
             ".";
    if (track_event.name_iid()) {
      slice += event_names_[track_event.name_iid()];
    } else {
      slice += track_event.name();
    }

    if (track_event.debug_annotations_size()) {
      slice += "(";
      bool first_annotation = true;
      for (const auto& it : track_event.debug_annotations()) {
        if (!first_annotation) {
          slice += ",";
        }
        slice += debug_annotation_names_[it.name_iid()] + "=";
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
          value << "(pointer)0x" << std::hex << it.pointer_value();
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
    events_.push_back(slice);
  }

  const std::string& get_event(size_t index) { return events_.at(index); }

  size_t events_size() const { return events_.size(); }

 private:
  std::vector<std::string> events_;
  std::map<uint64_t, std::string> categories_;
  std::map<uint64_t, std::string> event_names_;
  std::map<uint64_t, std::string> debug_annotation_names_;
};

class TracingTestHarness {
 public:
  TracingTestHarness() {
    old_platform_ = i::V8::GetCurrentPlatform();
    default_platform_ = v8::platform::NewDefaultPlatform();
    i::V8::SetPlatformForTesting(default_platform_.get());

    auto tracing = std::make_unique<v8::platform::tracing::TracingController>();
    tracing_controller_ = tracing.get();
    static_cast<v8::platform::DefaultPlatform*>(default_platform_.get())
        ->SetTracingController(std::move(tracing));

    tracing_controller_->InitializeForPerfetto(&perfetto_json_stream_);
    tracing_controller_->SetTraceEventListenerForTesting(&listener_);
  }

  ~TracingTestHarness() { i::V8::SetPlatformForTesting(old_platform_); }

  void StartTracing() {
    TraceConfig* trace_config = new TraceConfig();
    trace_config->AddIncludedCategory("v8");
    tracing_controller_->StartTracing(trace_config);
  }

  void StopTracing() {
    v8::TrackEvent::Flush();
    tracing_controller_->StopTracing();
  }

  const std::string& get_event(size_t index) {
    return listener_.get_event(index);
  }
  size_t events_size() const { return listener_.events_size(); }

  std::string perfetto_json_stream() { return perfetto_json_stream_.str(); }

 private:
  std::unique_ptr<v8::Platform> default_platform_;
  v8::Platform* old_platform_;
  v8::platform::tracing::TracingController* tracing_controller_;
  TestListener listener_;
  std::ostringstream perfetto_json_stream_;
};

TEST(Perfetto) {
  TracingTestHarness harness;
  harness.StartTracing();

  uint64_t uint64_arg = 1024;
  const char* str_arg = "str_arg";

  {
    TRACE_EVENT0("v8", "test1");
    TRACE_EVENT1("v8", "test2", "arg1", uint64_arg);
    TRACE_EVENT2("v8", "test3", "arg1", uint64_arg, "arg2", str_arg);
  }

  harness.StopTracing();

  CHECK_EQ("B:v8.test1", harness.get_event(0));
  CHECK_EQ("B:v8.test2(arg1=(uint)1024)", harness.get_event(1));
  CHECK_EQ("B:v8.test3(arg1=(uint)1024,arg2=(string)str_arg)",
           harness.get_event(2));
  CHECK_EQ("E:.", harness.get_event(3));
  CHECK_EQ("E:.", harness.get_event(4));
  CHECK_EQ("E:.", harness.get_event(5));

  CHECK_EQ(6, harness.events_size());
}

// Replacement for 'TestTracingController'
TEST(Categories) {
  TracingTestHarness harness;
  harness.StartTracing();

  {
    TRACE_EVENT0("v8", "v8.Test");
    // cat category is not included in default config
    TRACE_EVENT0("cat", "v8.Test2");
    TRACE_EVENT0("v8", "v8.Test3");
  }

  harness.StopTracing();

  CHECK_EQ(4, harness.events_size());
  CHECK_EQ("B:v8.v8.Test", harness.get_event(0));
  CHECK_EQ("B:v8.v8.Test3", harness.get_event(1));
  CHECK_EQ("E:.", harness.get_event(2));
  CHECK_EQ("E:.", harness.get_event(3));
}

// Replacement for 'TestTracingControllerMultipleArgsAndCopy'
TEST(MultipleArgsAndCopy) {
  uint64_t aa = 11;
  unsigned int bb = 22;
  uint16_t cc = 33;
  unsigned char dd = 44;
  int64_t ee = -55;
  int ff = -66;
  int16_t gg = -77;
  signed char hh = -88;
  bool ii1 = true;
  bool ii2 = false;
  double jj1 = 99.0;
  double jj2 = 1e100;
  double jj3 = std::numeric_limits<double>::quiet_NaN();
  double jj4 = std::numeric_limits<double>::infinity();
  double jj5 = -std::numeric_limits<double>::infinity();
  void* kk = &aa;
  const char* ll = "100";
  std::string mm = "INIT";
  std::string mmm = "\"INIT\"";

  TracingTestHarness harness;
  harness.StartTracing();

  // Create a scope for the tracing controller to terminate the trace writer.
  {
    TRACE_EVENT1("v8", "v8.Test.aa", "aa", aa);
    TRACE_EVENT1("v8", "v8.Test.bb", "bb", bb);
    TRACE_EVENT1("v8", "v8.Test.cc", "cc", cc);
    TRACE_EVENT1("v8", "v8.Test.dd", "dd", dd);
    TRACE_EVENT1("v8", "v8.Test.ee", "ee", ee);
    TRACE_EVENT1("v8", "v8.Test.ff", "ff", ff);
    TRACE_EVENT1("v8", "v8.Test.gg", "gg", gg);
    TRACE_EVENT1("v8", "v8.Test.hh", "hh", hh);
    TRACE_EVENT1("v8", "v8.Test.ii", "ii1", ii1);
    TRACE_EVENT1("v8", "v8.Test.ii", "ii2", ii2);
    TRACE_EVENT1("v8", "v8.Test.jj1", "jj1", jj1);
    TRACE_EVENT1("v8", "v8.Test.jj2", "jj2", jj2);
    TRACE_EVENT1("v8", "v8.Test.jj3", "jj3", jj3);
    TRACE_EVENT1("v8", "v8.Test.jj4", "jj4", jj4);
    TRACE_EVENT1("v8", "v8.Test.jj5", "jj5", jj5);
    TRACE_EVENT1("v8", "v8.Test.kk", "kk", kk);
    TRACE_EVENT1("v8", "v8.Test.ll", "ll", ll);
    TRACE_EVENT1("v8", "v8.Test.mm", "mm", TRACE_STR_COPY(mmm.c_str()));

    TRACE_EVENT2("v8", "v8.Test2.1", "aa", aa, "ll", ll);
    TRACE_EVENT2("v8", "v8.Test2.2", "mm1", TRACE_STR_COPY(mm.c_str()), "mm2",
                 TRACE_STR_COPY(mmm.c_str()));

    // Check copies are correct.
    TRACE_EVENT_COPY_INSTANT0("v8", mm.c_str(), TRACE_EVENT_SCOPE_THREAD);
    TRACE_EVENT_COPY_INSTANT2("v8", mm.c_str(), TRACE_EVENT_SCOPE_THREAD, "mm1",
                              mm.c_str(), "mm2", mmm.c_str());
    mm = "CHANGED";
    mmm = "CHANGED";

    auto arg = v8::tracing::TracedValue::Create();
    arg->SetInteger("value", 42);
    TRACE_EVENT_INSTANT1("v8", "v8.Test", TRACE_EVENT_SCOPE_THREAD, "a1",
                         std::move(arg));

    arg = v8::tracing::TracedValue::Create();
    arg->SetString("value", "string");
    auto arg2 = v8::tracing::TracedValue::Create();
    arg2->SetDouble("value", 1.23);
    TRACE_EVENT_INSTANT2("v8", "v8.Test", TRACE_EVENT_SCOPE_THREAD, "a1",
                         std::move(arg), "a2", std::move(arg2));
  }

  harness.StopTracing();

  CHECK_EQ("B:v8.v8.Test.aa(aa=(uint)11)", harness.get_event(0));
  CHECK_EQ("B:v8.v8.Test.bb(bb=(uint)22)", harness.get_event(1));
  CHECK_EQ("B:v8.v8.Test.cc(cc=(uint)33)", harness.get_event(2));
  CHECK_EQ("B:v8.v8.Test.dd(dd=(uint)44)", harness.get_event(3));
  CHECK_EQ("B:v8.v8.Test.ee(ee=(int)-55)", harness.get_event(4));
  CHECK_EQ("B:v8.v8.Test.ff(ff=(int)-66)", harness.get_event(5));
  CHECK_EQ("B:v8.v8.Test.gg(gg=(int)-77)", harness.get_event(6));
  CHECK_EQ("B:v8.v8.Test.hh(hh=(int)-88)", harness.get_event(7));
  CHECK_EQ("B:v8.v8.Test.ii(ii1=(bool)1)", harness.get_event(8));
  CHECK_EQ("B:v8.v8.Test.ii(ii2=(bool)0)", harness.get_event(9));
  CHECK_EQ("B:v8.v8.Test.jj1(jj1=(double)99)", harness.get_event(10));
  CHECK_EQ("B:v8.v8.Test.jj2(jj2=(double)1e+100)", harness.get_event(11));
  CHECK_EQ("B:v8.v8.Test.jj3(jj3=(double)nan)", harness.get_event(12));
  CHECK_EQ("B:v8.v8.Test.jj4(jj4=(double)inf)", harness.get_event(13));
  CHECK_EQ("B:v8.v8.Test.jj5(jj5=(double)-inf)", harness.get_event(14));

  std::ostringstream pointer_stream;
  pointer_stream << "B:v8.v8.Test.kk(kk=(pointer)" << &aa << ")";
  CHECK_EQ(pointer_stream.str().c_str(), harness.get_event(15));

  CHECK_EQ("B:v8.v8.Test.ll(ll=(string)100)", harness.get_event(16));
  CHECK_EQ("B:v8.v8.Test.mm(mm=(string)\"INIT\")", harness.get_event(17));
  CHECK_EQ("B:v8.v8.Test2.1(aa=(uint)11,ll=(string)100)",
           harness.get_event(18));
  CHECK_EQ("B:v8.v8.Test2.2(mm1=(string)INIT,mm2=(string)\"INIT\")",
           harness.get_event(19));
  CHECK_EQ("I:v8.INIT", harness.get_event(20));
  CHECK_EQ("I:v8.INIT(mm1=(string)INIT,mm2=(string)\"INIT\")",
           harness.get_event(21));
  CHECK_EQ("I:v8.v8.Test(a1=(json){\"value\":42})", harness.get_event(22));
  CHECK_EQ(
      "I:v8.v8.Test(a1=(json){\"value\":\"string\"},a2=(json){\"value\":1.23})",
      harness.get_event(23));

  // Check the terminating end events.
  for (size_t i = 0; i < 20; i++) CHECK_EQ("E:.", harness.get_event(24 + i));
}

TEST(JsonIntegrationTest) {
  // Check that tricky values are rendered correctly in the JSON output.
  double big_num = 1e100;
  double nan_num = std::numeric_limits<double>::quiet_NaN();
  double inf_num = std::numeric_limits<double>::infinity();
  double neg_inf_num = -std::numeric_limits<double>::infinity();

  TracingTestHarness harness;
  harness.StartTracing();

  {
    TRACE_EVENT1("v8", "v8.Test.1", "1", big_num);
    TRACE_EVENT1("v8", "v8.Test.2", "2", nan_num);
    TRACE_EVENT1("v8", "v8.Test.3", "3", inf_num);
    TRACE_EVENT1("v8", "v8.Test.4", "4", neg_inf_num);
  }

  harness.StopTracing();
  std::string json = harness.perfetto_json_stream();

  std::vector<std::string> all_args;
  GetJSONStrings(&all_args, json, "\"args\"", "{", "}");

  CHECK_EQ("\"1\":1e+100", all_args[0]);
  CHECK_EQ("\"2\":\"NaN\"", all_args[1]);
  CHECK_EQ("\"3\":\"Infinity\"", all_args[2]);
  CHECK_EQ("\"4\":\"-Infinity\"", all_args[3]);
}

#endif  // V8_USE_PERFETTO

}  // namespace tracing
}  // namespace platform
}  // namespace v8
