// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_PROTOCOL_INSPECTOR_IMPL_H_
#define V8_TEST_INSPECTOR_PROTOCOL_INSPECTOR_IMPL_H_

#include <map>
#include <vector>

#include "include/v8-inspector.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"

class TaskRunner;

class InspectorClientImpl : public v8_inspector::V8InspectorClient {
 public:
  class FrontendChannel {
   public:
    virtual ~FrontendChannel() = default;
    virtual void SendMessageToFrontend(
        int session_id, const v8_inspector::StringView& message) = 0;
  };

  InspectorClientImpl(v8::Isolate* isolate, TaskRunner* task_runner,
                      FrontendChannel* frontend_channel);
  virtual ~InspectorClientImpl();

  v8_inspector::V8Inspector* inspector() const { return inspector_.get(); }
  int ConnectSession(int context_group_id,
                     const v8_inspector::StringView& state);
  std::unique_ptr<v8_inspector::StringBuffer> DisconnectSession(int session_id);
  void SendMessage(int session_id, const v8_inspector::StringView& message);
  void BreakProgram(int context_group_id,
                    const v8_inspector::StringView& reason,
                    const v8_inspector::StringView& details);
  void SchedulePauseOnNextStatement(int context_group_id,
                                    const v8_inspector::StringView& reason,
                                    const v8_inspector::StringView& details);
  void CancelPauseOnNextStatement(int context_group_id);
  void SetCurrentTimeMSForTest(double time);
  void SetMemoryInfoForTest(v8::Local<v8::Value> memory_info);
  void SetLogConsoleApiMessageCalls(bool log);
  void ContextCreated(v8::Local<v8::Context> context, int context_group_id);
  void ContextDestroyed(v8::Local<v8::Context> context);

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
  void consoleAPIMessage(int contextGroupId,
                         v8::Isolate::MessageErrorLevel level,
                         const v8_inspector::StringView& message,
                         const v8_inspector::StringView& url,
                         unsigned lineNumber, unsigned columnNumber,
                         v8_inspector::V8StackTrace*) override;

  std::vector<int> GetSessionIds(int context_group_id);

  std::unique_ptr<v8_inspector::V8Inspector> inspector_;
  int last_session_id_ = 0;
  std::map<int, std::unique_ptr<v8_inspector::V8InspectorSession>> sessions_;
  std::map<v8_inspector::V8InspectorSession*, int> context_group_by_session_;
  std::map<int, std::unique_ptr<v8_inspector::V8Inspector::Channel>> channels_;
  TaskRunner* task_runner_;
  v8::Isolate* isolate_;
  v8::Global<v8::Value> memory_info_;
  FrontendChannel* frontend_channel_;
  bool current_time_set_for_test_ = false;
  double current_time_ = 0.0;
  bool log_console_api_message_calls_ = false;

  DISALLOW_COPY_AND_ASSIGN(InspectorClientImpl);
};

#endif  //  V8_TEST_INSPECTOR_PROTOCOL_INSPECTOR_IMPL_H_
