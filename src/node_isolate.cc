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

#include <v8.h>
#include <v8-debug.h>
#include <node.h>
#include <node_buffer.h>
#include <node_isolate.h>
#include <node_internals.h>
#include <node_object_wrap.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define isolate_debugger_constructor NODE_VAR(isolate_debugger_constructor)


namespace node {

using v8::Arguments;
using v8::Array;
using v8::Context;
using v8::False;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::ObjectTemplate;
using v8::Persistent;
using v8::String;
using v8::True;
using v8::Undefined;
using v8::Value;
using v8::Undefined;

static volatile bool initialized;
static volatile int id;
static volatile int isolate_count;
static ngx_queue_t isolate_list;
static uv_mutex_t isolate_mutex;

#ifdef NDEBUG
# define IF_DEBUG(expr)
#else
# define IF_DEBUG(expr) expr;
#endif

template <class T>
class Queue {
public:
  Queue() {
    if (uv_mutex_init(&mutex_)) abort();
    ngx_queue_init(&queue_);
  }

  ~Queue() {
    IF_DEBUG({
      uv_mutex_lock(&mutex_);
      assert(ngx_queue_empty(&queue_));
      uv_mutex_unlock(&mutex_);
    })
    uv_mutex_destroy(&mutex_);
  }

  void Produce(T item) {
    Message* m = new Message;
    m->item_ = item;

    uv_mutex_lock(&mutex_);
    ngx_queue_insert_tail(&queue_, &m->queue_);
    uv_mutex_unlock(&mutex_);
  }

  bool Consume(T& item) {
    ngx_queue_t* q = NULL;

    uv_mutex_lock(&mutex_);
    if (!ngx_queue_empty(&queue_)) {
      q = ngx_queue_head(&queue_);
      ngx_queue_remove(q);
    }
    uv_mutex_unlock(&mutex_);

    if (q == NULL) return false;

    Message* m = ngx_queue_data(q, Message, queue_);
    item = m->item_;
    delete m;

    return true;
  }

private:
  struct Message {
    ngx_queue_t queue_;
    T item_;
  };

  ngx_queue_t queue_;
  uv_mutex_t mutex_;
};


template <class T>
class Channel {
public:
  typedef void (*Callback)(T item, void* arg);

  Channel(uv_loop_t* loop, Callback callback, void* arg) {
    callback_ = callback;
    arg_ = arg;
    uv_async_init(loop, &async_, OnMessage);
    uv_unref(loop);
  }

  ~Channel() {
    uv_ref(async_.loop);
    uv_close(reinterpret_cast<uv_handle_t*>(&async_), NULL);
  }

  void Send(T item) {
    queue_.Produce(item);
    uv_async_send(&async_);
  }

private:
  static void OnMessage(uv_async_t* handle, int status) {
    Channel* c = container_of(handle, Channel, async_);
    c->OnMessage();
  }

  void OnMessage() {
    T item;
    while (queue_.Consume(item)) callback_(item, arg_);
  }

  void* arg_;
  Callback callback_;
  uv_async_t async_;
  Queue<T> queue_;
};


struct IsolateMessage {
  size_t size_;
  char* data_;

  IsolateMessage(const char* data, size_t size) {
    // make a copy for now
    size_ = size;
    data_ = new char[size];
    memcpy(data_, data, size);
  }

  ~IsolateMessage() {
    delete[] data_;
  }

