#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "v8.h"

#include <set>
#include <string>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;

class TracingController : public v8::platform::tracing::TracingController {
 public:
  TracingController() : v8::platform::tracing::TracingController() {}

  int64_t CurrentTimestampMicroseconds() override {
    return uv_hrtime() / 1000;
  }
};

class Agent {
 public:
  explicit Agent(const std::string& log_file_pattern);
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

  void Enable(const std::string& categories);
  void Enable(const std::set<std::string>& categories);
  void Disable(const std::set<std::string>& categories);
  std::string GetEnabledCategories();

 private:
  static void ThreadCb(void* arg);

  void Start();
  void RestartTracing();

  TraceConfig* CreateTraceConfig();

  const std::string& log_file_pattern_;
  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool started_ = false;

  std::multiset<std::string> categories_;
  TracingController* tracing_controller_ = nullptr;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
