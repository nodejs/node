#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "tracing/node_trace_buffer.h"
#include "tracing/node_trace_writer.h"
#include "uv.h"
#include "v8.h"

namespace node {
namespace tracing {

class Agent {
 public:
  Agent();
  void Start(v8::Platform* platform, const std::string& enabled_categories);
  void Stop();

 private:
  bool IsStarted() { return platform_ != nullptr; }
  static void ThreadCb(void* arg);

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  v8::Platform* platform_ = nullptr;
  TracingController* tracing_controller_ = nullptr;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
