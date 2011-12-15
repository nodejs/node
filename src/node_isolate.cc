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
static volatile int isolate_count;
static uv_mutex_t list_lock;
static ngx_queue_t list_head;


void Isolate::Initialize() {
  if (!initialized) {
    initialized = true;
    if (uv_mutex_init(&list_lock)) abort();
    ngx_queue_init(&list_head);
  }
}


Isolate* Isolate::New() {
  return new Isolate();
}


int Isolate::Count() {
  return isolate_count;
}


Isolate::Isolate() {
  uv_mutex_lock(&list_lock);

  assert(initialized && "node::Isolate::Initialize() hasn't been called");

  id_ = ++id;

  if (id_ == 1) {
    loop_ = uv_default_loop();
  } else {
    loop_ = uv_loop_new();
  }

  ngx_queue_init(&at_exit_callbacks_);

  ngx_queue_init(&list_member_);

  // Add this isolate into the list of all isolates.
  ngx_queue_insert_tail(&list_head, &list_member_);
  isolate_count++;

  uv_mutex_unlock(&list_lock);

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
  uv_mutex_lock(&list_lock);

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

  ngx_queue_remove(&list_member_);
  isolate_count--;
  assert(isolate_count >= 0);
  assert(isolate_count > 0 || ngx_queue_empty(&list_head));

  uv_mutex_unlock(&list_lock);
}


} // namespace node
