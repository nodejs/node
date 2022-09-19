#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "util.h"
#include "node_mutex.h"

#include <list>
#include <set>
#include <string>
#include <unordered_map>

namespace v8 {
class ConvertableToTraceFormat;
class TracingController;
}  // namespace v8

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceObject;

class Agent;

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

  int64_t CurrentTimestampMicroseconds() override {
    return uv_hrtime() / 1000;
  }
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

class AgentWriterHandle {
 public:
  inline AgentWriterHandle() = default;
  inline ~AgentWriterHandle() { reset(); }

  inline AgentWriterHandle(AgentWriterHandle&& other);
  inline AgentWriterHandle& operator=(AgentWriterHandle&& other);
  inline bool empty() const { return agent_ == nullptr; }
  inline void reset();

  inline void Enable(const std::set<std::string>& categories);
  inline void Disable(const std::set<std::string>& categories);

  inline bool IsDefaultHandle();

  inline Agent* agent() { return agent_; }

  inline v8::TracingController* GetTracingController();

  AgentWriterHandle(const AgentWriterHandle& other) = delete;
  AgentWriterHandle& operator=(const AgentWriterHandle& other) = delete;

 private:
  inline AgentWriterHandle(Agent* agent, int id) : agent_(agent), id_(id) {}

  Agent* agent_ = nullptr;
  int id_ = 0;

  friend class Agent;
};

class Agent {
 public:
  Agent();
  ~Agent();

  TracingController* GetTracingController() {
    TracingController* controller = tracing_controller_.get();
    CHECK_NOT_NULL(controller);
    return controller;
  }

  enum UseDefaultCategoryMode {
    kUseDefaultCategories,
    kIgnoreDefaultCategories
  };

  // Destroying the handle disconnects the client
  AgentWriterHandle AddClient(const std::set<std::string>& categories,
                              std::unique_ptr<AsyncTraceWriter> writer,
                              enum UseDefaultCategoryMode mode);
  // A handle that is only used for managing the default categories
  // (which can then implicitly be used through using `USE_DEFAULT_CATEGORIES`
  // when adding a client later).
  AgentWriterHandle DefaultHandle();

  // Returns a comma-separated list of enabled categories.
  std::string GetEnabledCategories() const;

  // Writes to all writers registered through AddClient().
  void AppendTraceEvent(TraceObject* trace_event);

  void AddMetadataEvent(std::unique_ptr<TraceObject> event);
  // Flushes all writers registered through AddClient().
  void Flush(bool blocking);

  TraceConfig* CreateTraceConfig() const;

 private:
  friend class AgentWriterHandle;

  void InitializeWritersOnThread();

  void Start();
  void StopTracing();
  void Disconnect(int client);

  void Enable(int id, const std::set<std::string>& categories);
  void Disable(int id, const std::set<std::string>& categories);

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;

  bool started_ = false;
  class ScopedSuspendTracing;

  // Each individual Writer has one id.
  int next_writer_id_ = 1;
  enum { kDefaultHandleId = -1 };
  // These maps store the original arguments to AddClient(), by id.
  std::unordered_map<int, std::multiset<std::string>> categories_;
  std::unordered_map<int, std::unique_ptr<AsyncTraceWriter>> writers_;
  std::unique_ptr<TracingController> tracing_controller_;

  // Variables related to initializing per-event-loop properties of individual
  // writers, such as libuv handles.
  Mutex initialize_writer_mutex_;
  ConditionVariable initialize_writer_condvar_;
  uv_async_t initialize_writer_async_;
  std::set<AsyncTraceWriter*> to_be_initialized_;

  Mutex metadata_events_mutex_;
  std::list<std::unique_ptr<TraceObject>> metadata_events_;
};

void AgentWriterHandle::reset() {
  if (agent_ != nullptr)
    agent_->Disconnect(id_);
  agent_ = nullptr;
}

AgentWriterHandle& AgentWriterHandle::operator=(AgentWriterHandle&& other) {
  reset();
  agent_ = other.agent_;
  id_ = other.id_;
  other.agent_ = nullptr;
  return *this;
}

AgentWriterHandle::AgentWriterHandle(AgentWriterHandle&& other) {
  *this = std::move(other);
}

void AgentWriterHandle::Enable(const std::set<std::string>& categories) {
  if (agent_ != nullptr) agent_->Enable(id_, categories);
}

void AgentWriterHandle::Disable(const std::set<std::string>& categories) {
  if (agent_ != nullptr) agent_->Disable(id_, categories);
}

bool AgentWriterHandle::IsDefaultHandle() {
  return agent_ != nullptr && id_ == Agent::kDefaultHandleId;
}

inline v8::TracingController* AgentWriterHandle::GetTracingController() {
  return agent_ != nullptr ? agent_->GetTracingController() : nullptr;
}

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
