#include "tracing/agent.h"

#include <sstream>
#include <string>

#include "env-inl.h"
#include "libplatform/libplatform.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using std::string;

Agent::Agent() {}

void Agent::Start(v8::Platform* platform, const string& enabled_categories) {
  platform_ = platform;

  int err = uv_loop_init(&tracing_loop_);
  CHECK_EQ(err, 0);

  NodeTraceWriter* trace_writer = new NodeTraceWriter(&tracing_loop_);
  TraceBuffer* trace_buffer = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, trace_writer, &tracing_loop_);

  tracing_controller_ = new TracingController();

  TraceConfig* trace_config = new TraceConfig();
  if (!enabled_categories.empty()) {
    std::stringstream category_list(enabled_categories);
    while (category_list.good()) {
      std::string category;
      getline(category_list, category, ',');
      trace_config->AddIncludedCategory(category.c_str());
    }
  } else {
    trace_config->AddIncludedCategory("v8");
    trace_config->AddIncludedCategory("node");
  }

  // This thread should be created *after* async handles are created
  // (within NodeTraceWriter and NodeTraceBuffer constructors).
  // Otherwise the thread could shut down prematurely.
  err = uv_thread_create(&thread_, ThreadCb, this);
  CHECK_EQ(err, 0);

  tracing_controller_->Initialize(trace_buffer);
  tracing_controller_->StartTracing(trace_config);
  v8::platform::SetTracingController(platform, tracing_controller_);
}

void Agent::Stop() {
  if (!IsStarted()) {
    return;
  }
  // Perform final Flush on TraceBuffer. We don't want the tracing controller
  // to flush the buffer again on destruction of the V8::Platform.
  tracing_controller_->StopTracing();
  tracing_controller_->Initialize(nullptr);
  tracing_controller_ = nullptr;

  // Thread should finish when the tracing loop is stopped.
  uv_thread_join(&thread_);
  v8::platform::SetTracingController(platform_, nullptr);
}

// static
void Agent::ThreadCb(void* arg) {
  Agent* agent = static_cast<Agent*>(arg);
  uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
}

}  // namespace tracing
}  // namespace node
