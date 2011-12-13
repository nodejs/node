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

#include "node_isolate.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>



namespace node {

static volatile bool initialized;
static volatile int id;
static uv_mutex_t id_lock;

void Isolate::Initialize() {
  if (!initialized) {
    initialized = true;
    if (uv_mutex_init(&id_lock)) abort();
  }
}

Isolate* Isolate::New(uv_loop_t* loop) {
  return new Isolate(loop);
}


Isolate::Isolate(uv_loop_t* loop) {
  assert(initialized && "node::Isolate::Initialize() hasn't been called");

  uv_mutex_lock(&id_lock);
  id_ = ++id;
  uv_mutex_unlock(&id_lock);

  ngx_queue_init(&at_exit_callbacks_);
  loop_ = loop;

  v8_isolate_ = v8::Isolate::GetCurrent();
  if (v8_isolate_ == NULL) {
    v8_isolate_ = v8::Isolate::New();
    v8_isolate_->Enter();
  }

  assert(v8_isolate_->GetData() == NULL);
  v8_isolate_->SetData(this);

  v8_context_ = v8::Context::New();
  v8_context_->Enter();

  globals_init(&globals_);
}


struct globals* Isolate::Globals() {
  return &globals_;
}


void Isolate::AtExit(AtExitCallback callback, void* arg) {
  struct AtExitCallbackInfo* it = new AtExitCallbackInfo;

  NODE_ISOLATE_CHECK(this);

  it->callback_ = callback;
  it->arg_ = arg;

  ngx_queue_insert_head(&at_exit_callbacks_, &it->at_exit_callbacks_);
}


void Isolate::Dispose() {
  struct AtExitCallbackInfo* it;
  ngx_queue_t* q;

  NODE_ISOLATE_CHECK(this);

  ngx_queue_foreach(q, &at_exit_callbacks_) {
    it = ngx_queue_data(q, struct AtExitCallbackInfo, at_exit_callbacks_);
    it->callback_(it->arg_);
    delete it;
  }
  ngx_queue_init(&at_exit_callbacks_);

  assert(v8_context_->InContext());
  v8_context_->Exit();
  v8_context_.Clear();
  v8_context_.Dispose();

  v8_isolate_->Exit();
  v8_isolate_->Dispose();
  v8_isolate_ = NULL;
}


} // namespace node
