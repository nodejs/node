// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/v8-tracing.h"

#include "src/base/atomicops.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"

#ifdef V8_USE_PERFETTO
#include "base/trace_event/common/trace_event_common.h"
#include "perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "perfetto/trace/trace_packet.pbzero.h"
#include "perfetto/tracing.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/semaphore.h"
#include "src/libplatform/tracing/json-trace-event-listener.h"
#endif  // V8_USE_PERFETTO

#ifdef V8_USE_PERFETTO
class V8DataSource : public perfetto::DataSource<V8DataSource> {
 public:
  void OnSetup(const SetupArgs&) override {}
  void OnStart(const StartArgs&) override {}
  void OnStop(const StopArgs&) override {}
};

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(V8DataSource);
#endif  // V8_USE_PERFETTO

namespace v8 {
namespace platform {
namespace tracing {

static const size_t kMaxCategoryGroups = 200;

// Parallel arrays g_category_groups and g_category_group_enabled are separate
// so that a pointer to a member of g_category_group_enabled can be easily
// converted to an index into g_category_groups. This allows macros to deal
// only with char enabled pointers from g_category_group_enabled, and we can
// convert internally to determine the category name from the char enabled
// pointer.
const char* g_category_groups[kMaxCategoryGroups] = {
    "toplevel",
    "tracing categories exhausted; must increase kMaxCategoryGroups",
    "__metadata"};

// The enabled flag is char instead of bool so that the API can be used from C.
unsigned char g_category_group_enabled[kMaxCategoryGroups] = {0};
// Indexes here have to match the g_category_groups array indexes above.
const int g_category_categories_exhausted = 1;
// Metadata category not used in V8.
// const int g_category_metadata = 2;
const int g_num_builtin_categories = 3;

// Skip default categories.
v8::base::AtomicWord g_category_index = g_num_builtin_categories;

TracingController::TracingController() = default;

TracingController::~TracingController() {
  StopTracing();

  {
    // Free memory for category group names allocated via strdup.
    base::MutexGuard lock(mutex_.get());
    for (size_t i = g_category_index - 1; i >= g_num_builtin_categories; --i) {
      const char* group = g_category_groups[i];
      g_category_groups[i] = nullptr;
      free(const_cast<char*>(group));
    }
    g_category_index = g_num_builtin_categories;
  }
}

void TracingController::Initialize(TraceBuffer* trace_buffer) {
  trace_buffer_.reset(trace_buffer);
  mutex_.reset(new base::Mutex());
}

#ifdef V8_USE_PERFETTO
void TracingController::InitializeForPerfetto(std::ostream* output_stream) {
  output_stream_ = output_stream;
  DCHECK_NOT_NULL(output_stream);
  DCHECK(output_stream->good());
  mutex_.reset(new base::Mutex());
}

void TracingController::SetTraceEventListenerForTesting(
    TraceEventListener* listener) {
  listener_for_testing_ = listener;
}
#endif

int64_t TracingController::CurrentTimestampMicroseconds() {
  return base::TimeTicks::HighResolutionNow().ToInternalValue();
}

int64_t TracingController::CurrentCpuTimestampMicroseconds() {
  return base::ThreadTicks::Now().ToInternalValue();
}

namespace {

#ifdef V8_USE_PERFETTO
void AddArgsToTraceProto(
    ::perfetto::protos::pbzero::ChromeTraceEvent* event, int num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables) {
  for (int i = 0; i < num_args; i++) {
    ::perfetto::protos::pbzero::ChromeTraceEvent_Arg* arg = event->add_args();
    // TODO(petermarshall): Set name_index instead if need be.
    arg->set_name(arg_names[i]);

    TraceObject::ArgValue arg_value;
    arg_value.as_uint = arg_values[i];
    switch (arg_types[i]) {
      case TRACE_VALUE_TYPE_CONVERTABLE: {
        // TODO(petermarshall): Support AppendToProto for Convertables.
        std::string json_value;
        arg_convertables[i]->AppendAsTraceFormat(&json_value);
        arg->set_json_value(json_value.c_str());
        break;
      }
      case TRACE_VALUE_TYPE_BOOL:
        arg->set_bool_value(arg_value.as_bool);
        break;
      case TRACE_VALUE_TYPE_UINT:
        arg->set_uint_value(arg_value.as_uint);
        break;
      case TRACE_VALUE_TYPE_INT:
        arg->set_int_value(arg_value.as_int);
        break;
      case TRACE_VALUE_TYPE_DOUBLE:
        arg->set_double_value(arg_value.as_double);
        break;
      case TRACE_VALUE_TYPE_POINTER:
        arg->set_pointer_value(arg_value.as_uint);
        break;
      // There is no difference between copy strings and regular strings for
      // Perfetto; the set_string_value(const char*) API will copy the string
      // into the protobuf by default.
      case TRACE_VALUE_TYPE_COPY_STRING:
      case TRACE_VALUE_TYPE_STRING:
        arg->set_string_value(arg_value.as_string);
        break;
      default:
        UNREACHABLE();
    }
  }
}
#endif  // V8_USE_PERFETTO

}  // namespace

uint64_t TracingController::AddTraceEvent(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags) {
  int64_t now_us = CurrentTimestampMicroseconds();

  return AddTraceEventWithTimestamp(
      phase, category_enabled_flag, name, scope, id, bind_id, num_args,
      arg_names, arg_types, arg_values, arg_convertables, flags, now_us);
}

uint64_t TracingController::AddTraceEventWithTimestamp(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags, int64_t timestamp) {
  int64_t cpu_now_us = CurrentCpuTimestampMicroseconds();

#ifdef V8_USE_PERFETTO
    // Don't use COMPLETE events with perfetto - instead transform them into
    // BEGIN/END pairs. This avoids the need for a thread-local stack of pending
    // trace events as perfetto does not support handles into the trace buffer.
    if (phase == TRACE_EVENT_PHASE_COMPLETE) phase = TRACE_EVENT_PHASE_BEGIN;

    V8DataSource::Trace([&](V8DataSource::TraceContext ctx) {
      auto packet = ctx.NewTracePacket();
      auto* trace_event_bundle = packet->set_chrome_events();
      auto* trace_event = trace_event_bundle->add_trace_events();

      trace_event->set_name(name);
      trace_event->set_timestamp(timestamp);
      trace_event->set_phase(phase);
      trace_event->set_thread_id(base::OS::GetCurrentThreadId());
      trace_event->set_duration(0);
      trace_event->set_thread_duration(0);
      if (scope) trace_event->set_scope(scope);
      trace_event->set_id(id);
      trace_event->set_flags(flags);
      if (category_enabled_flag) {
        const char* category_group_name =
            GetCategoryGroupName(category_enabled_flag);
        DCHECK_NOT_NULL(category_group_name);
        trace_event->set_category_group_name(category_group_name);
      }
      trace_event->set_process_id(base::OS::GetCurrentProcessId());
      trace_event->set_thread_timestamp(cpu_now_us);
      trace_event->set_bind_id(bind_id);

      AddArgsToTraceProto(trace_event, num_args, arg_names, arg_types,
                          arg_values, arg_convertables);
    });
    return 0;
#else

  uint64_t handle = 0;
  if (recording_.load(std::memory_order_acquire)) {
    TraceObject* trace_object = trace_buffer_->AddTraceEvent(&handle);
    if (trace_object) {
      {
        base::MutexGuard lock(mutex_.get());
        trace_object->Initialize(phase, category_enabled_flag, name, scope, id,
                                 bind_id, num_args, arg_names, arg_types,
                                 arg_values, arg_convertables, flags, timestamp,
                                 cpu_now_us);
      }
    }
  }
  return handle;
#endif  // V8_USE_PERFETTO
}

void TracingController::UpdateTraceEventDuration(
    const uint8_t* category_enabled_flag, const char* name, uint64_t handle) {
  int64_t now_us = CurrentTimestampMicroseconds();
  int64_t cpu_now_us = CurrentCpuTimestampMicroseconds();

#ifdef V8_USE_PERFETTO
  V8DataSource::Trace([&](V8DataSource::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    auto* trace_event_bundle = packet->set_chrome_events();
    auto* trace_event = trace_event_bundle->add_trace_events();

    trace_event->set_phase(TRACE_EVENT_PHASE_END);
    trace_event->set_thread_id(base::OS::GetCurrentThreadId());
    trace_event->set_timestamp(now_us);
    trace_event->set_process_id(base::OS::GetCurrentProcessId());
    trace_event->set_thread_timestamp(cpu_now_us);
  });
#else

  TraceObject* trace_object = trace_buffer_->GetEventByHandle(handle);
  if (!trace_object) return;
  trace_object->UpdateDuration(now_us, cpu_now_us);
#endif  // V8_USE_PERFETTO
}

const char* TracingController::GetCategoryGroupName(
    const uint8_t* category_group_enabled) {
  // Calculate the index of the category group by finding
  // category_group_enabled in g_category_group_enabled array.
  uintptr_t category_begin =
      reinterpret_cast<uintptr_t>(g_category_group_enabled);
  uintptr_t category_ptr = reinterpret_cast<uintptr_t>(category_group_enabled);
  // Check for out of bounds category pointers.
  DCHECK(category_ptr >= category_begin &&
         category_ptr < reinterpret_cast<uintptr_t>(g_category_group_enabled +
                                                    kMaxCategoryGroups));
  uintptr_t category_index =
      (category_ptr - category_begin) / sizeof(g_category_group_enabled[0]);
  return g_category_groups[category_index];
}

void TracingController::StartTracing(TraceConfig* trace_config) {
#ifdef V8_USE_PERFETTO
  DCHECK_NOT_NULL(output_stream_);
  DCHECK(output_stream_->good());
  json_listener_ = base::make_unique<JSONTraceEventListener>(output_stream_);

  // TODO(petermarshall): Set other the params for the config.
  ::perfetto::TraceConfig perfetto_trace_config;
  perfetto_trace_config.add_buffers()->set_size_kb(4096);
  auto* ds_config = perfetto_trace_config.add_data_sources()->mutable_config();
  ds_config->set_name("v8.trace_events");

  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("v8.trace_events");
  bool registered = V8DataSource::Register(dsd);
  CHECK(registered);

  tracing_session_ =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kUnspecifiedBackend);
  tracing_session_->Setup(perfetto_trace_config);
  tracing_session_->StartBlocking();

#endif  // V8_USE_PERFETTO

