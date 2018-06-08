#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "node_mutex.h"
#include "uv.h"
#include "v8.h"

#include <set>
#include <string>
#include <unordered_map>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceObject;

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

class Agent;

// Allows agent to be destroyed early. Methods are noop after the agent was
// destroyed
class AgentHandle {
 public:
  explicit AgentHandle(Agent* agent);
  void Reset();
  void AppendTraceEvent(TraceObject* trace_event);
  void Flush(bool blocking);
  void Disconnect(int client);

 private:
  Mutex mutex_;
  Agent* agent_;
};

class ClientHandle {
 public:
  ClientHandle(std::shared_ptr<AgentHandle> agent, int id)
               : agent_(agent), id_(id) {}
  ClientHandle(const ClientHandle&) = delete;
  ClientHandle& operator=(const ClientHandle&) = delete;
  ~ClientHandle();
  int id() const { return id_; }

 private:
  std::shared_ptr<AgentHandle> agent_;
  const int id_;
};

class Agent {
 public:
  explicit Agent(const std::string& log_file_pattern);
  ~Agent();
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

  // Destroying the handle disconnects the client
  std::unique_ptr<ClientHandle> AddClient(
      const std::set<std::string>& categories,
      std::unique_ptr<AsyncTraceWriter> writer);

  // These 3 methods operate on a "default" client, e.g. the file writer
  void Enable(const std::string& categories);
  void Enable(const std::set<std::string>& categories);
  void Disable(const std::set<std::string>& categories);
  std::string GetEnabledCategories();

  void AppendTraceEvent(TraceObject* trace_event);
  void Flush(bool blocking);

  TraceConfig* CreateTraceConfig();
  void Disconnect(int client);

 private:
  static void ThreadCb(void* arg);

  void Start();
  void StopTracing();

  const std::string& log_file_pattern_;
  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool started_ = false;

  std::unordered_map<int, std::set<std::string>> categories_;
  TracingController* tracing_controller_ = nullptr;
  std::unique_ptr<ClientHandle> file_writer_;
  int next_writer_id_ = 1;
  std::unordered_map<int, std::unique_ptr<AsyncTraceWriter>> writers_;
  std::multiset<std::string> file_writer_categories_;
  std::shared_ptr<AgentHandle> handle_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
