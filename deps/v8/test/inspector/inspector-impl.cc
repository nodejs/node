// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/inspector-impl.h"

#include "include/v8.h"

#include "src/vector.h"

namespace {

const int kInspectorClientIndex = 0;

class ChannelImpl final : public v8_inspector::V8Inspector::Channel {
 public:
  explicit ChannelImpl(InspectorClientImpl::FrontendChannel* frontend_channel)
      : frontend_channel_(frontend_channel) {}
  virtual ~ChannelImpl() = default;

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    frontend_channel_->SendMessageToFrontend(message->string());
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    frontend_channel_->SendMessageToFrontend(message->string());
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

v8::internal::Vector<uint16_t> ToVector(v8::Local<v8::String> str) {
  v8::internal::Vector<uint16_t> buffer =
      v8::internal::Vector<uint16_t>::New(str->Length());
  str->Write(buffer.start(), 0, str->Length());
  return buffer;
}

void MessageHandler(v8::Local<v8::Message> message,
                    v8::Local<v8::Value> exception) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetEnteredContext();
  if (context.IsEmpty()) return;
  v8_inspector::V8Inspector* inspector =
      InspectorClientImpl::InspectorFromContext(context);

  v8::Local<v8::StackTrace> stack = message->GetStackTrace();
  int script_id =
      static_cast<int>(message->GetScriptOrigin().ScriptID()->Value());
  if (!stack.IsEmpty() && stack->GetFrameCount() > 0) {
    int top_script_id = stack->GetFrame(0)->GetScriptId();
    if (top_script_id == script_id) script_id = 0;
  }
  int line_number = message->GetLineNumber(context).FromMaybe(0);
  int column_number = 0;
  if (message->GetStartColumn(context).IsJust())
    column_number = message->GetStartColumn(context).FromJust() + 1;

  v8_inspector::StringView detailed_message;
  v8::internal::Vector<uint16_t> message_text_string = ToVector(message->Get());
  v8_inspector::StringView message_text(message_text_string.start(),
                                        message_text_string.length());
  v8::internal::Vector<uint16_t> url_string;
  if (message->GetScriptOrigin().ResourceName()->IsString()) {
    url_string =
        ToVector(message->GetScriptOrigin().ResourceName().As<v8::String>());
  }
  v8_inspector::StringView url(url_string.start(), url_string.length());