  static void Free(char* data, void* arg) {
    IsolateMessage* msg = static_cast<IsolateMessage*>(arg);
    assert(data == msg->data_);
    delete msg;
  }
};


class IsolateChannel: public Channel<IsolateMessage*> {
public:
  IsolateChannel(uv_loop_t* loop, Callback callback, void* arg)
    : Channel<IsolateMessage*>(loop, callback, arg)
  {
  }
};


Handle<Value> Isolate::Send(const Arguments& args) {
  HandleScope scope;

  Isolate* isolate = Isolate::GetCurrent();
  assert(Buffer::HasInstance(args[0]));
  assert(isolate->send_channel_ != NULL);

  Local<Object> obj = args[0]->ToObject();
  const char* data = Buffer::Data(obj);
  size_t size = Buffer::Length(obj);

  IsolateMessage* msg = new IsolateMessage(data, size);
  isolate->send_channel_->Send(msg);

  return Undefined();
}


Handle<Value> Isolate::Unref(const Arguments& args) {
  HandleScope scope;

  Isolate* isolate = Isolate::GetCurrent();
  uv_unref(isolate->loop_);

  return Undefined();
}


void Isolate::OnMessage(IsolateMessage* msg, void* arg) {
  HandleScope scope;

  Isolate* self = static_cast<Isolate*>(arg);
  NODE_ISOLATE_CHECK(self);

  Buffer* buf = Buffer::New(msg->data_, msg->size_, IsolateMessage::Free, msg);
  Handle<Value> argv[] = { buf->handle_ };
  MakeCallback(self->globals_.process, "_onmessage", ARRAY_SIZE(argv), argv);
}


void Isolate::Initialize() {
  if (initialized) return;
  if (uv_mutex_init(&isolate_mutex)) abort();
  ngx_queue_init(&isolate_list);
  initialized = true;
}


int Isolate::Count() {
  int count;
  uv_mutex_lock(&isolate_mutex);
  count = isolate_count;
  uv_mutex_unlock(&isolate_mutex);
  return count;
}


Isolate::Isolate() {
  send_channel_ = NULL; // set (and deleted) by the parent isolate
  recv_channel_ = NULL;

  uv_mutex_lock(&isolate_mutex);
  assert(initialized && "node::Isolate::Initialize() hasn't been called");
  ngx_queue_insert_tail(&isolate_list, &isolate_list_);
  isolate_count++;
  id_ = ++id;
  uv_mutex_unlock(&isolate_mutex);

  if (id_ == 1) {
    loop_ = uv_default_loop();
  } else {
    loop_ = uv_loop_new();
    // Artificially ref the isolate loop so that the child
    // isolate stays alive by default.  process.exit will
    // unref the loop (see Isolate::Unref).
    uv_ref(loop_);
  }

  debug_state = kNone;
  debugger_instance = NULL;

  ngx_queue_init(&at_exit_callbacks_);

  v8_isolate_ = v8::Isolate::New();
  assert(v8_isolate_->GetData() == NULL);
  v8_isolate_->SetData(this);

  globals_init_ = false;
}


Isolate::~Isolate() {
  if (!argv_) return;
  for (size_t i = 0; argv_[i]; ++i) delete[] argv_[i];
  delete[] argv_;
}


struct globals* Isolate::Globals() {
  return &globals_;
}


void Isolate::AtExit(AtExitCallback callback, void* arg) {
  struct AtExitCallbackInfo* it = new AtExitCallbackInfo;

  //NODE_ISOLATE_CHECK(this);

  it->callback_ = callback;
  it->arg_ = arg;

  ngx_queue_insert_head(&at_exit_callbacks_, &it->queue_);
}


void Isolate::Enter() {
  v8_isolate_->Enter();

  if (v8_context_.IsEmpty()) {
    v8_context_ = Context::New();
  }
  v8_context_->Enter();

  if (!globals_init_) {
    globals_init_ = true;
    globals_init(&globals_);
  }

  NODE_ISOLATE_CHECK(this);
}


void Isolate::Exit() {
  NODE_ISOLATE_CHECK(this);
  v8_context_->Exit();
  v8_isolate_->Exit();
}


void Isolate::Dispose() {
  NODE_ISOLATE_CHECK(this);

  while (!ngx_queue_empty(&at_exit_callbacks_)) {
    ngx_queue_t* q = ngx_queue_head(&at_exit_callbacks_);
    ngx_queue_remove(q);

    AtExitCallbackInfo* it = ngx_queue_data(q, AtExitCallbackInfo, queue_);
    it->callback_(it->arg_);

    delete it;
  }

  assert(v8_context_->InContext());
  v8_context_->Exit();
  v8_context_.Clear();
  v8_context_.Dispose();

  v8_isolate_->Exit();
  v8_isolate_->Dispose();
  v8_isolate_ = NULL;

  uv_mutex_lock(&isolate_mutex);
  isolate_count--;
  ngx_queue_remove(&isolate_list_);
  assert(isolate_count >= 0);
  assert((isolate_count == 0 && ngx_queue_empty(&isolate_list))
      || (isolate_count > 0 && !ngx_queue_empty(&isolate_list)));
  uv_mutex_unlock(&isolate_mutex);
}


struct IsolateWrap: public ObjectWrap {
public:
  IsolateWrap(Isolate* parent_isolate) {
    parent_isolate_ = parent_isolate;

    uv_loop_t* parent_loop = parent_isolate->GetLoop();
    recv_channel_ = new IsolateChannel(
        parent_loop, IsolateWrap::OnMessage, this);

    isolate_ = new Isolate;
    send_channel_ = new IsolateChannel(
        isolate_->loop_, Isolate::OnMessage, isolate_);

    isolate_->send_channel_ = recv_channel_;
    isolate_->recv_channel_ = send_channel_;

    // TODO this could be folded into the regular channel
    uv_async_init(parent_loop, &child_exit_, AfterChildExit);
    isolate_->AtExit(AtChildExit, this);

    HandleScope scope;
    Local<ObjectTemplate> tpl = ObjectTemplate::New();
    tpl->SetInternalFieldCount(1);

    Local<Object> obj = tpl->NewInstance();
    Wrap(obj);
    Ref(); // unref'd when the child isolate exits

    obj->Set(String::NewSymbol("tid"),
             Integer::New(isolate_->id_));

    obj->Set(String::NewSymbol("send"),
             FunctionTemplate::New(Send)->GetFunction());
  }

