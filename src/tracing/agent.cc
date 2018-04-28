#include "tracing/agent.h"

#include <sstream>
#include <string>
#include "tracing/node_trace_buffer.h"
#include "tracing/node_trace_writer.h"

namespace node {
namespace tracing {

namespace {

class ScopedSuspendTracing {
 public:
  ScopedSuspendTracing(TracingController* controller, Agent* agent)
                       : controller_(controller), agent_(agent) {
    controller->StopTracing();
  }

  ~ScopedSuspendTracing() {
    TraceConfig* config = agent_->CreateTraceConfig();
    if (config != nullptr) {
      controller_->StartTracing(config);
    }
  }

 private:
  TracingController* controller_;
  Agent* agent_;
};

std::set<std::string> flatten(
    const std::unordered_map<int, std::set<std::string>>& map) {
  std::set<std::string> result;
  for (const auto& id_value : map)
    result.insert(id_value.second.begin(), id_value.second.end());
  return result;
}

}  // namespace

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceWriter;
using std::string;

Agent::Agent(const std::string& log_file_pattern)
    : log_file_pattern_(log_file_pattern), file_writer_(EmptyClientHandle()) {
  tracing_controller_ = new TracingController();
  tracing_controller_->Initialize(nullptr);
}

void Agent::Start() {
  if (started_)
    return;

  CHECK_EQ(uv_loop_init(&tracing_loop_), 0);

  NodeTraceBuffer* trace_buffer_ = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, this, &tracing_loop_);
  tracing_controller_->Initialize(trace_buffer_);

  // This thread should be created *after* async handles are created
  // (within NodeTraceWriter and NodeTraceBuffer constructors).
  // Otherwise the thread could shut down prematurely.
  CHECK_EQ(0, uv_thread_create(&thread_, ThreadCb, this));
  started_ = true;
}

Agent::ClientHandle Agent::AddClient(const std::set<std::string>& categories,
                                     std::unique_ptr<AsyncTraceWriter> writer) {
  Start();
  ScopedSuspendTracing suspend(tracing_controller_, this);
  int id = next_writer_id_++;
  writers_[id] = std::move(writer);
  categories_[id] = categories;

  auto client_id = new std::pair<Agent*, int>(this, id);
  return ClientHandle(client_id, &DisconnectClient);
}

void Agent::Stop() {
  file_writer_.reset();
}

void Agent::StopTracing() {
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

void Agent::Disconnect(int client) {
  ScopedSuspendTracing suspend(tracing_controller_, this);
  writers_.erase(client);
  categories_.erase(client);
}

// static
void Agent::ThreadCb(void* arg) {
  Agent* agent = static_cast<Agent*>(arg);
  uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
}

void Agent::Enable(const std::string& categories) {
  if (categories.empty())
    return;
  std::set<std::string> categories_set;
  std::stringstream category_list(categories);
  while (category_list.good()) {
    std::string category;
    getline(category_list, category, ',');
    categories_set.insert(category);
  }
  Enable(categories_set);
}

void Agent::Enable(const std::set<std::string>& categories) {
  std::string cats;
  for (const std::string cat : categories)
    cats += cat + ", ";
  if (categories.empty())
    return;

  file_writer_categories_.insert(categories.begin(), categories.end());
  std::set<std::string> full_list(file_writer_categories_.begin(),
                                  file_writer_categories_.end());
  if (!file_writer_) {
    // Ensure background thread is running
    Start();
    std::unique_ptr<NodeTraceWriter> writer(
        new NodeTraceWriter(log_file_pattern_, &tracing_loop_));
    file_writer_ = AddClient(full_list, std::move(writer));
  } else {
    ScopedSuspendTracing suspend(tracing_controller_, this);
    categories_[file_writer_->second] = full_list;
  }
}

void Agent::Disable(const std::set<std::string>& categories) {
  for (auto category : categories) {
    auto it = file_writer_categories_.find(category);
    if (it != file_writer_categories_.end())
      file_writer_categories_.erase(it);
  }
  if (!file_writer_)
    return;
  ScopedSuspendTracing suspend(tracing_controller_, this);
  categories_[file_writer_->second] = { file_writer_categories_.begin(),
                                        file_writer_categories_.end() };
}

TraceConfig* Agent::CreateTraceConfig() {
  if (categories_.empty())
    return nullptr;
  TraceConfig* trace_config = new TraceConfig();
  for (const auto& category : flatten(categories_)) {
    trace_config->AddIncludedCategory(category.c_str());
  }
  return trace_config;
}

std::string Agent::GetEnabledCategories() {
  std::string categories;
  for (const auto& category : flatten(categories_)) {
    if (!categories.empty())
      categories += ',';
    categories += category;
  }
  return categories;
}

void Agent::AppendTraceEvent(TraceObject* trace_event) {
  for (const auto& id_writer : writers_)
    id_writer.second->AppendTraceEvent(trace_event);
}

void Agent::Flush(bool blocking) {
  for (const auto& id_writer : writers_)
    id_writer.second->Flush(blocking);
}
}  // namespace tracing
}  // namespace node
