#ifndef SRC_INSPECTOR_AGENT_H_
#define SRC_INSPECTOR_AGENT_H_

#include <memory>

#include <stddef.h>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "node_debug_options.h"

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace v8 {
template <typename V>
class FunctionCallbackInfo;
template<typename T>
class Local;
class Message;
class Platform;
class Value;
}  // namespace v8

namespace v8_inspector {
class StringView;
}  // namespace v8_inspector

namespace node {
namespace inspector {

class InspectorSessionDelegate {
 public:
  virtual bool WaitForFrontendMessage() = 0;
  virtual void OnMessage(const v8_inspector::StringView& message) = 0;
};

class InspectorIo;
class NodeInspectorClient;

class Agent {
 public:
  explicit Agent(node::Environment* env);
  ~Agent();

  bool Start(v8::Platform* platform, const char* path,
             const DebugOptions& options);
  bool StartIoThread();
  void Stop();

  bool IsStarted();
  bool IsConnected();
  void WaitForDisconnect();
  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);
  void Connect(InspectorSessionDelegate* delegate);
  void Disconnect();
  void Dispatch(const v8_inspector::StringView& message);
  void RunMessageLoop();

 private:
  static void CallAndPauseOnStart(const v8::FunctionCallbackInfo<v8::Value>&);
  static void InspectorConsoleCall(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  static void InspectorWrapConsoleCall(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  node::Environment* parent_env_;
  std::unique_ptr<NodeInspectorClient> inspector_;
  std::unique_ptr<InspectorIo> io_;
  v8::Platform* platform_;
  bool inspector_console_;
  std::string path_;
  DebugOptions debug_options_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_AGENT_H_