  ~IsolateWrap() {
    delete isolate_;
    delete recv_channel_;
    delete send_channel_;
  }

  Isolate* GetIsolate() {
    return isolate_;
  }

private:
  // runs in the child thread
  static void AtChildExit(void* arg) {
    IsolateWrap* self = static_cast<IsolateWrap*>(arg);
    uv_async_send(&self->child_exit_);
  }

  // runs in the parent thread
  static void AfterChildExit(uv_async_t* handle, int status) {
    IsolateWrap* self = container_of(handle, IsolateWrap, child_exit_);
    self->OnExit();
  }

  void OnExit() {
    if (uv_thread_join(&isolate_->tid_)) abort();
    uv_close(reinterpret_cast<uv_handle_t*>(&child_exit_), NULL);
    MakeCallback(handle_, "onexit", 0, NULL);
    Unref(); // child is dead, it's safe to GC the JS object now
  }

  static void OnMessage(IsolateMessage* msg, void* arg) {
    IsolateWrap* self = static_cast<IsolateWrap*>(arg);
    self->OnMessage(msg);
  }

  void OnMessage(IsolateMessage* msg) {
    NODE_ISOLATE_CHECK(parent_isolate_);
    HandleScope scope;
    Buffer* buf = Buffer::New(
        msg->data_, msg->size_, IsolateMessage::Free, msg);
    Handle<Value> argv[] = { buf->handle_ };
    MakeCallback(handle_, "onmessage", ARRAY_SIZE(argv), argv);
  }

  // TODO merge with Isolate::Send(), it's almost identical
  static Handle<Value> Send(const Arguments& args) {
    HandleScope scope;
    IsolateWrap* self = Unwrap<IsolateWrap>(args.This());
    assert(Buffer::HasInstance(args[0]));

    Local<Object> obj = args[0]->ToObject();
    const char* data = Buffer::Data(obj);
    size_t size = Buffer::Length(obj);

    IsolateMessage* msg = new IsolateMessage(data, size);
    self->send_channel_->Send(msg);

    return Undefined();
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(IsolateWrap);
  Isolate* isolate_;
  Isolate* parent_isolate_;
  IsolateChannel* send_channel_;
  IsolateChannel* recv_channel_;
  uv_async_t child_exit_; // side effect: keeps the parent's event loop alive
                          // until the child exits
};


static void RunIsolate(void* arg) {
  Isolate* isolate = static_cast<Isolate*>(arg);
  isolate->Enter();
  StartThread(isolate, isolate->argc_, isolate->argv_);
  isolate->Dispose();
}


static Handle<Value> CreateIsolate(const Arguments& args) {
  HandleScope scope;

  assert(args[0]->IsArray());

  Local<Array> argv = args[0].As<Array>();
  assert(argv->Length() >= 2);

  Isolate* current_isolate = node::Isolate::GetCurrent();
  IsolateWrap* wrap = new IsolateWrap(current_isolate);
  Isolate* isolate = wrap->GetIsolate();

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

  // If options object was provided
  if (args.Length() > 1) {
    Local<Object> options = args[1].As<Object>();
    Local<Value> opt_debug = options->Get(String::New("debug"));
    Local<Value> opt_debug_brk = options->Get(String::New("debugBrk"));

    // Handle .debug = true case
    if (opt_debug->IsFunction()) {
      isolate->debug_state = opt_debug_brk->IsTrue() ?
          Isolate::kDebugBrk
          :
          Isolate::kDebug;
      isolate->debugger_instance = IsolateDebugger::New(opt_debug);
    }
  }

  if (uv_thread_create(&isolate->tid_, RunIsolate, isolate))
    return Null(); // wrap is collected by the GC
  else
    return wrap->handle_;
}


static Handle<Value> CountIsolate(const Arguments& args) {
  HandleScope scope;
  return scope.Close(Integer::New(Isolate::Count()));
}


void InitIsolates(Handle<Object> target) {
  HandleScope scope;
  NODE_SET_METHOD(target, "create", CreateIsolate);
  NODE_SET_METHOD(target, "count", CountIsolate);

  IsolateDebugger::Initialize();
}


class IsolateDebuggerMessage {
 public:
  IsolateDebugger* d_;
  uint16_t* value_;
  int len_;

