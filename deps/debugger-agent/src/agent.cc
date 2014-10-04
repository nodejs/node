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

#include "agent.h"
#include "debugger-agent.h"

#include "node.h"
#include "node_internals.h"  // ARRAY_SIZE
#include "env.h"
#include "env-inl.h"
#include "v8.h"
#include "v8-debug.h"
#include "util.h"
#include "util-inl.h"
#include "queue.h"

#include <string.h>

namespace node {
namespace debugger {

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::Object;
using v8::String;
using v8::Value;


Agent::Agent(Environment* env) : state_(kNone),
                                 port_(5858),
                                 wait_(false),
                                 parent_env_(env),
                                 child_env_(NULL),
                                 dispatch_handler_(NULL) {
  int err;

  err = uv_sem_init(&start_sem_, 0);
  CHECK_EQ(err, 0);

  err = uv_mutex_init(&message_mutex_);
  CHECK_EQ(err, 0);

  QUEUE_INIT(&messages_);
}


Agent::~Agent() {
  Stop();

  uv_sem_destroy(&start_sem_);
  uv_mutex_destroy(&message_mutex_);

  // Clean-up messages
  while (!QUEUE_EMPTY(&messages_)) {
    QUEUE* q = QUEUE_HEAD(&messages_);
    QUEUE_REMOVE(q);
    AgentMessage* msg = ContainerOf(&AgentMessage::member, q);
    delete msg;
  }
}


bool Agent::Start(int port, bool wait) {
  int err;

  if (state_ == kRunning)
    return false;

  err = uv_loop_init(&child_loop_);
  if (err != 0)
    goto loop_init_failed;

  // Interruption signal handler
  err = uv_async_init(&child_loop_, &child_signal_, ChildSignalCb);
  if (err != 0)
    goto async_init_failed;
  uv_unref(reinterpret_cast<uv_handle_t*>(&child_signal_));

  port_ = port;
  wait_ = wait;

  err = uv_thread_create(&thread_,
                         reinterpret_cast<uv_thread_cb>(ThreadCb),
                         this);
  if (err != 0)
    goto thread_create_failed;

  uv_sem_wait(&start_sem_);

  state_ = kRunning;

  return true;

 thread_create_failed:
  uv_close(reinterpret_cast<uv_handle_t*>(&child_signal_), NULL);

 async_init_failed:
  err = uv_loop_close(&child_loop_);
  CHECK_EQ(err, 0);

 loop_init_failed:
  return false;
}


void Agent::Enable() {
  v8::Debug::SetMessageHandler(MessageHandler);

  // Assign environment to the debugger's context
  // NOTE: The debugger context is created after `SetMessageHandler()` call
  parent_env()->AssignToContext(v8::Debug::GetDebugContext());
}


void Agent::Stop() {
  int err;

  if (state_ != kRunning) {
    return;
  }

  v8::Debug::SetMessageHandler(NULL);

  // Send empty message to terminate things
  EnqueueMessage(new AgentMessage(NULL, 0));

  // Signal worker thread to make it stop
  err = uv_async_send(&child_signal_);
  CHECK_EQ(err, 0);

  err = uv_thread_join(&thread_);
  CHECK_EQ(err, 0);

  uv_close(reinterpret_cast<uv_handle_t*>(&child_signal_), NULL);
  uv_run(&child_loop_, UV_RUN_NOWAIT);

  err = uv_loop_close(&child_loop_);
  CHECK_EQ(err, 0);

  state_ = kNone;
}


void Agent::WorkerRun() {
  static const char* argv[] = { "node", "--debug-agent" };
  Isolate* isolate = Isolate::New();
  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);

    HandleScope handle_scope(isolate);
    Local<Context> context = Context::New(isolate);

    Context::Scope context_scope(context);
    Environment* env = CreateEnvironment(
        isolate,
        &child_loop_,
        context,
        ARRAY_SIZE(argv),
        argv,
        ARRAY_SIZE(argv),
        argv);

    child_env_ = env;

    // Expose API
    InitAdaptor(env);
    LoadEnvironment(env);

    CHECK_EQ(&child_loop_, env->event_loop());
    uv_run(&child_loop_, UV_RUN_DEFAULT);

    // Clean-up peristent
    api_.Reset();

    // Clean-up all running handles
    env->CleanupHandles();

