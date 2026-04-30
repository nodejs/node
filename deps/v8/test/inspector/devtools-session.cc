// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/inspector/devtools-session.h"

#include <memory>

#include "include/cppgc/allocation.h"
#include "include/v8-cppgc.h"

namespace v8 {
namespace internal {

DevToolsSession* DevToolsSession::Connect(
    v8::Isolate* isolate, v8_inspector::V8Inspector* inspector,
    int context_group_id, const v8_inspector::StringView& state,
    v8_inspector::V8Inspector::ClientTrustLevel client_trust_level,
    v8_inspector::V8Inspector::SessionPauseState pause_state,
    std::shared_ptr<v8_inspector::V8Inspector::Channel> channel) {
  v8::SealHandleScope seal_handle_scope(isolate);
  DevToolsSession* session = cppgc::MakeGarbageCollected<DevToolsSession>(
      isolate->GetCppHeap()->GetAllocationHandle(), ++last_session_id_,
      context_group_id, std::move(channel));
  session->v8_session_ = inspector->connectShared(
      context_group_id, session, state, client_trust_level, pause_state);
  return session;
}

void DevToolsSession::Disconnect() {
  CHECK(v8_session_);
  v8_session_.reset();
  disconnected_ = true;
}

DevToolsSession::DevToolsSession(
    int session_id, int context_group_id,
    std::shared_ptr<v8_inspector::V8Inspector::Channel> channel)
    : session_id_(session_id),
      context_group_id_(context_group_id),
      channel_(std::move(channel)) {}

void DevToolsSession::sendResponse(
    int callId, std::unique_ptr<v8_inspector::StringBuffer> message) {
  if (disconnected_) return;

  channel_->sendResponse(callId, std::move(message));
}

void DevToolsSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  if (disconnected_) return;

  channel_->sendNotification(std::move(message));
}

void DevToolsSession::flushProtocolNotifications() {}

void DevToolsSession::Trace(cppgc::Visitor* visitor) const {
  v8_inspector::V8Inspector::ManagedChannel::Trace(visitor);
}

int DevToolsSession::last_session_id_ = 0;

}  // namespace internal
}  // namespace v8
