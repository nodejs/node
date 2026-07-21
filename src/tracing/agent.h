#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

// This defines a compatible agent interface for both the legacy V8
// tracing and the Perfetto tracing. The interface is designed to be
// used by the rest of the Node.js codebase that only generates
// trace events with the trace macros, or interacts with the tracing
// system.
//
// To generate trace events by API, or consume the tracing data, the
// caller should directly refer to the derived trace agent interfaces.
//
// This file should not include either `libplatform/v8-tracing.h` or
// `perfetto.h`.

#include "v8-platform.h"

#include <memory>
#include <set>
#include <string>

namespace node {
namespace tracing {

class Agent;
class AgentWriterHandle;

class Agent {
 public:
  virtual ~Agent() = default;

  virtual v8::TracingController* GetTracingController() = 0;

  // Returns a comma-separated list of enabled categories.
  virtual std::string GetEnabledCategories() const = 0;

  // Called by V8Platform to start/stop file-based tracing.
  virtual void StartTracing(const std::string& categories) = 0;
  virtual void StopTracing() = 0;

  virtual AgentWriterHandle* GetDefaultWriterHandle() = 0;

  virtual void AddTraceStateObserver(
      v8::TracingController::TraceStateObserver* observer) = 0;
  virtual void RemoveTraceStateObserver(
      v8::TracingController::TraceStateObserver* observer) = 0;

  struct Deleter {
    void operator()(Agent* agent);
  };
  static std::unique_ptr<Agent, Deleter> CreateDefault();
  static Agent* GetInstance();

 private:
  virtual void Disconnect(int client) = 0;

  virtual void Enable(int id, const std::set<std::string>& categories) = 0;
  virtual void Disable(int id, const std::set<std::string>& categories) = 0;

  friend class AgentWriterHandle;
};

class AgentWriterHandle {
 public:
  inline AgentWriterHandle() = default;
  inline ~AgentWriterHandle() { reset(); }

  inline AgentWriterHandle(AgentWriterHandle&& other);
  inline AgentWriterHandle& operator=(AgentWriterHandle&& other);
  inline bool empty() const { return agent_ == nullptr; }
  inline void reset();

  inline void Enable(const std::set<std::string>& categories);
  inline void Disable(const std::set<std::string>& categories);

  inline Agent* agent() { return agent_; }

  AgentWriterHandle(const AgentWriterHandle& other) = delete;
  AgentWriterHandle& operator=(const AgentWriterHandle& other) = delete;

 private:
  inline AgentWriterHandle(Agent* agent, int id) : agent_(agent), id_(id) {}

  Agent* agent_ = nullptr;
  int id_ = 0;

  friend class Agent;
  friend class LegacyTracingAgent;
};

void AgentWriterHandle::reset() {
  if (agent_ != nullptr)
    agent_->Disconnect(id_);
  agent_ = nullptr;
}

AgentWriterHandle& AgentWriterHandle::operator=(AgentWriterHandle&& other) {
  reset();
  agent_ = other.agent_;
  id_ = other.id_;
  other.agent_ = nullptr;
  return *this;
}

AgentWriterHandle::AgentWriterHandle(AgentWriterHandle&& other) {
  *this = std::move(other);
}

void AgentWriterHandle::Enable(const std::set<std::string>& categories) {
  if (agent_ != nullptr) agent_->Enable(id_, categories);
}

void AgentWriterHandle::Disable(const std::set<std::string>& categories) {
  if (agent_ != nullptr) agent_->Disable(id_, categories);
}

}  // namespace tracing
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TRACING_AGENT_H_
