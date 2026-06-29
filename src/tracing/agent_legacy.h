#ifndef SRC_TRACING_AGENT_LEGACY_H_
#define SRC_TRACING_AGENT_LEGACY_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// This is an implementation of the legacy V8 tracing agent
// defined in `libplatform/v8-tracing.h`.

#include "libplatform/v8-tracing.h"
#include "node_mutex.h"
#include "tracing/agent.h"
#include "util.h"
#include "uv.h"

#include <list>
#include <set>
#include <string>
#include <unordered_map>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceObject;

class AsyncTraceWriter {
 public:
  virtual ~AsyncTraceWriter() = default;
  virtual void AppendTraceEvent(TraceObject* trace_event) = 0;
  virtual void Flush(bool blocking) = 0;
  virtual void InitializeOnThread(uv_loop_t* loop) {}
};

class TracingController : public v8::platform::tracing::TracingController {
 public:
  TracingController() : v8::platform::tracing::TracingController() {}

  int64_t CurrentTimestampMicroseconds() override { return uv_hrtime() / 1000; }
  void AddMetadataEvent(
      const unsigned char* category_group_enabled,
      const char* name,
      int num_args,
      const char** arg_names,
      const unsigned char* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* convertable_values,
      unsigned int flags);
};

class NodeTraceStateObserver
    : public v8::TracingController::TraceStateObserver {
 public:
  void OnTraceEnabled() override;
  void OnTraceDisabled() override { UNREACHABLE(); }

  explicit NodeTraceStateObserver(v8::TracingController* controller)
      : controller_(controller) {}
  ~NodeTraceStateObserver() override = default;

 private:
  v8::TracingController* controller_;
};

class LegacyTracingAgent final : public Agent {
 public:
  enum UseDefaultCategoryMode {
    kUseDefaultCategories,
    kIgnoreDefaultCategories
  };

  LegacyTracingAgent();
  ~LegacyTracingAgent() override;

  TracingController* GetTracingController() override {
    TracingController* controller = tracing_controller_.get();
    CHECK_NOT_NULL(controller);
    return controller;
  }

  AgentWriterHandle AddClient(const std::set<std::string>& categories,
                              std::unique_ptr<AsyncTraceWriter> writer,
                              enum UseDefaultCategoryMode mode);
  std::string GetEnabledCategories() const override;
  void AppendTraceEvent(TraceObject* trace_event);
  void AddMetadataEvent(std::unique_ptr<TraceObject> event);
  void Flush(bool blocking);
  TraceConfig* CreateTraceConfig() const;

  void StartTracing(const std::string& categories) override;
  void StopTracing() override;
  AgentWriterHandle* GetDefaultWriterHandle() override;

  void AddTraceStateObserver(
      v8::TracingController::TraceStateObserver* observer) override {
    tracing_controller_->AddTraceStateObserver(observer);
  }
  void RemoveTraceStateObserver(
      v8::TracingController::TraceStateObserver* observer) override {
    tracing_controller_->RemoveTraceStateObserver(observer);
  }

 private:
  static constexpr int kDefaultHandleId = -1;

  void Disconnect(int client) override;

  void Enable(int id, const std::set<std::string>& categories) override;
  void Disable(int id, const std::set<std::string>& categories) override;

  void InitializeWritersOnThread();
  void Start();
  void Stop();

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;

  bool started_ = false;
  class ScopedSuspendTracing;

  int next_writer_id_ = 1;
  std::unordered_map<int, std::multiset<std::string>> categories_;
  std::unordered_map<int, std::unique_ptr<AsyncTraceWriter>> writers_;
  std::unique_ptr<TracingController> tracing_controller_;

  Mutex initialize_writer_mutex_;
  ConditionVariable initialize_writer_condvar_;
  uv_async_t initialize_writer_async_;
  std::set<AsyncTraceWriter*> to_be_initialized_;

  Mutex metadata_events_mutex_;
  std::list<std::unique_ptr<TraceObject>> metadata_events_;

  AgentWriterHandle tracing_file_writer_;
  std::unique_ptr<NodeTraceStateObserver> trace_state_observer_;
};

}  // namespace tracing
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TRACING_AGENT_LEGACY_H_
