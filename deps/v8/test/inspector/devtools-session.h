// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_INSPECTOR_DEVTOOLS_SESSION_
#define V8_TEST_INSPECTOR_DEVTOOLS_SESSION_

#include <memory>

#include "include/cppgc/allocation.h"
#include "include/v8-inspector.h"
#include "test/inspector/task-runner.h"

namespace v8 {
namespace internal {

class DevToolsSession : public v8_inspector::V8Inspector::ManagedChannel {
 public:
  ~DevToolsSession() override = default;
  DevToolsSession(const DevToolsSession&) = delete;
  DevToolsSession& operator=(const DevToolsSession&) = delete;

  static DevToolsSession* Connect(
      v8::Isolate* isolate, v8_inspector::V8Inspector* inspector,
      int context_group_id, const v8_inspector::StringView& state,
      v8_inspector::V8Inspector::ClientTrustLevel client_trust_level,
      v8_inspector::V8Inspector::SessionPauseState pause_state,
      std::shared_ptr<v8_inspector::V8Inspector::Channel> channel);
  void Disconnect();

  void Trace(cppgc::Visitor* visitor) const override;

  int session_id() const { return session_id_; }
  int context_group_id() const { return context_group_id_; }
  v8_inspector::V8InspectorSession* v8_session() const {
    CHECK(v8_session_);
    return v8_session_.get();
  }

 private:
  DevToolsSession(int session_id, int context_group_id,
                  std::shared_ptr<v8_inspector::V8Inspector::Channel> channel);

  // ManagedChannel implementation.
  void sendResponse(
      int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void flushProtocolNotifications() override;

  static int last_session_id_;

  int session_id_;
  int context_group_id_;
  std::shared_ptr<v8_inspector::V8InspectorSession> v8_session_;
  std::shared_ptr<v8_inspector::V8Inspector::Channel> channel_;
  bool disconnected_ = false;

  friend class cppgc::MakeGarbageCollectedTrait<DevToolsSession>;
};

}  // namespace internal
}  // namespace v8

#endif  //  V8_TEST_INSPECTOR_DEVTOOLS_SESSION_
