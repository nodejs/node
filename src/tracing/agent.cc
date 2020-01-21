#include "tracing/agent.h"

#include <string>
#include "trace_event.h"
#include "tracing/node_trace_buffer.h"
#include "debug_utils-inl.h"
#include "env-inl.h"

namespace node {
namespace tracing {

class Agent::ScopedSuspendTracing {
 public:
  ScopedSuspendTracing(TracingController* controller, Agent* agent,
                       bool do_suspend = true)
    : controller_(controller), agent_(do_suspend ? agent : nullptr) {
    if (do_suspend) {
      CHECK(agent_->started_);
      controller->StopTracing();
    }
  }

  ~ScopedSuspendTracing() {
    if (agent_ == nullptr) return;
    TraceConfig* config = agent_->CreateTraceConfig();
    if (config != nullptr) {
      controller_->StartTracing(config);
    }
  }

 private:
  TracingController* controller_;
  Agent* agent_;
};

namespace {

std::set<std::string> flatten(
    const std::unordered_map<int, std::multiset<std::string>>& map) {
  std::set<std::string> result;
  for (const auto& id_value : map)
    result.insert(id_value.second.begin(), id_value.second.end());
  return result;
}

}  // namespace

using v8::platform::tracing::TraceConfig;
using v8::platform::tracing::TraceWriter;
using std::string;

Agent::Agent() : tracing_controller_(new TracingController()) {
  tracing_controller_->Initialize(nullptr);

  CHECK_EQ(uv_loop_init(&tracing_loop_), 0);
  CHECK_EQ(uv_async_init(&tracing_loop_,
                         &initialize_writer_async_,
                         [](uv_async_t* async) {
    Agent* agent = ContainerOf(&Agent::initialize_writer_async_, async);
    agent->InitializeWritersOnThread();
  }), 0);
  uv_unref(reinterpret_cast<uv_handle_t*>(&initialize_writer_async_));
}

void Agent::InitializeWritersOnThread() {
  Mutex::ScopedLock lock(initialize_writer_mutex_);
  while (!to_be_initialized_.empty()) {
    AsyncTraceWriter* head = *to_be_initialized_.begin();
    head->InitializeOnThread(&tracing_loop_);
    to_be_initialized_.erase(head);
  }
  initialize_writer_condvar_.Broadcast(lock);
}

Agent::~Agent() {
  categories_.clear();
  writers_.clear();

  StopTracing();

  uv_close(reinterpret_cast<uv_handle_t*>(&initialize_writer_async_), nullptr);
  uv_run(&tracing_loop_, UV_RUN_ONCE);
  CheckedUvLoopClose(&tracing_loop_);
}

void Agent::Start() {
  if (started_)
    return;

  NodeTraceBuffer* trace_buffer_ = new NodeTraceBuffer(
      NodeTraceBuffer::kBufferChunks, this, &tracing_loop_);
  tracing_controller_->Initialize(trace_buffer_);

  // This thread should be created *after* async handles are created
  // (within NodeTraceWriter and NodeTraceBuffer constructors).
  // Otherwise the thread could shut down prematurely.
  CHECK_EQ(0, uv_thread_create(&thread_, [](void* arg) {
    Agent* agent = static_cast<Agent*>(arg);
    uv_run(&agent->tracing_loop_, UV_RUN_DEFAULT);
  }, this));
  started_ = true;
}

AgentWriterHandle Agent::AddClient(
    const std::set<std::string>& categories,
    std::unique_ptr<AsyncTraceWriter> writer,
    enum UseDefaultCategoryMode mode) {
  Start();

  const std::set<std::string>* use_categories = &categories;

  std::set<std::string> categories_with_default;
  if (mode == kUseDefaultCategories) {
    categories_with_default.insert(categories.begin(), categories.end());
    categories_with_default.insert(categories_[kDefaultHandleId].begin(),
                                   categories_[kDefaultHandleId].end());
    use_categories = &categories_with_default;
  }

  ScopedSuspendTracing suspend(tracing_controller_.get(), this);
  int id = next_writer_id_++;
  AsyncTraceWriter* raw = writer.get();
  writers_[id] = std::move(writer);
  categories_[id] = { use_categories->begin(), use_categories->end() };

  {
    Mutex::ScopedLock lock(initialize_writer_mutex_);
    to_be_initialized_.insert(raw);
    uv_async_send(&initialize_writer_async_);
    while (to_be_initialized_.count(raw) > 0)
      initialize_writer_condvar_.Wait(lock);
  }

  return AgentWriterHandle(this, id);
}

AgentWriterHandle Agent::DefaultHandle() {
  return AgentWriterHandle(this, kDefaultHandleId);
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
  if (client == kDefaultHandleId) return;
  {
    Mutex::ScopedLock lock(initialize_writer_mutex_);
    to_be_initialized_.erase(writers_[client].get());
  }
  ScopedSuspendTracing suspend(tracing_controller_.get(), this);
  writers_.erase(client);
  categories_.erase(client);
}

void Agent::Enable(int id, const std::set<std::string>& categories) {
  if (categories.empty())
    return;

  ScopedSuspendTracing suspend(tracing_controller_.get(), this,
                               id != kDefaultHandleId);
  categories_[id].insert(categories.begin(), categories.end());
}

void Agent::Disable(int id, const std::set<std::string>& categories) {
  ScopedSuspendTracing suspend(tracing_controller_.get(), this,
                               id != kDefaultHandleId);
  std::multiset<std::string>& writer_categories = categories_[id];
  for (const std::string& category : categories) {
    auto it = writer_categories.find(category);
    if (it != writer_categories.end())
      writer_categories.erase(it);
  }
}

TraceConfig* Agent::CreateTraceConfig() const {
  if (categories_.empty())
    return nullptr;
  TraceConfig* trace_config = new TraceConfig();
  for (const auto& category : flatten(categories_)) {
    trace_config->AddIncludedCategory(category.c_str());
  }
  return trace_config;
}

std::string Agent::GetEnabledCategories() const {
  std::string categories;
  for (const std::string& category : flatten(categories_)) {
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

void Agent::AddMetadataEvent(std::unique_ptr<TraceObject> event) {
  Mutex::ScopedLock lock(metadata_events_mutex_);
  metadata_events_.push_back(std::move(event));
}

void Agent::Flush(bool blocking) {
  {
    Mutex::ScopedLock lock(metadata_events_mutex_);
    for (const auto& event : metadata_events_)
      AppendTraceEvent(event.get());
  }

  for (const auto& id_writer : writers_)
    id_writer.second->Flush(blocking);
}

void TracingController::AddMetadataEvent(
    const unsigned char* category_group_enabled,
    const char* name,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* convertable_values,
    unsigned int flags) {
  std::unique_ptr<TraceObject> trace_event(new TraceObject);
  trace_event->Initialize(
      TRACE_EVENT_PHASE_METADATA, category_group_enabled, name,
      node::tracing::kGlobalScope,  // scope
      node::tracing::kNoId,         // id
      node::tracing::kNoId,         // bind_id
      num_args, arg_names, arg_types, arg_values, convertable_values,
      TRACE_EVENT_FLAG_NONE,
      CurrentTimestampMicroseconds(),
      CurrentCpuTimestampMicroseconds());
  node::tracing::TraceEventHelper::GetAgent()->AddMetadataEvent(
      std::move(trace_event));
}

}  // namespace tracing
}  // namespace node