  inspector->exceptionThrown(context, message_text, exception, detailed_message,
                             url, line_number, column_number,
                             inspector->createStackTrace(stack), script_id);
}

v8::Local<v8::String> ToString(v8::Isolate* isolate,
                               const v8_inspector::StringView& string) {
  if (string.is8Bit())
    return v8::String::NewFromOneByte(isolate, string.characters8(),
                                      v8::NewStringType::kNormal,
                                      static_cast<int>(string.length()))
        .ToLocalChecked();
  else
    return v8::String::NewFromTwoByte(isolate, string.characters16(),
                                      v8::NewStringType::kNormal,
                                      static_cast<int>(string.length()))
        .ToLocalChecked();
}

void Print(v8::Isolate* isolate, const v8_inspector::StringView& string) {
  v8::Local<v8::String> v8_string = ToString(isolate, string);
  v8::String::Utf8Value utf8_string(v8_string);
  fwrite(*utf8_string, sizeof(**utf8_string), utf8_string.length(), stdout);
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

class DisconnectTask : public TaskRunner::Task {
 public:
  explicit DisconnectTask(InspectorClientImpl* client, bool reset_inspector,
                          v8::base::Semaphore* ready_semaphore)
      : client_(client),
        reset_inspector_(reset_inspector),
        ready_semaphore_(ready_semaphore) {}
  virtual ~DisconnectTask() = default;

  bool is_inspector_task() final { return true; }

  void Run(v8::Isolate* isolate,
           const v8::Global<v8::Context>& global_context) {
    client_->disconnect(reset_inspector_);
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

 private:
  InspectorClientImpl* client_;
  bool reset_inspector_;
  v8::base::Semaphore* ready_semaphore_;
};

class CreateContextGroupTask : public TaskRunner::Task {
 public:
  CreateContextGroupTask(InspectorClientImpl* client,
                         TaskRunner::SetupGlobalTasks setup_global_tasks,
                         v8::base::Semaphore* ready_semaphore,
                         int* context_group_id)
      : client_(client),
        setup_global_tasks_(std::move(setup_global_tasks)),
        ready_semaphore_(ready_semaphore),
        context_group_id_(context_group_id) {}
  virtual ~CreateContextGroupTask() = default;

  bool is_inspector_task() final { return true; }

  void Run(v8::Isolate* isolate,
           const v8::Global<v8::Context>& global_context) {
    *context_group_id_ = client_->createContextGroup(setup_global_tasks_);
    if (ready_semaphore_) ready_semaphore_->Signal();
  }

 private:
  InspectorClientImpl* client_;
  TaskRunner::SetupGlobalTasks setup_global_tasks_;
  v8::base::Semaphore* ready_semaphore_;
  int* context_group_id_;
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

  if (states_.empty()) {
    int context_group_id = TaskRunner::GetContextGroupId(context);
    v8_inspector::StringView state;
    sessions_[context_group_id] =
        inspector_->connect(context_group_id, channel_.get(), state);
    context->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);
    v8_inspector::V8ContextInfo info(context, context_group_id,
                                     v8_inspector::StringView());
    info.hasMemoryOnConsole = true;
    inspector_->contextCreated(info);
  } else {
    for (const auto& it : states_) {
      int context_group_id = it.first;
      v8::Local<v8::Context> context =
          task_runner_->GetContext(context_group_id);
      v8_inspector::StringView state = it.second->string();
      sessions_[context_group_id] =
          inspector_->connect(context_group_id, channel_.get(), state);
      context->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);
      v8_inspector::V8ContextInfo info(context, context_group_id,
                                       v8_inspector::StringView());
      info.hasMemoryOnConsole = true;
      inspector_->contextCreated(info);
    }
  }
  states_.clear();
}

void InspectorClientImpl::scheduleReconnect(
    v8::base::Semaphore* ready_semaphore) {
  task_runner_->Append(
      new DisconnectTask(this, /* reset_inspector */ true, nullptr));
  task_runner_->Append(new ConnectTask(this, ready_semaphore));
}

void InspectorClientImpl::scheduleDisconnect(
    v8::base::Semaphore* ready_semaphore) {
  task_runner_->Append(
      new DisconnectTask(this, /* reset_inspector */ false, ready_semaphore));
}

void InspectorClientImpl::disconnect(bool reset_inspector) {
  for (const auto& it : sessions_) {
    states_[it.first] = it.second->stateJSON();
  }
  sessions_.clear();
  if (reset_inspector) inspector_.reset();
}

void InspectorClientImpl::scheduleCreateContextGroup(
    TaskRunner::SetupGlobalTasks setup_global_tasks,
    v8::base::Semaphore* ready_semaphore, int* context_group_id) {
  task_runner_->Append(new CreateContextGroupTask(
      this, std::move(setup_global_tasks), ready_semaphore, context_group_id));
}

int InspectorClientImpl::createContextGroup(
    const TaskRunner::SetupGlobalTasks& setup_global_tasks) {
  v8::HandleScope handle_scope(isolate_);
  v8::Local<v8::Context> context =
      task_runner_->NewContextGroup(setup_global_tasks);
  context->SetAlignedPointerInEmbedderData(kInspectorClientIndex, this);
  int context_group_id = TaskRunner::GetContextGroupId(context);
  v8_inspector::StringView state;
  sessions_[context_group_id] =
      inspector_->connect(context_group_id, channel_.get(), state);
  inspector_->contextCreated(v8_inspector::V8ContextInfo(
      context, context_group_id, v8_inspector::StringView()));
  return context_group_id;
}

bool InspectorClientImpl::formatAccessorsAsProperties(
    v8::Local<v8::Value> object) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Private> shouldFormatAccessorsPrivate = v8::Private::ForApi(
      isolate, v8::String::NewFromUtf8(isolate, "allowAccessorFormatting",
                                       v8::NewStringType::kNormal)
                   .ToLocalChecked());
  CHECK(object->IsObject());
  return object.As<v8::Object>()
      ->HasPrivate(context, shouldFormatAccessorsPrivate)
      .FromMaybe(false);
}

v8::Local<v8::Context> InspectorClientImpl::ensureDefaultContextInGroup(
    int context_group_id) {
  CHECK(isolate_);
  return task_runner_->GetContext(context_group_id);
}

