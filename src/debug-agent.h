// Copyright Fedor Indutny and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_DEBUG_AGENT_H_
#define SRC_DEBUG_AGENT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_mutex.h"
#include "node_debug_options.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"
#include "v8-debug.h"

#include <string.h>
#include <string>

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace node {
namespace debugger {

class AgentMessage {
 public:
  AgentMessage(uint16_t* val, int length) : length_(length) {
    if (val == nullptr) {
      data_ = val;
    } else {
      data_ = new uint16_t[length];
      memcpy(data_, val, length * sizeof(*data_));
    }
  }

  ~AgentMessage() {
    delete[] data_;
    data_ = nullptr;
  }

  inline const uint16_t* data() const { return data_; }
  inline int length() const { return length_; }

  ListNode<AgentMessage> member;

 private:
  uint16_t* data_;
  int length_;
};

class Agent {
 public:
  explicit Agent(node::Environment* env);
  ~Agent();

  typedef void (*DispatchHandler)(node::Environment* env);

  // Start the debugger agent thread
  bool Start(const DebugOptions& options);
  // Listen for debug events
  void Enable();
  // Stop the debugger agent
  void Stop();

  inline void set_dispatch_handler(DispatchHandler handler) {
    dispatch_handler_ = handler;
  }

  inline node::Environment* parent_env() const { return parent_env_; }
  inline node::Environment* child_env() const { return child_env_; }

 protected:
  void InitAdaptor(Environment* env);

  // Worker body
  void WorkerRun();

  static void ThreadCb(Agent* agent);
  static void ParentSignalCb(uv_async_t* signal);
  static void ChildSignalCb(uv_async_t* signal);
  static void MessageHandler(const v8::Debug::Message& message);

  // V8 API
  static Agent* Unwrap(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NotifyListen(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NotifyWait(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SendCommand(const v8::FunctionCallbackInfo<v8::Value>& args);

  void EnqueueMessage(AgentMessage* message);

  enum State {
    kNone,
    kRunning
  };

  State state_;
  DebugOptions options_;

  bool wait_;

  uv_sem_t start_sem_;
  node::Mutex message_mutex_;
  uv_async_t child_signal_;

  uv_thread_t thread_;
  node::Environment* parent_env_;
  node::Environment* child_env_;
  uv_loop_t child_loop_;
  v8::Persistent<v8::Object> api_;

  ListHead<AgentMessage, &AgentMessage::member> messages_;

  DispatchHandler dispatch_handler_;
};

}  // namespace debugger
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_DEBUG_AGENT_H_
