#include "node_v8_platform-inl.h"

#include <memory>

#include "env-inl.h"
#include "node.h"
#include "node_metadata.h"
#include "node_options.h"
#include "node_platform.h"
#include "tracing/node_trace_writer.h"
#include "tracing/trace_event.h"
#include "tracing/traced_value.h"
#include "util.h"

namespace node {

void NodeTraceStateObserver::OnTraceEnabled() {
  std::string title = GetProcessTitle("");
  if (!title.empty()) {
    // Only emit the metadata event if the title can be retrieved
    // successfully. Ignore it otherwise.
    TRACE_EVENT_METADATA1(
        "__metadata", "process_name", "name", TRACE_STR_COPY(title.c_str()));
  }
  TRACE_EVENT_METADATA1("__metadata",
                        "version",
                        "node",
                        per_process::metadata.versions.node.c_str());
  TRACE_EVENT_METADATA1(
      "__metadata", "thread_name", "name", "JavaScriptMainThread");

  auto trace_process = tracing::TracedValue::Create();
  trace_process->BeginDictionary("versions");

#define V(key)                                                                 \
  trace_process->SetString(#key, per_process::metadata.versions.key.c_str());

  NODE_VERSIONS_KEYS(V)
#undef V

  trace_process->EndDictionary();

  trace_process->SetString("arch", per_process::metadata.arch.c_str());
  trace_process->SetString("platform", per_process::metadata.platform.c_str());

  trace_process->BeginDictionary("release");
  trace_process->SetString("name", per_process::metadata.release.name.c_str());
#if NODE_VERSION_IS_LTS
  trace_process->SetString("lts", per_process::metadata.release.lts.c_str());
#endif
  trace_process->EndDictionary();
  TRACE_EVENT_METADATA1(
      "__metadata", "node", "process", std::move(trace_process));

  // This only runs the first time tracing is enabled
  controller_->RemoveTraceStateObserver(this);
}

#if NODE_USE_V8_PLATFORM
void V8Platform::Initialize(int thread_pool_size) {
  CHECK(!initialized_);
  initialized_ = true;
  tracing_agent_ = std::make_unique<tracing::Agent>();
  node::tracing::TraceEventHelper::SetAgent(tracing_agent_.get());
  node::tracing::TracingController* controller =
      tracing_agent_->GetTracingController();
  trace_state_observer_ = std::make_unique<NodeTraceStateObserver>(controller);
  controller->AddTraceStateObserver(trace_state_observer_.get());
  tracing_file_writer_ = tracing_agent_->DefaultHandle();
  // Only start the tracing agent if we enabled any tracing categories.
  if (!per_process::cli_options->trace_event_categories.empty()) {
    StartTracingAgent();
  }
  // Tracing must be initialized before platform threads are created.
  platform_ = new NodePlatform(thread_pool_size, controller);
  v8::V8::InitializePlatform(platform_);
}
// Make sure V8Platform don not call into Libuv threadpool,
// see DefaultProcessExitHandlerInternal in environment.cc
void V8Platform::Dispose() {
  if (!initialized_) return;
  initialized_ = false;
  node::tracing::TraceEventHelper::SetAgent(nullptr);
  StopTracingAgent();
  platform_->Shutdown();
  delete platform_;
  platform_ = nullptr;
  // Destroy tracing after the platform (and platform threads) have been
  // stopped.
  tracing_agent_.reset(nullptr);
  // The observer remove itself in OnTraceEnabled
  trace_state_observer_.reset(nullptr);
}

void V8Platform::StartTracingAgent() {
  // Attach a new NodeTraceWriter only if this function hasn't been called
  // before.
  if (tracing_file_writer_.IsDefaultHandle()) {
    std::vector<std::string> categories =
        SplitString(per_process::cli_options->trace_event_categories, ',');

    tracing_file_writer_ = tracing_agent_->AddClient(
        std::set<std::string>(std::make_move_iterator(categories.begin()),
                              std::make_move_iterator(categories.end())),
        std::unique_ptr<tracing::AsyncTraceWriter>(new tracing::NodeTraceWriter(
            per_process::cli_options->trace_event_file_pattern)),
        tracing::Agent::kUseDefaultCategories);
  }
}
#endif  // !NODE_USE_V8_PLATFORM

}  // namespace node
