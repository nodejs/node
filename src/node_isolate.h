// Copyright Joyent, Inc. and other Node contributors.
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

#ifndef SRC_NODE_ISOLATE_H_
#define SRC_NODE_ISOLATE_H_

#include "v8.h"
#include "v8-debug.h"
#include "uv.h"
#include "node_vars.h"
#include "node_object_wrap.h"
#include "ngx-queue.h"

#ifdef NDEBUG
# define NODE_ISOLATE_CHECK(ptr) ((void) (ptr))
#else
# include <assert.h>
# define NODE_ISOLATE_CHECK(ptr)                        \
  do {                                                  \
    node::Isolate* data_ = node::Isolate::GetCurrent(); \
    assert(data_ == (ptr));                             \
  }                                                     \
  while (0)
#endif


namespace node {

template <class T>

class Channel;

class IsolateWrap;
class IsolateChannel;
class IsolateMessage;
class IsolateDebugger;
class IsolateDebuggerMessage;

class Isolate {
public:
  char** argv_;
  int argc_;
  uv_thread_t tid_;

  enum {
    kNone,
    kDebug,
    kDebugBrk
  } debug_state;
  IsolateDebugger* debugger_instance;

  // Call this before instantiating any Isolate
  static void Initialize();
  static int Count();

  typedef void (*AtExitCallback)(void* arg);

  static v8::Handle<v8::Value> Send(const v8::Arguments& args);

  static Isolate* GetCurrent() {
    return reinterpret_cast<Isolate*>(v8::Isolate::GetCurrent()->GetData());
  }

  uv_loop_t* GetLoop() {
    NODE_ISOLATE_CHECK(this);
    return loop_;
  }

  v8::Isolate* GetV8Isolate() {
    NODE_ISOLATE_CHECK(this);
    return v8_isolate_;
  }

  v8::Handle<v8::Context> GetV8Context() {
    NODE_ISOLATE_CHECK(this);
    return v8_context_;
  }

  /* Register a handler that should run when the current isolate exits.
   * Handlers run in LIFO order.
   */
  void AtExit(AtExitCallback callback, void *arg);

  struct globals* Globals();

  unsigned int id_;

  // This constructor is used for every non-main thread
  Isolate();
  ~Isolate();

  void Enter();
  void Exit();

  /* Shutdown the isolate. Call this method at thread death. */
  void Dispose();

private:
  friend class IsolateWrap;

  struct AtExitCallbackInfo {
    AtExitCallback callback_;
    ngx_queue_t queue_;
    void* arg_;
  };

  static void OnMessage(IsolateMessage*, void*);

  // Forbid implicit constructors and copy constructors
  void operator=(const Isolate&) {}
  Isolate(const Isolate&) {}

  ngx_queue_t at_exit_callbacks_;
  v8::Persistent<v8::Context> v8_context_;
  v8::Isolate* v8_isolate_;
  IsolateChannel* send_channel_;
  IsolateChannel* recv_channel_;
  uv_loop_t* loop_;

  // Global variables for this isolate.
  struct globals globals_;
  bool globals_init_;
};

class IsolateDebugger : ObjectWrap {
public:
  static void Initialize();
  void Init();
  static void InitCallback(uv_async_t* c, int status);

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static IsolateDebugger* New(v8::Handle<v8::Value> init);

  static v8::Handle<v8::Value> Write(const v8::Arguments& args);

  static void DebugMessageHandler(const v8::Debug::Message& message);
  static void MessageCallback(IsolateDebuggerMessage* msg, void*);

  IsolateDebugger(v8::Handle<v8::Value> init);
  ~IsolateDebugger();

protected:
  Isolate* host_;
  uv_loop_t* host_loop_;

  uv_async_t init_callback_;
  v8::Persistent<v8::Value> init_callback_fn_;

  bool initialized_;
  Isolate* debuggee_;
  v8::Isolate* debuggee_v8_;

  struct debug_msg_s {
    uint16_t* value;
    int len;

    IsolateDebugger* d;
  };

  Channel<IsolateDebuggerMessage*>* msg_channel_;
};

} // namespace node

#endif // SRC_NODE_ISOLATE_H_