  trace_config_.reset(trace_config);
  std::unordered_set<v8::TracingController::TraceStateObserver*> observers_copy;
  {
    base::MutexGuard lock(mutex_.get());
    recording_.store(true, std::memory_order_release);
    UpdateCategoryGroupEnabledFlags();
    observers_copy = observers_;
  }
  for (auto o : observers_copy) {
    o->OnTraceEnabled();
  }
}

void TracingController::StopTracing() {
  bool expected = true;
  if (!recording_.compare_exchange_strong(expected, false)) {
    return;
  }
  UpdateCategoryGroupEnabledFlags();
  std::unordered_set<v8::TracingController::TraceStateObserver*> observers_copy;
  {
    base::MutexGuard lock(mutex_.get());
    observers_copy = observers_;
  }
  for (auto o : observers_copy) {
    o->OnTraceDisabled();
  }

#ifdef V8_USE_PERFETTO
  // Emit a fake trace event from the main thread. The final trace event is
  // sometimes skipped because perfetto can't guarantee that the caller is
  // totally finished writing to it without synchronization. To avoid the
  // situation where we lose the last trace event, add a fake one here that will
  // be sacrificed.
  // TODO(petermarshall): Use the Client API to flush here rather than this
  // workaround when that becomes available.
  V8DataSource::Trace([&](V8DataSource::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
  });
  tracing_session_->StopBlocking();

  std::vector<char> trace = tracing_session_->ReadTraceBlocking();
  json_listener_->ParseFromArray(trace);
  if (listener_for_testing_) listener_for_testing_->ParseFromArray(trace);

  json_listener_.reset();
#else

  {
    base::MutexGuard lock(mutex_.get());
    DCHECK(trace_buffer_);
    trace_buffer_->Flush();
  }
#endif  // V8_USE_PERFETTO
}

void TracingController::UpdateCategoryGroupEnabledFlag(size_t category_index) {
  unsigned char enabled_flag = 0;
  const char* category_group = g_category_groups[category_index];
  if (recording_.load(std::memory_order_acquire) &&
      trace_config_->IsCategoryGroupEnabled(category_group)) {
    enabled_flag |= ENABLED_FOR_RECORDING;
  }

  // TODO(fmeawad): EventCallback and ETW modes are not yet supported in V8.
  // TODO(primiano): this is a temporary workaround for catapult:#2341,
  // to guarantee that metadata events are always added even if the category
  // filter is "-*". See crbug.com/618054 for more details and long-term fix.
  if (recording_.load(std::memory_order_acquire) &&
      !strcmp(category_group, "__metadata")) {
    enabled_flag |= ENABLED_FOR_RECORDING;
  }

  base::Relaxed_Store(reinterpret_cast<base::Atomic8*>(
                          g_category_group_enabled + category_index),
                      enabled_flag);
}

void TracingController::UpdateCategoryGroupEnabledFlags() {
  size_t category_index = base::Acquire_Load(&g_category_index);
  for (size_t i = 0; i < category_index; i++) UpdateCategoryGroupEnabledFlag(i);
}

const uint8_t* TracingController::GetCategoryGroupEnabled(
    const char* category_group) {
  // Check that category group does not contain double quote
  DCHECK(!strchr(category_group, '"'));

  // The g_category_groups is append only, avoid using a lock for the fast path.
  size_t category_index = base::Acquire_Load(&g_category_index);

  // Search for pre-existing category group.
  for (size_t i = 0; i < category_index; ++i) {
    if (strcmp(g_category_groups[i], category_group) == 0) {
      return &g_category_group_enabled[i];
    }
  }

  // Slow path. Grab the lock.
  base::MutexGuard lock(mutex_.get());

  // Check the list again with lock in hand.
  unsigned char* category_group_enabled = nullptr;
  category_index = base::Acquire_Load(&g_category_index);
  for (size_t i = 0; i < category_index; ++i) {
    if (strcmp(g_category_groups[i], category_group) == 0) {
      return &g_category_group_enabled[i];
    }
  }

  // Create a new category group.
  // Check that there is a slot for the new category_group.
  DCHECK(category_index < kMaxCategoryGroups);
  if (category_index < kMaxCategoryGroups) {
    // Don't hold on to the category_group pointer, so that we can create
    // category groups with strings not known at compile time (this is
    // required by SetWatchEvent).
    const char* new_group = strdup(category_group);
    g_category_groups[category_index] = new_group;
    DCHECK(!g_category_group_enabled[category_index]);
    // Note that if both included and excluded patterns in the
    // TraceConfig are empty, we exclude nothing,
    // thereby enabling this category group.
    UpdateCategoryGroupEnabledFlag(category_index);
    category_group_enabled = &g_category_group_enabled[category_index];
    // Update the max index now.
    base::Release_Store(&g_category_index, category_index + 1);
  } else {
    category_group_enabled =
        &g_category_group_enabled[g_category_categories_exhausted];
  }
  return category_group_enabled;
}

void TracingController::AddTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {
  {
    base::MutexGuard lock(mutex_.get());
    observers_.insert(observer);
    if (!recording_.load(std::memory_order_acquire)) return;
  }
  // Fire the observer if recording is already in progress.
  observer->OnTraceEnabled();
}

void TracingController::RemoveTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {
  base::MutexGuard lock(mutex_.get());
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

}  // namespace tracing
}  // namespace platform
}  // namespace v8
