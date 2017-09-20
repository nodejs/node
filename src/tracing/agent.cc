#include "tracing/agent.h"

#include <sstream>
#include <string>

#include "env-inl.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using std::string;

Agent::Agent() {
  int err = uv_loop_init(&tracing_loop_);
  CHECK_EQ(err, 0);

  NodeTraceWriter* trace_writer = new NodeTraceWriter(&tracing_loop_);
  TraceBuffer* trace_buffer = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, trace_writer, &tracing_loop_);
  tracing_controller_ = new TracingController();
  tracing_controller_->Initialize(trace_buffer);
}

void Agent::Start(const string& enabled_categories) {
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
  int err = uv_thread_create(&thread_, ThreadCb, this);
  CHECK_EQ(err, 0);

  tracing_controller_->StartTracing(trace_config);
  started_ = true;
}

void Agent::Stop() {
  if (!started_) {
    return;
  }
  // Perform final Flush on TraceBuffer. We don't want the tracing controller
  // to flush the buffer again on destruction of the V8::Platform.
  tracing_controller_->StopTracing();
  tracing_controller_->Initialize(nullptr);
  started_ = false;

  // Thread should finish when the tracing loop is stopped.
  uv_thread_join(&thread_);
}

// static
void Agent::ThreadCb(void* arg) {
  Agent* agent = static_cast<Agent*>(arg);
  uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
}

}  // namespace tracing
}  // namespace node
