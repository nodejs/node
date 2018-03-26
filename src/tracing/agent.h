#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "v8.h"

#include <string>
#include <unordered_set>
#include <vector>

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
  explicit Agent(const std::string& log_file_pattern);

  void EnableCategories(const std::string& categories);
  void EnableCategories(const std::vector<std::string>& categories);
  void DisableCategories(const std::vector<std::string>& categories);
  const std::unordered_set<std::string>& GetEnabledCategories() const {
    return categories_;
  }

  void StopTracing();
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

 private:
  static void ThreadCb(void* arg);

  void InitializeOnce();
  void StartTracing();

  const std::string& log_file_pattern_;
  std::unordered_set<std::string> categories_;

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool initialized_ = false;
  bool started_ = false;
  TracingController* tracing_controller_ = nullptr;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
