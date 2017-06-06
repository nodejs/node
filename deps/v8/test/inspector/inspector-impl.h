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

  void scheduleReconnect(v8::base::Semaphore* ready_semaphore);
  void scheduleDisconnect(v8::base::Semaphore* ready_semaphore);
  void scheduleCreateContextGroup(v8::ExtensionConfiguration* extensions,
                                  v8::base::Semaphore* ready_semaphore,
                                  int* context_group_id);

  static v8_inspector::V8Inspector* InspectorFromContext(
      v8::Local<v8::Context> context);
  static v8_inspector::V8InspectorSession* SessionFromContext(
      v8::Local<v8::Context> context);

  // context_group_id = 0 means default context group.
  v8_inspector::V8InspectorSession* session(int context_group_id = 0);

  void setCurrentTimeMSForTest(double time);
  void setMemoryInfoForTest(v8::Local<v8::Value> memory_info);

 private:
  // V8InspectorClient implementation.
  bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
  v8::Local<v8::Context> ensureDefaultContextInGroup(
      int context_group_id) override;
  double currentTimeMS() override;
  v8::MaybeLocal<v8::Value> memoryInfo(v8::Isolate* isolate,
                                       v8::Local<v8::Context>) override;
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;

  friend class SendMessageToBackendTask;

  friend class ConnectTask;
  void connect(v8::Local<v8::Context> context);
  friend class DisconnectTask;
  void disconnect(bool reset_inspector);
  friend class CreateContextGroupTask;
  int createContextGroup(v8::ExtensionConfiguration* extensions);

  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  std::unique_ptr<v8_inspector::V8Inspector::Channel> channel_;

  std::map<int, std::unique_ptr<v8_inspector::V8InspectorSession>> sessions_;
  std::map<int, std::unique_ptr<v8_inspector::StringBuffer>> states_;

  v8::Isolate* isolate_;
  v8::Global<v8::Value> memory_info_;

  TaskRunner* task_runner_;
  FrontendChannel* frontend_channel_;

  bool current_time_set_for_test_ = false;
  double current_time_ = 0.0;

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