void InspectorClientImpl::setCurrentTimeMSForTest(double time) {
  current_time_ = time;
  current_time_set_for_test_ = true;
}

double InspectorClientImpl::currentTimeMS() {
  if (current_time_set_for_test_) return current_time_;
  return v8::base::OS::TimeCurrentMillis();
}

void InspectorClientImpl::setMemoryInfoForTest(
    v8::Local<v8::Value> memory_info) {
  memory_info_.Reset(isolate_, memory_info);
}

void InspectorClientImpl::setLogConsoleApiMessageCalls(bool log) {
  log_console_api_message_calls_ = log;
}

v8::MaybeLocal<v8::Value> InspectorClientImpl::memoryInfo(
    v8::Isolate* isolate, v8::Local<v8::Context>) {
  if (memory_info_.IsEmpty()) return v8::MaybeLocal<v8::Value>();
  return memory_info_.Get(isolate);
}

void InspectorClientImpl::runMessageLoopOnPause(int) {
  task_runner_->RunMessageLoop(true);
}

void InspectorClientImpl::quitMessageLoopOnPause() {
  task_runner_->QuitMessageLoop();
}

void InspectorClientImpl::consoleAPIMessage(
    int contextGroupId, v8::Isolate::MessageErrorLevel level,
    const v8_inspector::StringView& message,
    const v8_inspector::StringView& url, unsigned lineNumber,
    unsigned columnNumber, v8_inspector::V8StackTrace* stack) {
  if (!log_console_api_message_calls_) return;
  Print(isolate_, message);
  fprintf(stdout, " (");
  Print(isolate_, url);
  fprintf(stdout, ":%d:%d)", lineNumber, columnNumber);
  Print(isolate_, stack->toString()->string());
  fprintf(stdout, "\n");
}

v8_inspector::V8Inspector* InspectorClientImpl::InspectorFromContext(
    v8::Local<v8::Context> context) {
  return InspectorClientFromContext(context)->inspector_.get();
}

v8_inspector::V8InspectorSession* InspectorClientImpl::SessionFromContext(
    v8::Local<v8::Context> context) {
  int context_group_id = TaskRunner::GetContextGroupId(context);
  return InspectorClientFromContext(context)->sessions_[context_group_id].get();
}

v8_inspector::V8InspectorSession* InspectorClientImpl::session(
    int context_group_id) {
  if (context_group_id) {
    return sessions_[context_group_id].get();
  } else {
    return sessions_.begin()->second.get();
  }
}

class SendMessageToBackendTask : public TaskRunner::Task {
 public:
  explicit SendMessageToBackendTask(
      const v8::internal::Vector<uint16_t>& message, int context_group_id)
      : message_(message), context_group_id_(context_group_id) {}

  bool is_inspector_task() final { return true; }

  void Run(v8::Isolate* isolate,
           const v8::Global<v8::Context>& global_context) override {
    v8_inspector::V8InspectorSession* session = nullptr;
    {
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = global_context.Get(isolate);
      if (!context_group_id_) {
        session = InspectorClientImpl::SessionFromContext(context);
      } else {
        session = InspectorClientFromContext(context)
                      ->sessions_[context_group_id_]
                      .get();
      }
      if (!session) return;
    }
    v8_inspector::StringView message_view(message_.start(), message_.length());
    session->dispatchProtocolMessage(message_view);
  }

 private:
  v8::internal::Vector<uint16_t> message_;
  int context_group_id_;
};

TaskRunner* SendMessageToBackendExtension::backend_task_runner_ = nullptr;

void SendMessageToBackendExtension::Run(v8::Isolate* isolate,
                                        v8::Local<v8::ObjectTemplate> global) {
  global->Set(
      v8::String::NewFromUtf8(isolate, "sendMessageToBackend",
                              v8::NewStringType::kNormal)
          .ToLocalChecked(),
      v8::FunctionTemplate::New(
          isolate, &SendMessageToBackendExtension::SendMessageToBackend));
}

void SendMessageToBackendExtension::SendMessageToBackend(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(backend_task_runner_);
  CHECK(args.Length() == 2 && args[0]->IsString() && args[1]->IsInt32());
  v8::Local<v8::String> message = args[0].As<v8::String>();
  backend_task_runner_->Append(new SendMessageToBackendTask(
      ToVector(message), args[1].As<v8::Int32>()->Value()));
}
