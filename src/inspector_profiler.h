#ifndef SRC_INSPECTOR_PROFILER_H_
#define SRC_INSPECTOR_PROFILER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "env.h"
#include "inspector_agent.h"

namespace node {
// Forward declaration to break recursive dependency chain with src/env.h.
class Environment;

namespace profiler {

class V8ProfilerConnection {
 public:
  class V8ProfilerSessionDelegate : public inspector::InspectorSessionDelegate {
   public:
    explicit V8ProfilerSessionDelegate(V8ProfilerConnection* connection)
        : connection_(connection) {}

    void SendMessageToFrontend(
        const v8_inspector::StringView& message) override {
      connection_->OnMessage(message);
    }

   private:
    V8ProfilerConnection* connection_;
  };

  explicit V8ProfilerConnection(Environment* env);
  virtual ~V8ProfilerConnection() = default;
  Environment* env() { return env_; }

  // Use DispatchMessage() to dispatch necessary inspector messages
  virtual void Start() = 0;
  virtual void End() = 0;
  // Override this to respond to the messages sent from the session.
  virtual void OnMessage(const v8_inspector::StringView& message) = 0;
  virtual bool ending() const = 0;

  void DispatchMessage(v8::Local<v8::String> message);
  // Write the result to a path
  bool WriteResult(const char* path, v8::Local<v8::String> result);

 private:
  std::unique_ptr<inspector::InspectorSession> session_;
  Environment* env_ = nullptr;
};

class V8CoverageConnection : public V8ProfilerConnection {
 public:
  explicit V8CoverageConnection(Environment* env) : V8ProfilerConnection(env) {}

  void Start() override;
  void End() override;
  void OnMessage(const v8_inspector::StringView& message) override;
  bool ending() const override { return ending_; }

 private:
  bool WriteCoverage(v8::Local<v8::String> message);
  v8::MaybeLocal<v8::String> GetResult(v8::Local<v8::String> message);
  std::unique_ptr<inspector::InspectorSession> session_;
  bool ending_ = false;
};

}  // namespace profiler
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_INSPECTOR_PROFILER_H_
