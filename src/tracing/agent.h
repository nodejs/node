#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
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


class Agent {
 public:
  // Resetting the pointer disconnects client
  using ClientHandle = std::unique_ptr<std::pair<Agent*, int>,
                                       void (*)(std::pair<Agent*, int>*)>;

  static ClientHandle EmptyClientHandle() {
    return ClientHandle(nullptr, DisconnectClient);
  }
  explicit Agent(const std::string& log_file_pattern);
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

  // Destroying the handle disconnects the client
  ClientHandle AddClient(const std::set<std::string>& categories,
                         std::unique_ptr<AsyncTraceWriter> writer);

  // These 3 methods operate on a "default" client, e.g. the file writer
  void Enable(const std::string& categories);
  void Enable(const std::set<std::string>& categories);
  void Disable(const std::set<std::string>& categories);
  std::string GetEnabledCategories();

  void AppendTraceEvent(TraceObject* trace_event);
  void Flush(bool blocking);

  TraceConfig* CreateTraceConfig();

 private:
  static void ThreadCb(void* arg);
  static void DisconnectClient(std::pair<Agent*, int>* id_agent) {
    id_agent->first->Disconnect(id_agent->second);
    delete id_agent;
  }

  void Start();
  void StopTracing();
  void Disconnect(int client);

  const std::string& log_file_pattern_;
  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool started_ = false;

  std::unordered_map<int, std::set<std::string>> categories_;
  TracingController* tracing_controller_ = nullptr;
  ClientHandle file_writer_;
  int next_writer_id_ = 1;
  std::unordered_map<int, std::unique_ptr<AsyncTraceWriter>> writers_;
  std::multiset<std::string> file_writer_categories_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
