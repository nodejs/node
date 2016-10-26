#ifndef SRC_INSPECTOR_AGENT_H_
#define SRC_INSPECTOR_AGENT_H_

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "platform/v8_inspector/public/V8Inspector.h"

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace v8 {
class Platform;
template<typename T>
class Local;
class Value;
class Message;
}  // namespace v8

namespace node {
namespace inspector {

class AgentImpl;

class Agent {
 public:
  explicit Agent(node::Environment* env);
  ~Agent();

  // Start the inspector agent thread
  bool Start(v8::Platform* platform, const char* path, int port, bool wait);
  // Stop the inspector agent
  void Stop();

  void ContextCreated(const v8_inspector::V8ContextInfo& info);
  void ContextDestroyed(v8::Local<v8::Context> context);

  bool IsStarted();
  bool IsConnected();
  void WaitForDisconnect();

  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);
 private:
  AgentImpl* impl;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_AGENT_H_
