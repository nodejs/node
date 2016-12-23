// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_INSPECTOR_IMPL_H_
#define V8_TEST_INSPECTOR_PROTOCOL_INSPECTOR_IMPL_H_

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "test/inspector/task-runner.h"

class InspectorClientImpl : public v8_inspector::V8InspectorClient {
 public:
  class FrontendChannel {
   public:
    virtual ~FrontendChannel() = default;
    virtual void SendMessageToFrontend(
        const v8_inspector::StringView& message) = 0;
  };

  InspectorClientImpl(TaskRunner* task_runner,
                      FrontendChannel* frontend_channel,
                      v8::base::Semaphore* ready_semaphore);
  virtual ~InspectorClientImpl();

  static v8_inspector::V8Inspector* InspectorFromContext(
      v8::Local<v8::Context> context);

 private:
  // V8InspectorClient implementation.
  v8::Local<v8::Context> ensureDefaultContextInGroup(
      int context_group_id) override;
  double currentTimeMS() override;
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;

  static v8_inspector::V8InspectorSession* SessionFromContext(
      v8::Local<v8::Context> context);

  friend class SendMessageToBackendTask;

  friend class ConnectTask;
  void connect(v8::Local<v8::Context> context);

  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  std::unique_ptr<v8_inspector::V8InspectorSession> session_;
  std::unique_ptr<v8_inspector::V8Inspector::Channel> channel_;

  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;

  TaskRunner* task_runner_;
  FrontendChannel* frontend_channel_;

  DISALLOW_COPY_AND_ASSIGN(InspectorClientImpl);
};

class SendMessageToBackendExtension : public v8::Extension {
 public:
  SendMessageToBackendExtension()
      : v8::Extension("v8_inspector/frontend",
                      "native function sendMessageToBackend();") {}
  virtual v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate, v8::Local<v8::String> name);

  static void set_backend_task_runner(TaskRunner* task_runner) {
    backend_task_runner_ = task_runner;
  }

 private:
  static void SendMessageToBackend(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static TaskRunner* backend_task_runner_;
};

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_INSPECTOR_IMPL_H_
