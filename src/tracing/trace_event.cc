#include "tracing/trace_event.h"

namespace node {
namespace tracing {

v8::Platform* platform_ = nullptr;

void TraceEventHelper::SetCurrentPlatform(v8::Platform* platform) {
  platform_ = platform;
}

v8::Platform* TraceEventHelper::GetCurrentPlatform() {
  return platform_;
}

}  // namespace tracing
}  // namespace node
