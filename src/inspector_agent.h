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
class NodePlatform;
}  // namespace node

#include "v8.h"

namespace v8_inspector {
class StringView;
}  // namespace v8_inspector

namespace node {
namespace inspector {

class InspectorSessionDelegate {
 public:
  virtual ~InspectorSessionDelegate() = default;
  virtual bool WaitForFrontendMessageWhilePaused() = 0;
  virtual void SendMessageToFrontend(const v8_inspector::StringView& message)
                                     = 0;
};

class InspectorIo;
class NodeInspectorClient;

class Agent {
 public:
  explicit Agent(node::Environment* env);
  ~Agent();

  // Create client_, may create io_ if option enabled
  bool Start(node::NodePlatform* platform, const char* path,
             const DebugOptions& options);
  // Stop and destroy io_
  void Stop();

  bool IsStarted() { return !!client_; }

  // IO thread started, and client connected
  bool IsConnected();


  void WaitForDisconnect();
  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);

  // Async stack traces instrumentation.
  void AsyncTaskScheduled(const v8_inspector::StringView& taskName, void* task,
                          bool recurring);
  void AsyncTaskCanceled(void* task);
  void AsyncTaskStarted(void* task);
  void AsyncTaskFinished(void* task);
  void AllAsyncTasksCanceled();

  void RegisterAsyncHook(v8::Isolate* isolate,
    v8::Local<v8::Function> enable_function,
    v8::Local<v8::Function> disable_function);

  // These methods are called by the WS protocol and JS binding to create
  // inspector sessions.  The inspector responds by using the delegate to send
  // messages back.
  void Connect(InspectorSessionDelegate* delegate);
  void Disconnect();
  void Dispatch(const v8_inspector::StringView& message);
  InspectorSessionDelegate* delegate();

  void RunMessageLoop();
  bool enabled() { return enabled_; }
  void PauseOnNextJavascriptStatement(const std::string& reason);

  // Initialize 'inspector' module bindings
  static void InitInspector(v8::Local<v8::Object> target,
                            v8::Local<v8::Value> unused,
                            v8::Local<v8::Context> context,
                            void* priv);

  InspectorIo* io() {
    return io_.get();
  }

  // Can only be called from the the main thread.
  bool StartIoThread(bool wait_for_connect);

  // Calls StartIoThread() from off the main thread.
  void RequestIoThreadStart();

  DebugOptions& options() { return debug_options_; }
  void ContextCreated(v8::Local<v8::Context> context);

 private:
  node::Environment* parent_env_;
  std::unique_ptr<NodeInspectorClient> client_;
  std::unique_ptr<InspectorIo> io_;
  v8::Platform* platform_;
  bool enabled_;
  std::string path_;
  DebugOptions debug_options_;
  int next_context_number_;

  v8::Persistent<v8::Function> enable_async_hook_function_;
  v8::Persistent<v8::Function> disable_async_hook_function_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_AGENT_H_
