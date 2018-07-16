#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "v8.h"
#include "util.h"

#include <set>
#include <string>
#include <unordered_map>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceObject;

class Agent;

class AsyncTraceWriter {
 public:
  virtual ~AsyncTraceWriter() {}
  virtual void AppendTraceEvent(TraceObject* trace_event) = 0;
  virtual void Flush(bool blocking) = 0;
};

class TracingController : public v8::platform::tracing::TracingController {
 public:
  TracingController() : v8::platform::tracing::TracingController() {}

  int64_t CurrentTimestampMicroseconds() override {
    return uv_hrtime() / 1000;
  }
};

class AgentWriterHandle {
 public:
  inline AgentWriterHandle() {}
  inline ~AgentWriterHandle() { reset(); }

  inline AgentWriterHandle(AgentWriterHandle&& other);
  inline AgentWriterHandle& operator=(AgentWriterHandle&& other);
  inline bool empty() const { return agent_ == nullptr; }
  inline void reset();

 private:
  inline AgentWriterHandle(Agent* agent, int id) : agent_(agent), id_(id) {}

  AgentWriterHandle(const AgentWriterHandle& other) = delete;
  AgentWriterHandle& operator=(const AgentWriterHandle& other) = delete;

  Agent* agent_ = nullptr;
  int id_;

  friend class Agent;
};

class Agent {
 public:
  explicit Agent(const std::string& log_file_pattern);
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

  // Destroying the handle disconnects the client
  AgentWriterHandle AddClient(const std::set<std::string>& categories,
                              std::unique_ptr<AsyncTraceWriter> writer);

  // These 3 methods operate on a "default" client, e.g. the file writer
  void Enable(const std::string& categories);
  void Enable(const std::set<std::string>& categories);
  void Disable(const std::set<std::string>& categories);

  // Returns a comma-separated list of enabled categories.
  std::string GetEnabledCategories() const;

  // Writes to all writers registered through AddClient().
  void AppendTraceEvent(TraceObject* trace_event);
  // Flushes all writers registered through AddClient().
  void Flush(bool blocking);

  TraceConfig* CreateTraceConfig() const;

 private:
  friend class AgentWriterHandle;

  static void ThreadCb(void* arg);

  void Start();
  void StopTracing();
  void Disconnect(int client);

  const std::string& log_file_pattern_;
  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool started_ = false;

  // Each individual Writer has one id.
  int next_writer_id_ = 1;
  // These maps store the original arguments to AddClient(), by id.
  std::unordered_map<int, std::set<std::string>> categories_;
  std::unordered_map<int, std::unique_ptr<AsyncTraceWriter>> writers_;
  TracingController* tracing_controller_ = nullptr;
  AgentWriterHandle file_writer_;
  std::multiset<std::string> file_writer_categories_;
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

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
