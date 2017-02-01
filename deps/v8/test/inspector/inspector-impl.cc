// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/inspector-impl.h"

#include "include/v8.h"
#include "src/inspector/string-16.h"

namespace {

const int kInspectorClientIndex = v8::Context::kDebugIdIndex + 1;

class ChannelImpl final : public v8_inspector::V8Inspector::Channel {
 public:
  explicit ChannelImpl(InspectorClientImpl::FrontendChannel* frontend_channel)
      : frontend_channel_(frontend_channel) {}
  virtual ~ChannelImpl() = default;

 private:
  void sendProtocolResponse(int callId,
                            const v8_inspector::StringView& message) override {
    frontend_channel_->SendMessageToFrontend(message);
  }
  void sendProtocolNotification(
      const v8_inspector::StringView& message) override {
    frontend_channel_->SendMessageToFrontend(message);
  }
  void flushProtocolNotifications() override {}

  InspectorClientImpl::FrontendChannel* frontend_channel_;
  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

InspectorClientImpl* InspectorClientFromContext(
    v8::Local<v8::Context> context) {
  InspectorClientImpl* inspector_client = static_cast<InspectorClientImpl*>(
      context->GetAlignedPointerFromEmbedderData(kInspectorClientIndex));
  CHECK(inspector_client);
  return inspector_client;
}

v8_inspector::String16 ToString16(v8::Local<v8::String> str) {
  std::unique_ptr<uint16_t[]> buffer(new uint16_t[str->Length()]);
  str->Write(reinterpret_cast<uint16_t*>(buffer.get()), 0, str->Length());
  return v8_inspector::String16(buffer.get(), str->Length());
}

void MessageHandler(v8::Local<v8::Message> message,
                    v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetEnteredContext();
  if (context.IsEmpty()) return;
  v8_inspector::V8Inspector* inspector =
      InspectorClientImpl::InspectorFromContext(context);

  v8::Local<v8::StackTrace> stack = message->GetStackTrace();
  int script_id = message->GetScriptOrigin().ScriptID()->Value();
  if (!stack.IsEmpty() && stack->GetFrameCount() > 0) {
    int top_script_id = stack->GetFrame(0)->GetScriptId();
    if (top_script_id == script_id) script_id = 0;
  }
  int line_number = message->GetLineNumber(context).FromMaybe(0);
  int column_number = 0;
  if (message->GetStartColumn(context).IsJust())
    column_number = message->GetStartColumn(context).FromJust() + 1;

  v8_inspector::StringView detailed_message;
  v8_inspector::String16 message_text_string = ToString16(message->Get());
  v8_inspector::StringView message_text(message_text_string.characters16(),
                                        message_text_string.length());
  v8_inspector::String16 url_string;
  if (message->GetScriptOrigin().ResourceName()->IsString()) {
    url_string =
        ToString16(message->GetScriptOrigin().ResourceName().As<v8::String>());
  }
  v8_inspector::StringView url(url_string.characters16(), url_string.length());

  inspector->exceptionThrown(context, message_text, exception, detailed_message,
                             url, line_number, column_number,
                             inspector->createStackTrace(stack), script_id);
}

}  //  namespace

class ConnectTask : public TaskRunner::Task {
 public:
  ConnectTask(InspectorClientImpl* client, v8::base::Semaphore* ready_semaphore)
      : client_(client), ready_semaphore_(ready_semaphore) {}
  virtual ~ConnectTask() = default;

  bool is_inspector_task() final { return true; }

  void Run(v8::Isolate* isolate,
           const v8::Global<v8::Context>& global_context) {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = global_context.Get(isolate);
    client_->connect(context);
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

 private:
  InspectorClientImpl* client_;
  v8::base::Semaphore* ready_semaphore_;
};

InspectorClientImpl::InspectorClientImpl(TaskRunner* task_runner,
                                         FrontendChannel* frontend_channel,
                                         v8::base::Semaphore* ready_semaphore)
    : isolate_(nullptr),
      task_runner_(task_runner),
      frontend_channel_(frontend_channel) {
  task_runner_->Append(new ConnectTask(this, ready_semaphore));
}

InspectorClientImpl::~InspectorClientImpl() {}

void InspectorClientImpl::connect(v8::Local<v8::Context> context) {
  isolate_ = context->GetIsolate();
  isolate_->AddMessageListener(MessageHandler);
  channel_.reset(new ChannelImpl(frontend_channel_));

  inspector_ = v8_inspector::V8Inspector::create(isolate_, this);
  session_ = inspector_->connect(1, channel_.get(), v8_inspector::StringView());

  context->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);
  inspector_->contextCreated(
      v8_inspector::V8ContextInfo(context, 1, v8_inspector::StringView()));
  context_.Reset(isolate_, context);
}

v8::Local<v8::Context> InspectorClientImpl::ensureDefaultContextInGroup(int) {
  CHECK(isolate_);
  return context_.Get(isolate_);
}

double InspectorClientImpl::currentTimeMS() {
  return v8::base::OS::TimeCurrentMillis();
}

void InspectorClientImpl::runMessageLoopOnPause(int) {
  task_runner_->RunMessageLoop(true);
}

void InspectorClientImpl::quitMessageLoopOnPause() {
  task_runner_->QuitMessageLoop();
}

v8_inspector::V8Inspector* InspectorClientImpl::InspectorFromContext(
    v8::Local<v8::Context> context) {
  return InspectorClientFromContext(context)->inspector_.get();
}

v8_inspector::V8InspectorSession* InspectorClientImpl::SessionFromContext(
    v8::Local<v8::Context> context) {
  return InspectorClientFromContext(context)->session_.get();
}

class SendMessageToBackendTask : public TaskRunner::Task {
 public:
  explicit SendMessageToBackendTask(const v8_inspector::String16& message)
      : message_(message) {}

  bool is_inspector_task() final { return true; }

  void Run(v8::Isolate* isolate,
           const v8::Global<v8::Context>& global_context) override {
    v8_inspector::V8InspectorSession* session = nullptr;
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = global_context.Get(isolate);
      session = InspectorClientImpl::SessionFromContext(context);
      CHECK(session);
    }
    v8_inspector::StringView message_view(
        reinterpret_cast<const uint16_t*>(message_.characters16()),
        message_.length());
    session->dispatchProtocolMessage(message_view);
  }

 private:
  v8_inspector::String16 message_;
};

TaskRunner* SendMessageToBackendExtension::backend_task_runner_ = nullptr;

v8::Local<v8::FunctionTemplate>
SendMessageToBackendExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> name) {
  return v8::FunctionTemplate::New(
      isolate, SendMessageToBackendExtension::SendMessageToBackend);
}

void SendMessageToBackendExtension::SendMessageToBackend(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(backend_task_runner_);
  CHECK(args.Length() == 1 && args[0]->IsString());
  v8::Local<v8::String> message = args[0].As<v8::String>();
  backend_task_runner_->Append(
      new SendMessageToBackendTask(ToString16(message)));
}