  IsolateDebuggerMessage(IsolateDebugger* d, uint16_t* value, int len) {
    d_ = d;
    value_ = new uint16_t[len];
    len_ = len;
    memcpy(value_, value, len * sizeof(value_[0]));
  }

  ~IsolateDebuggerMessage() {
    delete[] value_;
  }
};


void IsolateDebugger::Initialize() {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(IsolateDebugger::New);
  isolate_debugger_constructor = Persistent<FunctionTemplate>::New(t);

  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("IsolateDebugger"));

  NODE_SET_PROTOTYPE_METHOD(t, "write", IsolateDebugger::Write);
}


IsolateDebugger::IsolateDebugger(Handle<Value> init) {
  debuggee_ = NULL;
  initialized_ = false;
  host_ = Isolate::GetCurrent();
  host_loop_ = host_->GetLoop();
  init_callback_fn_ = Persistent<Value>::New(init);

  // Init async handle to invoke js callback once
  // debugger will be initialized
  uv_async_init(host_loop_,
                &init_callback_,
                IsolateDebugger::InitCallback);
  init_callback_.data = reinterpret_cast<void*>(this);

  msg_channel_ = new Channel<IsolateDebuggerMessage*>(
      host_loop_, MessageCallback, NULL);
}


IsolateDebugger::~IsolateDebugger() {
  init_callback_fn_.Clear();
  init_callback_fn_.Dispose();
  delete msg_channel_;
}


void IsolateDebugger::Init(void) {
  HandleScope scope;

  Isolate* isolate = Isolate::GetCurrent();

  debuggee_ = isolate;
  debuggee_v8_ = isolate->GetV8Isolate();
  v8::Debug::SetMessageHandler2(IsolateDebugger::DebugMessageHandler);

  // Expose v8debug for isolate

  if (isolate->debug_state == Isolate::kDebugBrk) {
    Local<Context> debugContext = v8::Debug::GetDebugContext();

    debugContext->SetSecurityToken(
        isolate->GetV8Context()->GetSecurityToken()
    );
    isolate->GetV8Context()->Global()->Set(
        String::New("v8debug"),
        debugContext->Global()
    );
  }

  initialized_ = true;

  uv_async_send(&init_callback_);
}


void IsolateDebugger::InitCallback(uv_async_t* c, int status) {
  assert(c->data != NULL);

  IsolateDebugger* d = reinterpret_cast<IsolateDebugger*>(c->data);

  d->host_->Enter();
  HandleScope scope;

  Handle<Value> argv[1] = { d->handle_ };
  Function::Cast(*d->init_callback_fn_)->Call(d->handle_, 1, argv);

  d->host_->Exit();

  // Unreference loop
  uv_unref(d->host_loop_);
}


Handle<Value> IsolateDebugger::New(const Arguments& args) {
  HandleScope scope;

  IsolateDebugger* d = new IsolateDebugger(args[0]);
  d->Wrap(args.Holder());

  return args.This();
}


IsolateDebugger* IsolateDebugger::New(Handle<Value> init) {
  HandleScope scope;

  Handle<Value> argv[1] = { init };
  Handle<Object> i = isolate_debugger_constructor->GetFunction()->NewInstance(
      1,
      argv
  );

  return ObjectWrap::Unwrap<IsolateDebugger>(i);
}


Handle<Value> IsolateDebugger::Write(const Arguments& args) {
  HandleScope scope;

  if (args.Length() != 1) {
    return ThrowException(String::New(
        "IsolateDebugger::Write requires one argument"
    ));
  }

  IsolateDebugger* d = ObjectWrap::Unwrap<IsolateDebugger>(args.This());
  assert(d->initialized_);

  String::Value v(args[0]->ToString());
  v8::Debug::SendCommand(*v,
                         v.length(),
                         NULL,
                         d->debuggee_v8_);

  return Undefined();
}


void IsolateDebugger::DebugMessageHandler(const v8::Debug::Message& message) {
  IsolateDebugger* d = Isolate::GetCurrent()->debugger_instance;

  String::Value v(message.GetJSON());
  d->msg_channel_->Send(new IsolateDebuggerMessage(d, *v, v.length()));
}


void IsolateDebugger::MessageCallback(IsolateDebuggerMessage* msg, void*) {
  assert(msg != NULL);

  IsolateDebugger *d = msg->d_;
  // Enter parent isolate context
  d->host_->Enter();
  HandleScope scope;

  // debugger.onmessage should be a function!
  Handle<Value> argv[] = { String::New(msg->value_, msg->len_) };
  MakeCallback(d->handle_, "onmessage", ARRAY_SIZE(argv), argv);

  // Free memory allocated for message
  delete msg;

  // And leave isolate
  d->host_->Exit();
}


} // namespace node


NODE_MODULE(node_isolates, node::InitIsolates)
