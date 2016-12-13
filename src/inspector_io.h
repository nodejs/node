#ifndef SRC_INSPECTOR_IO_H_
#define SRC_INSPECTOR_IO_H_

#include "inspector_socket_server.h"
#include "node_debug_options.h"
#include "node_mutex.h"
#include "uv.h"

#include <memory>
#include <stddef.h>
#include <vector>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif


// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace v8_inspector {
class StringBuffer;
class StringView;
}  // namespace v8_inspector

namespace node {
namespace inspector {

class InspectorIoDelegate;

enum class InspectorAction {
  kStartSession, kEndSession, kSendMessage
};

enum class TransportAction {
  kSendMessage, kStop
};

class InspectorIo {
 public:
  InspectorIo(node::Environment* env, v8::Platform* platform,
              const std::string& path, const DebugOptions& options);

  // Start the inspector agent thread
  bool Start();
  // Stop the inspector agent
  void Stop();

  bool IsStarted();
  bool IsConnected();
  void WaitForDisconnect();

  void PostIncomingMessage(InspectorAction action, int session_id,
                           const std::string& message);
  void ResumeStartup() {
    uv_sem_post(&start_sem_);
  }

 private:
  template <typename Action>
  using MessageQueue =
      std::vector<std::tuple<Action, int,
                  std::unique_ptr<v8_inspector::StringBuffer>>>;
  enum class State { kNew, kAccepting, kConnected, kDone, kError };

  static void ThreadCbIO(void* agent);
  static void MainThreadAsyncCb(uv_async_t* req);

  template <typename Transport> static void WriteCbIO(uv_async_t* async);
  template <typename Transport> void WorkerRunIO();
  void SetConnected(bool connected);
  void DispatchMessages();
  void Write(TransportAction action, int session_id,
             const v8_inspector::StringView& message);
  template <typename ActionType>
  bool AppendMessage(MessageQueue<ActionType>* vector, ActionType action,
                     int session_id,
                     std::unique_ptr<v8_inspector::StringBuffer> buffer);
  template <typename ActionType>
  void SwapBehindLock(MessageQueue<ActionType>* vector1,
                      MessageQueue<ActionType>* vector2);
  void WaitForFrontendMessage();
  void NotifyMessageReceived();
  bool StartThread(bool wait);

  // Message queues
  ConditionVariable incoming_message_cond_;

  const DebugOptions options_;
  uv_sem_t start_sem_;
  Mutex state_lock_;
  uv_thread_t thread_;

  InspectorIoDelegate* delegate_;
  bool shutting_down_;
  State state_;
  node::Environment* parent_env_;

  uv_async_t io_thread_req_;
  uv_async_t main_thread_req_;
  std::unique_ptr<InspectorSessionDelegate> session_delegate_;
  v8::Platform* platform_;
  MessageQueue<InspectorAction> incoming_message_queue_;
  MessageQueue<TransportAction> outgoing_message_queue_;
  bool dispatching_messages_;
  int session_id_;

  std::string script_name_;
  std::string script_path_;
  const std::string id_;

  friend class DispatchOnInspectorBackendTask;
  friend class IoSessionDelegate;
  friend void InterruptCallback(v8::Isolate*, void* agent);
};

std::unique_ptr<v8_inspector::StringBuffer> Utf8ToStringView(
    const std::string& message);

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_IO_H_
