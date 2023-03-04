#ifndef SRC_NODE_V8_PLATFORM_INL_H_
#define SRC_NODE_V8_PLATFORM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>

#include "env-inl.h"
#include "node.h"
#include "node_metadata.h"
#include "node_platform.h"
#include "node_options.h"
#include "tracing/node_trace_writer.h"
#include "tracing/trace_event.h"
#include "tracing/traced_value.h"
#include "util.h"

namespace node {

// Ensures that __metadata trace events are only emitted
// when tracing is enabled.
class NodeTraceStateObserver
    : public v8::TracingController::TraceStateObserver {
 public:
  void OnTraceEnabled() override;

  inline void OnTraceDisabled() override {
    // Do nothing here. This should never be called because the
    // observer removes itself when OnTraceEnabled() is called.
    UNREACHABLE();
  };

  explicit NodeTraceStateObserver(v8::TracingController* controller)
      : controller_(controller) {}

  ~NodeTraceStateObserver() override = default;

 private:
  v8::TracingController* controller_;
};

struct V8Platform {
  bool initialized_ = false;

#if NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size);
  void Dispose();
  void StartTracingAgent();

  inline void DrainVMTasks(v8::Isolate* isolate) {
    platform_->DrainTasks(isolate);
  }

  inline void StopTracingAgent() {
    tracing_file_writer_.reset();
  }

  inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
    return &tracing_file_writer_;
  }

  inline NodePlatform* Platform() {
    return platform_;
  }

  std::unique_ptr<NodeTraceStateObserver> trace_state_observer_;
  std::unique_ptr<tracing::Agent> tracing_agent_;
  tracing::AgentWriterHandle tracing_file_writer_;
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
  inline NodePlatform* Platform() {
    return nullptr;
  }

  inline void StopTracingAgent() {}

  inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
    return nullptr;
  }
#endif  // !NODE_USE_V8_PLATFORM
};

namespace per_process {
extern struct V8Platform v8_platform;
}

inline void StartTracingAgent() {
  return per_process::v8_platform.StartTracingAgent();
}

inline tracing::AgentWriterHandle* GetTracingAgentWriter() {
  return per_process::v8_platform.GetTracingAgentWriter();
}

inline void DisposePlatform() {
  per_process::v8_platform.Dispose();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_PLATFORM_INL_H_