    env->Dispose();
    env = NULL;
  }
  isolate->Dispose();
}


void Agent::InitAdaptor(Environment* env) {
  Isolate* isolate = env->isolate();
  HandleScope scope(isolate);

  // Create API adaptor
  Local<FunctionTemplate> t = FunctionTemplate::New(isolate);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewFromUtf8(isolate, "DebugAPI"));

  NODE_SET_PROTOTYPE_METHOD(t, "notifyListen", NotifyListen);
  NODE_SET_PROTOTYPE_METHOD(t, "notifyWait", NotifyWait);
  NODE_SET_PROTOTYPE_METHOD(t, "sendCommand", SendCommand);

  Local<Object> api = t->GetFunction()->NewInstance();
  api->SetAlignedPointerInInternalField(0, this);

  api->Set(String::NewFromUtf8(isolate, "port"), Integer::New(isolate, port_));

  env->process_object()->Set(String::NewFromUtf8(isolate, "_debugAPI"), api);
  api_.Reset(env->isolate(), api);
}


Agent* Agent::Unwrap(const v8::FunctionCallbackInfo<v8::Value>& args) {
  void* ptr = args.Holder()->GetAlignedPointerFromInternalField(0);
  return reinterpret_cast<Agent*>(ptr);
}


void Agent::NotifyListen(const FunctionCallbackInfo<Value>& args) {
  Agent* a = Unwrap(args);

  // Notify other thread that we are ready to process events
  uv_sem_post(&a->start_sem_);
}


void Agent::NotifyWait(const FunctionCallbackInfo<Value>& args) {
  Agent* a = Unwrap(args);

  a->wait_ = false;

  int err = uv_async_send(&a->child_signal_);
  CHECK_EQ(err, 0);
}


void Agent::SendCommand(const FunctionCallbackInfo<Value>& args) {
  Agent* a = Unwrap(args);
  Environment* env = a->child_env();
  HandleScope scope(env->isolate());

  String::Value v(args[0]);

  v8::Debug::SendCommand(a->parent_env()->isolate(), *v, v.length());
  if (a->dispatch_handler_ != NULL)
    a->dispatch_handler_(a->parent_env());
}


void Agent::ThreadCb(Agent* agent) {
  agent->WorkerRun();
}


void Agent::ChildSignalCb(uv_async_t* signal) {
  Agent* a = ContainerOf(&Agent::child_signal_, signal);
  Isolate* isolate = a->child_env()->isolate();

  HandleScope scope(isolate);
  Local<Object> api = PersistentToLocal(isolate, a->api_);

  uv_mutex_lock(&a->message_mutex_);
  while (!QUEUE_EMPTY(&a->messages_)) {
    QUEUE* q = QUEUE_HEAD(&a->messages_);
    AgentMessage* msg = ContainerOf(&AgentMessage::member, q);

    // Time to close everything
    if (msg->data() == NULL) {
      QUEUE_REMOVE(q);
      delete msg;

      MakeCallback(isolate, api, "onclose", 0, NULL);
      break;
    }

    // Waiting for client, do not send anything just yet
    // TODO(indutny): move this to js-land
    if (a->wait_)
      break;

    QUEUE_REMOVE(q);
    Local<Value> argv[] = {
      String::NewFromTwoByte(isolate,
                             msg->data(),
                             String::kNormalString,
                             msg->length())
    };

    // Emit message
    MakeCallback(isolate,
                 api,
                 "onmessage",
                 ARRAY_SIZE(argv),
                 argv);
    delete msg;
  }
  uv_mutex_unlock(&a->message_mutex_);
}


void Agent::EnqueueMessage(AgentMessage* message) {
  uv_mutex_lock(&message_mutex_);
  QUEUE_INSERT_TAIL(&messages_, &message->member);
  uv_mutex_unlock(&message_mutex_);
  uv_async_send(&child_signal_);
}


void Agent::MessageHandler(const v8::Debug::Message& message) {
  Isolate* isolate = message.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);
  Agent* a = env->debugger_agent();
  CHECK_NE(a, NULL);
  CHECK_EQ(isolate, a->parent_env()->isolate());

  HandleScope scope(isolate);
  Local<String> json = message.GetJSON();
  String::Value v(json);

  AgentMessage* msg = new AgentMessage(*v, v.length());
  a->EnqueueMessage(msg);
}

}  // namespace debugger
}  // namespace node
