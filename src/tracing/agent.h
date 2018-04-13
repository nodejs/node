#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "v8.h"
#include "node_trace_writer.h"

namespace node {
namespace tracing {

class TracingController : public v8::platform::tracing::TracingController {
 public:
  TracingController() : v8::platform::tracing::TracingController() {}

  int64_t CurrentTimestampMicroseconds() override {
    return uv_hrtime() / 1000;
  }
};

class Agent {
 public:
  explicit Agent(const std::string& log_file_pattern,
                 enum trace_format format = TRACE_FORMAT_JSON);
  void Start(const std::string& enabled_categories);
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

 private:
  static void ThreadCb(void* arg);

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool started_ = false;
  TracingController* tracing_controller_ = nullptr;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
