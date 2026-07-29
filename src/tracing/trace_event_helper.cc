#include "tracing/trace_event_helper.h"
#include "node.h"
#include "uv.h"

#include <string>
#include <string_view>

namespace node {
namespace tracing {

v8::TracingController* g_controller = nullptr;

v8::TracingController* TraceEventHelper::GetTracingController() {
  return g_controller;
}

void TraceEventHelper::SetTracingController(v8::TracingController* controller) {
  g_controller = controller;
}

namespace {
void replace_substring(std::string* target,
                       std::string_view search,
                       std::string_view insert) {
  size_t pos = target->find(search);
  for (; pos != std::string::npos; pos = target->find(search, pos)) {
    target->replace(pos, search.size(), insert);
    pos += insert.size();
  }
}
}  // namespace

std::string GetTraceFilePath(std::string_view log_file_pattern, int file_num) {
  // Evaluate a JS-style template string that accepts ${pid} and ${rotation}.
  std::string filepath(log_file_pattern);
  replace_substring(&filepath, "${pid}", std::to_string(uv_os_getpid()));
  replace_substring(&filepath, "${rotation}", std::to_string(file_num));
  return filepath;
}

}  // namespace tracing

v8::TracingController* GetTracingController() {
  return tracing::TraceEventHelper::GetTracingController();
}

void SetTracingController(v8::TracingController* controller) {
  tracing::TraceEventHelper::SetTracingController(controller);
}

}  // namespace node
