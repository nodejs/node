#ifndef SRC_INSPECTOR_IO_H_
#define SRC_INSPECTOR_IO_H_

#include "inspector_socket_server.h"
#include "node_debug_options.h"
#include "node_mutex.h"
#include "uv.h"

#include <deque>
#include <memory>
#include <stddef.h>

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

std::string FormatWsAddress(const std::string& host, int port,
                            const std::string& target_id,
                            bool include_protocol);

class InspectorIoDelegate;

enum class InspectorAction {
  kStartSession,
  kEndSession,
  kSendMessage
};

// kKill closes connections and stops the server, kStop only stops the server
enum class TransportAction {
  kKill,
  kSendMessage,
  kStop
};

class InspectorIo {
 public:
  InspectorIo(node::Environment* env, v8::Platform* platform,
              const std::string& path, const DebugOptions& options,
              bool wait_for_connect);

  ~InspectorIo();
  // Start the inspector agent thread, waiting for it to initialize,
  // and waiting as well for a connection if wait_for_connect.
  bool Start();
  // Stop the inspector agent thread.
  void Stop();

  bool IsStarted();
  bool IsConnected();

  void WaitForDisconnect();
  // Called from thread to queue an incoming message and trigger
  // DispatchMessages() on the main thread.
  void PostIncomingMessage(InspectorAction action, int session_id,
                           const std::string& message);
  void ResumeStartup() {
    uv_sem_post(&thread_start_sem_);
  }
  void ServerDone() {
    uv_close(reinterpret_cast<uv_handle_t*>(&thread_req_), nullptr);
  }

  int port() const { return port_; }
  std::string host() const { return options_.host_name(); }
  std::vector<std::string> GetTargetIds() const;

 private:
  template <typename Action>
  using MessageQueue =
      std::deque<std::tuple<Action, int,
                  std::unique_ptr<v8_inspector::StringBuffer>>>;
  enum class State {
    kNew,
    kAccepting,
    kConnected,
    kDone,
    kError,
    kShutDown
  };

  // Callback for main_thread_req_'s uv_async_t
  static void MainThreadReqAsyncCb(uv_async_t* req);

  // Wrapper for agent->ThreadMain()
  static void ThreadMain(void* agent);

  // Runs a uv_loop_t
  template <typename Transport> void ThreadMain();
  // Called by ThreadMain's loop when triggered by thread_req_, writes
  // messages from outgoing_message_queue to the InspectorSockerServer
  template <typename Transport> static void IoThreadAsyncCb(uv_async_t* async);

  void SetConnected(bool connected);
  void DispatchMessages();
  // Write action to outgoing_message_queue, and wake the thread
  void Write(TransportAction action, int session_id,
             const v8_inspector::StringView& message);
  // Thread-safe append of message to a queue. Return true if the queue
  // used to be empty.
  template <typename ActionType>
  bool AppendMessage(MessageQueue<ActionType>* vector, ActionType action,
                     int session_id,
                     std::unique_ptr<v8_inspector::StringBuffer> buffer);
  // Used as equivalent of a thread-safe "pop" of an entire queue's content.
  template <typename ActionType>
  void SwapBehindLock(MessageQueue<ActionType>* vector1,
                      MessageQueue<ActionType>* vector2);
  // Wait on incoming_message_cond_
  void WaitForFrontendMessageWhilePaused();
  // Broadcast incoming_message_cond_
  void NotifyMessageReceived();

  const DebugOptions options_;

  // The IO thread runs its own uv_loop to implement the TCP server off
  // the main thread.
  uv_thread_t thread_;
  // Used by Start() to wait for thread to initialize, or for it to initialize
  // and receive a connection if wait_for_connect was requested.
  uv_sem_t thread_start_sem_;

  InspectorIoDelegate* delegate_;
  State state_;
  node::Environment* parent_env_;

  // Attached to the uv_loop in ThreadMain()
  uv_async_t thread_req_;
  // Note that this will live while the async is being closed - likely, past
  // the parent object lifespan
  std::pair<uv_async_t, Agent*>* main_thread_req_;
  std::unique_ptr<InspectorSessionDelegate> session_delegate_;
  v8::Platform* platform_;

  // Message queues
  ConditionVariable incoming_message_cond_;
  Mutex state_lock_;  // Locked before mutating either queue.
  MessageQueue<InspectorAction> incoming_message_queue_;
  MessageQueue<TransportAction> outgoing_message_queue_;
  MessageQueue<InspectorAction> dispatching_message_queue_;

  bool dispatching_messages_;
  int session_id_;

  std::string script_name_;
  std::string script_path_;
  const bool wait_for_connect_;
  int port_;

  friend class DispatchMessagesTask;
  friend class IoSessionDelegate;
  friend void InterruptCallback(v8::Isolate*, void* agent);
};

std::unique_ptr<v8_inspector::StringBuffer> Utf8ToStringView(
    const std::string& message);

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_IO_H_
