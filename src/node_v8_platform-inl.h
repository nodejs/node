#ifndef SRC_NODE_V8_PLATFORM_INL_H_
#define SRC_NODE_V8_PLATFORM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>

#include "env-inl.h"
#include "node.h"
#include "node_options.h"
#include "node_platform.h"
#include "tracing/agent.h"

namespace node {

struct V8Platform {
  bool initialized_ = false;

#if NODE_USE_V8_PLATFORM
  inline void Initialize(int thread_pool_size) {
    CHECK(!initialized_);
    initialized_ = true;
    tracing_agent_ = tracing::Agent::CreateDefault();
    // Only start the tracing agent if we enabled any tracing categories.
    if (!per_process::cli_options->trace_event_categories.empty()) {
      StartTracingAgent();
    }
    // Tracing must be initialized before platform threads are created.
    platform_ = new NodePlatform(thread_pool_size,
                                 tracing_agent_->GetTracingController());
    v8::V8::InitializePlatform(platform_);
  }
  // Make sure V8Platform don not call into Libuv threadpool,
  // see DefaultProcessExitHandlerInternal in environment.cc
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
    tracing_agent_.reset(nullptr);
  }

  inline void DrainVMTasks(v8::Isolate* isolate) {
    platform_->DrainTasks(isolate);
  }

  inline void StartTracingAgent() {
    if (!initialized_) return;
    tracing_agent_->StartTracing(
        per_process::cli_options->trace_event_categories);
  }

  inline void StopTracingAgent() {
    if (!initialized_) return;
    tracing_agent_->StopTracing();
  }

  inline NodePlatform* Platform() { return platform_; }

  std::unique_ptr<tracing::Agent, tracing::Agent::Deleter> tracing_agent_;
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

  inline NodePlatform* Platform() { return nullptr; }
#endif  // !NODE_USE_V8_PLATFORM
};

namespace per_process {
extern struct V8Platform v8_platform;
}

inline void DisposePlatform() {
  per_process::v8_platform.Dispose();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_PLATFORM_INL_H_
