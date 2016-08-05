#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "tracing/node_trace_buffer.h"
#include "tracing/node_trace_writer.h"
#include "uv.h"
#include "v8.h"

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace node {
namespace tracing {

class Agent {
 public:
  explicit Agent(Environment* env);
  void Start(v8::Platform* platform, const char* trace_config_file);
  void Stop();
  void SendFlushSignal();

 private:
  static void ThreadCb(void* agent);
  static void FlushSignalCb(uv_async_t* signal);

  bool IsStarted() { return platform_ != nullptr; }
  void WorkerRun();

  v8::Platform* platform_ = nullptr;
  Environment* parent_env_;
  uv_thread_t thread_;
  uv_loop_t child_loop_;
  uv_async_t flush_signal_;
  TracingController* tracing_controller_;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
