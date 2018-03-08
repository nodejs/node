#include "tracing/agent.h"

#include <sstream>
#include <string>
#include "tracing/node_trace_buffer.h"
#include "tracing/node_trace_writer.h"

#include "env-inl.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using std::string;

Agent::Agent(const std::string& log_file_pattern) :
    log_file_pattern_(log_file_pattern) {
  tracing_controller_ = new TracingController();
  tracing_controller_->Initialize(nullptr);
}

void Agent::InitializeOnce() {
  if (initialized_)
    return;
  int err = uv_loop_init(&tracing_loop_);
  CHECK_EQ(err, 0);

  NodeTraceWriter* trace_writer = new NodeTraceWriter(
      log_file_pattern_, &tracing_loop_);
  TraceBuffer* trace_buffer = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, trace_writer, &tracing_loop_);
  tracing_controller_->Initialize(trace_buffer);
}

void Agent::StartTracing(const string& enabled_categories) {
  TraceConfig* trace_config = new TraceConfig();
  tracing_controller_->StopTracing();
  if (!enabled_categories.empty()) {
    if (!started_) {
      InitializeOnce();
      // This thread should be created *after* async handles are created
      // (within NodeTraceWriter and NodeTraceBuffer constructors).
      // Otherwise the thread could shut down prematurely.
      int err = uv_thread_create(&thread_, ThreadCb, this);
      CHECK_EQ(err, 0);
      started_ = true;
    }
    std::stringstream category_list(enabled_categories);
    while (category_list.good()) {
      std::string category;
      getline(category_list, category, ',');
      trace_config->AddIncludedCategory(category.c_str());
    }
    tracing_controller_->StartTracing(trace_config);
  }
}

void Agent::StopTracing() {
  tracing_controller_->StopTracing();
}

void Agent::Stop() {
  // Perform final Flush on TraceBuffer. We don't want the tracing controller
  // to flush the buffer again on destruction of the V8::Platform.
  tracing_controller_->StopTracing();
  tracing_controller_->Initialize(nullptr);

  // Thread should finish when the tracing loop is stopped.
  if (started_) {
    uv_thread_join(&thread_);
    started_ = false;
  }
}

// static
void Agent::ThreadCb(void* arg) {
  Agent* agent = static_cast<Agent*>(arg);
  uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
}

}  // namespace tracing
}  // namespace node
