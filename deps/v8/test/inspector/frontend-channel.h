// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_FRONTEND_CHANNEL_H_
#define V8_TEST_INSPECTOR_FRONTEND_CHANNEL_H_

#include <vector>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "test/inspector/task-runner.h"
#include "test/inspector/utils.h"

namespace v8 {
namespace internal {

class FrontendChannelImpl : public v8_inspector::V8Inspector::Channel {
 public:
  FrontendChannelImpl(TaskRunner* task_runner, int context_group_id,
                      v8::Isolate* isolate, v8::Local<v8::Function> function)
      : task_runner_(task_runner),
        context_group_id_(context_group_id),
        function_(isolate, function) {}
  ~FrontendChannelImpl() override = default;
  FrontendChannelImpl(const FrontendChannelImpl&) = delete;
  FrontendChannelImpl& operator=(const FrontendChannelImpl&) = delete;

  void set_session_id(int session_id) { session_id_ = session_id; }

 private:
  void sendResponse(
      int callId,
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    task_runner_->Append(
        std::make_unique<SendMessageTask>(this, ToVector(message->string())));
  }
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override {
    task_runner_->Append(
        std::make_unique<SendMessageTask>(this, ToVector(message->string())));
  }
  void flushProtocolNotifications() override {}

  class SendMessageTask : public TaskRunner::Task {
   public:
    SendMessageTask(FrontendChannelImpl* channel,
                    const std::vector<uint16_t>& message)
        : channel_(channel), message_(message) {}
    ~SendMessageTask() override = default;
    bool is_priority_task() final { return false; }

   private:
    void Run(IsolateData* data) override {
      v8::MicrotasksScope microtasks_scope(data->isolate(),
                                           v8::MicrotasksScope::kRunMicrotasks);
      v8::HandleScope handle_scope(data->isolate());
      v8::Local<v8::Context> context =
          data->GetDefaultContext(channel_->context_group_id_);
      v8::Context::Scope context_scope(context);
      v8::Local<v8::Value> message = ToV8String(data->isolate(), message_);
      v8::MaybeLocal<v8::Value> result;
      result = channel_->function_.Get(data->isolate())
                   ->Call(context, context->Global(), 1, &message);
    }
    FrontendChannelImpl* channel_;
    std::vector<uint16_t> message_;
  };

  TaskRunner* task_runner_;
  int context_group_id_;
  v8::Global<v8::Function> function_;
  int session_id_;
};

}  // namespace internal
}  // namespace v8

#endif  //  V8_TEST_INSPECTOR_FRONTEND_CHANNEL_H_
