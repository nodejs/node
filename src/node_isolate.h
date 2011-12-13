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
#include "uv.h"
#include "node_vars.h"

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

class Isolate {
public:
  // Call this before instantiating any Isolate
  static void Initialize();

  typedef void (*AtExitCallback)(void* arg);

  static Isolate* New();

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

  /* Shutdown the isolate. Call this method at thread death. */
  void Dispose();

  struct globals* Globals();

  unsigned int id_;

private:
  Isolate();

  struct AtExitCallbackInfo {
    ngx_queue_t at_exit_callbacks_;
    AtExitCallback callback_;
    void* arg_;
  };

  ngx_queue_t at_exit_callbacks_;
  v8::Persistent<v8::Context> v8_context_;
  v8::Isolate* v8_isolate_;
  uv_loop_t* loop_;

  // Global variables for this isolate.
  struct globals globals_;
};

} // namespace node

#endif // SRC_NODE_ISOLATE_H_
