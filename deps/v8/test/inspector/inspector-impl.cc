// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/inspector-impl.h"

#include "include/v8.h"

#include "src/vector.h"
#include "test/inspector/isolate-data.h"
#include "test/inspector/task-runner.h"

namespace {

class ChannelImpl final : public v8_inspector::V8Inspector::Channel {
 public:
  ChannelImpl(InspectorClientImpl::FrontendChannel* frontend_channel,
              int session_id)
      : frontend_channel_(frontend_channel), session_id_(session_id) {}
  virtual ~ChannelImpl() = default;

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    frontend_channel_->SendMessageToFrontend(session_id_, message->string());
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    frontend_channel_->SendMessageToFrontend(session_id_, message->string());
  }
  void flushProtocolNotifications() override {}

  InspectorClientImpl::FrontendChannel* frontend_channel_;
  int session_id_;
  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

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
      IsolateData::FromContext(context)->inspector()->inspector();

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

InspectorClientImpl::InspectorClientImpl(v8::Isolate* isolate,
                                         TaskRunner* task_runner,
                                         FrontendChannel* frontend_channel)
    : task_runner_(task_runner),
      isolate_(isolate),
      frontend_channel_(frontend_channel) {
  isolate_->AddMessageListener(MessageHandler);
  inspector_ = v8_inspector::V8Inspector::create(isolate_, this);
}

InspectorClientImpl::~InspectorClientImpl() {}

int InspectorClientImpl::ConnectSession(int context_group_id,
                                        const v8_inspector::StringView& state) {
  int session_id = ++last_session_id_;
  channels_[session_id].reset(new ChannelImpl(frontend_channel_, session_id));
  sessions_[session_id] =
      inspector_->connect(context_group_id, channels_[session_id].get(), state);
  context_group_by_session_[sessions_[session_id].get()] = context_group_id;
  return session_id;
}

std::unique_ptr<v8_inspector::StringBuffer>
InspectorClientImpl::DisconnectSession(int session_id) {
  auto it = sessions_.find(session_id);
  CHECK(it != sessions_.end());
  context_group_by_session_.erase(it->second.get());
  std::unique_ptr<v8_inspector::StringBuffer> result = it->second->stateJSON();
  sessions_.erase(it);
  channels_.erase(session_id);
  return result;
}

void InspectorClientImpl::SendMessage(int session_id,
                                      const v8_inspector::StringView& message) {
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) it->second->dispatchProtocolMessage(message);
}

void InspectorClientImpl::BreakProgram(
    int context_group_id, const v8_inspector::StringView& reason,
    const v8_inspector::StringView& details) {
  for (int session_id : GetSessionIds(context_group_id)) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) it->second->breakProgram(reason, details);
  }
}

void InspectorClientImpl::SchedulePauseOnNextStatement(
    int context_group_id, const v8_inspector::StringView& reason,
    const v8_inspector::StringView& details) {
  for (int session_id : GetSessionIds(context_group_id)) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end())
      it->second->schedulePauseOnNextStatement(reason, details);
  }
}

void InspectorClientImpl::CancelPauseOnNextStatement(int context_group_id) {
  for (int session_id : GetSessionIds(context_group_id)) {
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) it->second->cancelPauseOnNextStatement();
  }
}

void InspectorClientImpl::ContextCreated(v8::Local<v8::Context> context,
                                         int context_group_id) {
  v8_inspector::V8ContextInfo info(context, context_group_id,
                                   v8_inspector::StringView());
  info.hasMemoryOnConsole = true;
  inspector_->contextCreated(info);
}

void InspectorClientImpl::ContextDestroyed(v8::Local<v8::Context> context) {
  inspector_->contextDestroyed(context);
}

std::vector<int> InspectorClientImpl::GetSessionIds(int context_group_id) {
  std::vector<int> result;
  for (auto& it : sessions_) {
    if (context_group_by_session_[it.second.get()] == context_group_id)
      result.push_back(it.first);
  }
  return result;
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
  return task_runner_->data()->GetContext(context_group_id);
}

void InspectorClientImpl::SetCurrentTimeMSForTest(double time) {
  current_time_ = time;
  current_time_set_for_test_ = true;
}

double InspectorClientImpl::currentTimeMS() {
  if (current_time_set_for_test_) return current_time_;
  return v8::base::OS::TimeCurrentMillis();
}

void InspectorClientImpl::SetMemoryInfoForTest(
    v8::Local<v8::Value> memory_info) {
  memory_info_.Reset(isolate_, memory_info);
}

void InspectorClientImpl::SetLogConsoleApiMessageCalls(bool log) {
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
