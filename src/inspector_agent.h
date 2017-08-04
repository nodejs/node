#ifndef SRC_INSPECTOR_AGENT_H_
#define SRC_INSPECTOR_AGENT_H_

#include <memory>
#include <vector>

#include <stddef.h>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "v8.h"
#include "node_debug_options.h"

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace v8 {
class Context;
template <typename V>
class FunctionCallbackInfo;
template<typename T>
class Local;
class Message;
class Object;
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
  virtual ~InspectorSessionDelegate() = default;
  virtual bool WaitForFrontendMessageWhilePaused() = 0;
  virtual void SendMessageToFrontend(const v8_inspector::StringView& message)
                                     = 0;
};

class InspectorIo;
class NodeInspectorClient;

class ContextInfo {
 public:
  explicit ContextInfo(v8::Local<v8::Context> context, const int group_id,
                       const char* name, const char* origin = nullptr,
                       const char* aux_data = nullptr)
                       : group_id_(group_id),
                         name_(name),
                         origin_(origin),
                         aux_data_(aux_data) {
    context_.Reset(context->GetIsolate(), context);
  }

  inline v8::Local<v8::Context> context(v8::Isolate* isolate) const {
    return context_.Get(isolate);
  }

  int group_id() const { return group_id_; }
  const char* name() const { return name_; }
  const char* origin() const { return origin_; }
  const char* aux_data() const { return aux_data_; }

 private:
  v8::Persistent<v8::Context> context_;
  const int group_id_;
  const char* name_;
  const char* origin_;
  const char* aux_data_;
};

class Agent {
 public:
  explicit Agent(node::Environment* env);
  ~Agent();

  // Create client_, may create io_ if option enabled
  bool Start(v8::Platform* platform, const char* path,
             const DebugOptions& options);
  // Stop and destroy io_
  void Stop();

  bool ContextRegistered(v8::Local<v8::Context> context);
  bool ContextCreated(const node::inspector::ContextInfo* info);
  void ContextDestroyed(v8::Local<v8::Context> context);

  bool IsStarted() { return !!client_; }

  // IO thread started, and client connected
  bool IsConnected();


  void WaitForDisconnect();
  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);

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

 private:
  node::Environment* parent_env_;
  std::unique_ptr<NodeInspectorClient> client_;
  std::unique_ptr<InspectorIo> io_;
  v8::Platform* platform_;
  std::vector<const node::inspector::ContextInfo*> contexts_;
  bool enabled_;
  std::string path_;
  DebugOptions debug_options_;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_AGENT_H_
