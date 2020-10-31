#ifndef SRC_NODE_V8_PLATFORM_INL_H_
#define SRC_NODE_V8_PLATFORM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>

#include "env-inl.h"
#include "node.h"
#include "node_metadata.h"
#include "node_platform.h"
#include "node_options.h"
#include "util.h"

#ifndef V8_USE_PERFETTO
#include "tracing/node_trace_writer.h"
#include "tracing/trace_event.h"
#include "tracing/traced_value.h"
#else
#include "tracing/tracing.h"
#include "perfetto/tracing.h"
#include "protos/perfetto/trace/track_event/nodejs.pbzero.h"
#include "protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#endif

namespace node {

#ifndef V8_USE_PERFETTO
// Ensures that __metadata trace events are only emitted
// when tracing is enabled.
class NodeTraceStateObserver
    : public v8::TracingController::TraceStateObserver {
 public:
  inline void OnTraceEnabled() override {
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
    trace_process->SetString("platform",
                             per_process::metadata.platform.c_str());

    trace_process->BeginDictionary("release");
    trace_process->SetString("name",
                             per_process::metadata.release.name.c_str());
#if NODE_VERSION_IS_LTS
    trace_process->SetString("lts", per_process::metadata.release.lts.c_str());
#endif
    trace_process->EndDictionary();

    TRACE_EVENT_METADATA1(
        "__metadata", "node", "process", std::move(trace_process));

    // This only runs the first time tracing is enabled
    controller_->RemoveTraceStateObserver(this);
  }

  inline void OnTraceDisabled() override {
    // Do nothing here. This should never be called because the
    // observer removes itself when OnTraceEnabled() is called.
    UNREACHABLE();
  }

  explicit NodeTraceStateObserver(v8::TracingController* controller)
      : controller_(controller) {}
  ~NodeTraceStateObserver() override = default;

 private:
  v8::TracingController* controller_;
};
#else
static void TraceProcessMetadata() {
  auto fn = [&](perfetto::EventContext ctx) {
        auto metadata = ctx.event()->set_node_process_metadata();
        metadata->set_title(GetProcessTitle(""));
        metadata->set_version(per_process::metadata.versions.node);
        metadata->set_arch(per_process::metadata.arch);
        metadata->set_platform(per_process::metadata.platform);
        metadata->set_release(per_process::metadata.release.name);
#if NODE_VERSION_IS_LTS
        metadata->set_lts(per_process::metadata.release.lts);
#endif  // NODE_VERSION_IS_LTS
#define V(key)                                                                 \
  do {                                                                         \
    auto version = metadata->add_versions();                                   \
    version->set_component(#key);                                              \
    version->set_version(per_process::metadata.versions.key);                  \
  } while (0);
        NODE_VERSIONS_KEYS(V)
#undef V
      };
  TRACE_EVENT_INSTANT("node,node.process", "process", fn);
}
#endif  // V8_USE_PERFETTO

struct V8Platform {
  bool initialized_ = false;

#if NODE_USE_V8_PLATFORM
  inline void Initialize(int thread_pool_size) {
    CHECK(!initialized_);
    initialized_ = true;
#ifndef V8_USE_PERFETTO
    tracing_agent_ = std::make_unique<tracing::Agent>();
    node::tracing::TraceEventHelper::SetAgent(tracing_agent_.get());
    node::tracing::TracingController* controller =
        tracing_agent_->GetTracingController();
    trace_state_observer_ =
        std::make_unique<NodeTraceStateObserver>(controller);
    controller->AddTraceStateObserver(trace_state_observer_.get());
    tracing_file_writer_ = tracing_agent_->DefaultHandle();
#else
    tracing::TracingService::Options tracing_service_options;
    // TODO(@jasnell): Configure the options
    tracing_service_.Init(tracing_service_options);
    v8::TracingController* controller = tracing_service_.tracing_controller();
    node::tracing::SetProcessTrackTitle(GetProcessTitle(""));
    node::tracing::SetThreadTrackTitle("JavaScriptMainThread");
#endif
    // Only start the tracing agent if we enabled any tracing categories.
    if (!per_process::cli_options->trace_event_categories.empty()) {
      StartTracingAgent();
    }
    // Tracing must be initialized before platform threads are created.
    platform_ = new NodePlatform(thread_pool_size, controller);
    v8::V8::InitializePlatform(platform_);
  }

  inline void Dispose() {
    if (!initialized_)
      return;
    initialized_ = false;

    StopTracingAgent();
    platform_->Shutdown();
    delete platform_;
    platform_ = nullptr;
    // Destroy tracing after the platform (and platform threads) have been
    // stopped.
#ifndef V8_USE_PERFETTO
    tracing_agent_.reset(nullptr);
    trace_state_observer_.reset(nullptr);
#else
    tracing_session_.reset();
#endif
  }

  inline void DrainVMTasks(v8::Isolate* isolate) {
    platform_->DrainTasks(isolate);
  }

  inline void StartTracingAgent() {
#ifndef V8_USE_PERFETTO
    // Attach a new NodeTraceWriter only if this function hasn't been called
    // before.
    if (tracing_file_writer_.IsDefaultHandle()) {
      std::vector<std::string> categories =
          SplitString(per_process::cli_options->trace_event_categories, ',');

      tracing_file_writer_ = tracing_agent_->AddClient(
          std::set<std::string>(std::make_move_iterator(categories.begin()),
                                std::make_move_iterator(categories.end())),
          std::unique_ptr<tracing::AsyncTraceWriter>(
              new tracing::NodeTraceWriter(
                  per_process::cli_options->trace_event_file_pattern)),
          tracing::Agent::kUseDefaultCategories);
    }
#else
    if (!tracing_session_) {
      tracing::FileTracingSession::Options session_options;
      session_options.enabled_categories =
          SplitString(per_process::cli_options->trace_event_categories, ',');
      session_options.settings.file_pattern =
          per_process::cli_options->trace_event_file_pattern;
      tracing_session_ =
          std::make_unique<tracing::FileTracingSession>(
              &tracing_service_,
              session_options);
      tracing_service_.AddTracingSession(tracing_session_.get());
      tracing_service_.StartTracing();
      TraceProcessMetadata();
    }
#endif
  }

  inline void StopTracingAgent() {
#ifndef V8_USE_PERFETTO
    tracing_file_writer_.reset();
#else
    tracing_service_.StopTracing();
    if (tracing_session_)
      tracing_session_.reset();
#endif
  }

#ifndef V8_USE_PERFETTO
  inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
    return &tracing_file_writer_;
  }
#endif

  inline NodePlatform* Platform() { return platform_; }

#ifndef V8_USE_PERFETTO
  std::unique_ptr<NodeTraceStateObserver> trace_state_observer_;
  std::unique_ptr<tracing::Agent> tracing_agent_;
  tracing::AgentWriterHandle tracing_file_writer_;
#else
  tracing::TracingService tracing_service_;
  std::unique_ptr<tracing::TracingSessionBase> tracing_session_;
#endif
  NodePlatform* platform_;
#else   // !NODE_USE_V8_PLATFORM
  inline void Initialize(int thread_pool_size) {}
  inline void Dispose() {}
  inline void DrainVMTasks(v8::Isolate* isolate) {}
  inline void StartTracingAgent() {
    if (!per_process::cli_options->trace_event_categories.empty()) {
      fprintf(stderr,
              "Node compiled with NODE_USE_V8_PLATFORM=0, "
              "so event tracing is not available.\n");
    }
  }
  inline void StopTracingAgent() {}

#ifndef V8_USE_PERFETTO
  inline tracing::AgentWriterHandle* GetTracingAgentWriter() { return nullptr; }
#endif

  inline NodePlatform* Platform() { return nullptr; }
#endif  // !NODE_USE_V8_PLATFORM
};

namespace per_process {
extern struct V8Platform v8_platform;
}

inline void StartTracingAgent() {
  return per_process::v8_platform.StartTracingAgent();
}

#ifndef V8_USE_PERFETTO
inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
  return per_process::v8_platform.GetTracingAgentWriter();
}
#endif

inline void DisposePlatform() {
  per_process::v8_platform.Dispose();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_PLATFORM_INL_H_
