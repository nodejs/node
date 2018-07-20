#ifndef SRC_INSPECTOR_MAIN_THREAD_INTERFACE_H_
#define SRC_INSPECTOR_MAIN_THREAD_INTERFACE_H_

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "env.h"
#include "inspector_agent.h"
#include "node_mutex.h"

#include <deque>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace v8_inspector {
class StringBuffer;
class StringView;
}  // namespace v8_inspector

namespace node {
namespace inspector {
class Request {
 public:
  virtual void Call() = 0;
  virtual ~Request() {}
};

std::unique_ptr<v8_inspector::StringBuffer> Utf8ToStringView(
    const std::string& message);

using MessageQueue = std::deque<std::unique_ptr<Request>>;
class MainThreadInterface;

class MainThreadHandle : public std::enable_shared_from_this<MainThreadHandle> {
 public:
  explicit MainThreadHandle(MainThreadInterface* main_thread)
                            : main_thread_(main_thread) {}
  ~MainThreadHandle() {
    CHECK_NULL(main_thread_);  // main_thread_ should have called Reset
  }
  std::unique_ptr<InspectorSession> Connect(
      std::unique_ptr<InspectorSessionDelegate> delegate,
      bool prevent_shutdown);
  bool Post(std::unique_ptr<Request> request);
  Agent* GetInspectorAgent();
  std::unique_ptr<InspectorSessionDelegate> MakeThreadSafeDelegate(
      std::unique_ptr<InspectorSessionDelegate> delegate);
  bool Expired();

 private:
  void Reset();

  MainThreadInterface* main_thread_;
  Mutex block_lock_;
  int next_session_id_ = 0;

  friend class MainThreadInterface;
};

class MainThreadInterface {
 public:
  MainThreadInterface(Agent* agent, uv_loop_t*, v8::Isolate* isolate,
                      v8::Platform* platform);
  ~MainThreadInterface();

  void DispatchMessages();
  void Post(std::unique_ptr<Request> request);
  bool WaitForFrontendEvent();
  std::shared_ptr<MainThreadHandle> GetHandle();
  Agent* inspector_agent() {
    return agent_;
  }

 private:
  using AsyncAndInterface = std::pair<uv_async_t, MainThreadInterface*>;

  static void DispatchMessagesAsyncCallback(uv_async_t* async);
  static void CloseAsync(AsyncAndInterface*);

  MessageQueue requests_;
  Mutex requests_lock_;   // requests_ live across threads
  // This queue is to maintain the order of the messages for the cases
  // when we reenter the DispatchMessages function.
  MessageQueue dispatching_message_queue_;
  bool dispatching_messages_ = false;
  ConditionVariable incoming_message_cond_;
  // Used from any thread
  Agent* const agent_;
  v8::Isolate* const isolate_;
  v8::Platform* const platform_;
  DeleteFnPtr<AsyncAndInterface, CloseAsync> main_thread_request_;
  std::shared_ptr<MainThreadHandle> handle_;
};

}  // namespace inspector
}  // namespace node
#endif  // SRC_INSPECTOR_MAIN_THREAD_INTERFACE_H_
