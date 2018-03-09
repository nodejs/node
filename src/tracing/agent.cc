#include "tracing/agent.h"
#include "tracing/node_trace_buffer.h"
#include "tracing/node_trace_writer.h"
#include "env-inl.h"

#include <sstream>
#include <string>
#include <vector>

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;

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

void Agent::StartTracing() {
  tracing_controller_->StopTracing();
  if (!categories_.empty()) {
    if (!started_) {
      InitializeOnce();
      // This thread should be created *after* async handles are created
      // (within NodeTraceWriter and NodeTraceBuffer constructors).
      // Otherwise the thread could shut down prematurely.
      int err = uv_thread_create(&thread_, ThreadCb, this);
      CHECK_EQ(err, 0);
      started_ = true;
    }
    TraceConfig* trace_config = new TraceConfig();
    for (auto it = categories_.begin(); it != categories_.end(); it++) {
      trace_config->AddIncludedCategory((*it).c_str());
    }
    tracing_controller_->StartTracing(trace_config);
  }
}

void Agent::EnableCategories(const std::string& categories) {
  if (!categories.empty()) {
    std::stringstream category_list(categories);
    while (category_list.good()) {
      std::string category;
      getline(category_list, category, ',');
      categories_.insert(category);
    }
    StartTracing();
  }
}

void Agent::EnableCategories(const std::vector<std::string>& categories) {
  if (!categories.empty()) {
    for (auto category = categories.begin();
         category != categories.end();
         category++) {
      categories_.insert(*category);
    }
    StartTracing();
  }
}

void Agent::DisableCategories(const std::vector<std::string>& categories) {
  if (!categories.empty()) {
    for (auto category = categories.begin();
         category != categories.end();
         category++) {
      categories_.erase(*category);
    }
    StartTracing();
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
