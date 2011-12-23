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

#include <node.h>
#include <node_isolate.h>
#include <node_internals.h>
#include <v8.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>


namespace node {

using v8::Arguments;
using v8::Array;
using v8::False;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::True;
using v8::Value;

static char magic_isolate_cookie_[] = "magic isolate cookie";


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


int Isolate::Count() {
  return isolate_count;
}


void Isolate::JoinAll() {
  uv_mutex_lock(&list_lock);

  while (ngx_queue_empty(&list_head) == false) {
    ngx_queue_t* q = ngx_queue_head(&list_head);
    assert(q);
    Isolate* isolate = ngx_queue_data(q, Isolate, list_member_);
    assert(isolate);

    // Unlock the list while we join the thread.
    uv_mutex_unlock(&list_lock);

    uv_thread_join(&isolate->tid_);

    // Relock to check the next element in the list.
    uv_mutex_lock(&list_lock);
  }

  // Unlock the list finally.
  uv_mutex_unlock(&list_lock);
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

  v8_isolate_ = v8::Isolate::New();
  assert(v8_isolate_->GetData() == NULL);
  v8_isolate_->SetData(this);

  globals_init_ = false;
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


void Isolate::Enter() {
  v8_isolate_->Enter();

  if (v8_context_.IsEmpty()) {
    v8_context_ = v8::Context::New();
  }
  v8_context_->Enter();

  if (!globals_init_) {
    globals_init_ = true;
    globals_init(&globals_);
  }

  NODE_ISOLATE_CHECK(this);
}


void Isolate::Dispose() {
  uv_mutex_lock(&list_lock);

  NODE_ISOLATE_CHECK(this);

  struct AtExitCallbackInfo* it;
  ngx_queue_t* q;


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


static void RunIsolate(void* arg) {
  node::Isolate* isolate = reinterpret_cast<node::Isolate*>(arg);
  isolate->Enter();

  // TODO in the future when v0.6 is dead, move StartThread and related
  // handles into node_isolate.cc. It is currently organized like this to
  // minimize diff (and thus merge conflicts) between the legacy v0.6
  // branch.
  StartThread(isolate, isolate->argc_, isolate->argv_);

  isolate->Dispose();
  delete isolate;
}


static Handle<Value> CreateIsolate(const Arguments& args) {
  HandleScope scope;

  assert(args[0]->IsArray());

  Local<Array> argv = args[0].As<Array>();
  assert(argv->Length() >= 2);

  // Note that isolate lock is aquired in the constructor here. It will not
  // be unlocked until RunIsolate starts and calls isolate->Enter().
  Isolate* isolate = new node::Isolate();

  // Copy over arguments into isolate
  isolate->argc_ = argv->Length();
  isolate->argv_ = new char*[isolate->argc_ + 1];
  for (int i = 0; i < isolate->argc_; ++i) {
    String::Utf8Value str(argv->Get(i));
    size_t size = 1 + strlen(*str);
    isolate->argv_[i] = new char[size];
    memcpy(isolate->argv_[i], *str, size);
  }
  isolate->argv_[isolate->argc_] = NULL;

  if (uv_thread_create(&isolate->tid_, RunIsolate, isolate)) {
    delete isolate;
    return Null();
  }

  // TODO instead of ObjectTemplate - have a special wrapper.
  Local<ObjectTemplate> tpl = ObjectTemplate::New();
  tpl->SetInternalFieldCount(2);

  Local<Object> obj = tpl->NewInstance();
  obj->SetPointerInInternalField(0, magic_isolate_cookie_);
  obj->SetPointerInInternalField(1, isolate);

  return scope.Close(obj);
}


static Handle<Value> CountIsolate(const Arguments& args) {
  HandleScope scope;
  return scope.Close(Integer::New(Isolate::Count()));
}


static Handle<Value> JoinIsolate(const Arguments& args) {
  HandleScope scope;

  assert(args[0]->IsObject());

  Local<Object> obj = args[0]->ToObject();
  assert(obj->InternalFieldCount() == 2);
  assert(obj->GetPointerFromInternalField(0) == magic_isolate_cookie_);

  Isolate* ti = reinterpret_cast<Isolate*>(
      obj->GetPointerFromInternalField(1));

  if (uv_thread_join(&ti->tid_))
    return False(); // error
  else
    return True();  // ok
}


void InitIsolates(Handle<Object> target) {
  HandleScope scope;
  NODE_SET_METHOD(target, "create", CreateIsolate);
  NODE_SET_METHOD(target, "count", CountIsolate);
  NODE_SET_METHOD(target, "join", JoinIsolate);
}


} // namespace node


NODE_MODULE(node_isolates, node::InitIsolates)
