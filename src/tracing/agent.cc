#include "tracing/agent.h"

#include <sstream>
#include <string>
#include "tracing/node_trace_buffer.h"
#include "tracing/node_trace_writer.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TraceConfig;
using std::string;

Agent::Agent(const std::string& log_file_pattern)
    : log_file_pattern_(log_file_pattern) {
  tracing_controller_ = new TracingController();
  tracing_controller_->Initialize(nullptr);
}

void Agent::Start() {
  if (started_)
    return;

  CHECK_EQ(uv_loop_init(&tracing_loop_), 0);

  NodeTraceWriter* trace_writer =
      new NodeTraceWriter(log_file_pattern_, &tracing_loop_);
  TraceBuffer* trace_buffer = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, trace_writer, &tracing_loop_);
  tracing_controller_->Initialize(trace_buffer);

  // This thread should be created *after* async handles are created
  // (within NodeTraceWriter and NodeTraceBuffer constructors).
  // Otherwise the thread could shut down prematurely.
  CHECK_EQ(0, uv_thread_create(&thread_, ThreadCb, this));
  started_ = true;
}

void Agent::Stop() {
  if (!started_)
    return;
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

void Agent::Enable(const std::string& categories) {
  if (!categories.empty()) {
    std::stringstream category_list(categories);
    while (category_list.good()) {
      std::string category;
      getline(category_list, category, ',');
      categories_.insert(category.c_str());
    }
    RestartTracing();
  }
}

void Agent::Enable(const std::set<std::string>& categories) {
  if (!categories.empty()) {
    categories_.insert(categories.begin(), categories.end());
    RestartTracing();
  }
}

void Agent::Disable(const std::set<std::string>& categories) {
  if (!categories.empty()) {
    for (auto category : categories) {
      auto pos = categories_.lower_bound(category);
      if (pos != categories_.end())
        categories_.erase(pos);
    }
    RestartTracing();
  }
}

void Agent::RestartTracing() {
  static bool warned;
  if (!warned) {
    warned = true;
    fprintf(stderr, "Warning: Trace event is an experimental feature "
            "and could change at any time.\n");
  }
  Start();  // Start the agent if it hasn't already been started
  tracing_controller_->StopTracing();
  auto config = CreateTraceConfig();
  if (config != nullptr)
    tracing_controller_->StartTracing(config);
}

TraceConfig* Agent::CreateTraceConfig() {
  if (categories_.empty())
    return nullptr;
  TraceConfig* trace_config = new TraceConfig();
  for (auto category = categories_.begin();
       category != categories_.end();
       category = categories_.upper_bound(*category)) {
    trace_config->AddIncludedCategory(category->c_str());
  }
  return trace_config;
}

std::string Agent::GetEnabledCategories() {
  std::string categories;
  for (auto category = categories_.begin();
       category != categories_.end();
       category = categories_.upper_bound(*category)) {
    if (!categories.empty())
      categories += ',';
    categories += *category;
  }
  return categories;
}

}  // namespace tracing
}  // namespace node
