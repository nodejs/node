// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-console-agent-impl.h"

#include "src/inspector/protocol/Protocol.h"
#include "src/inspector/v8-console-message.h"
#include "src/inspector/v8-inspector-impl.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"

namespace v8_inspector {

namespace ConsoleAgentState {
static const char consoleEnabled[] = "consoleEnabled";
}  // namespace ConsoleAgentState

V8ConsoleAgentImpl::V8ConsoleAgentImpl(
    V8InspectorSessionImpl* session, protocol::FrontendChannel* frontendChannel,
    protocol::DictionaryValue* state)
    : m_session(session),
      m_state(state),
      m_frontend(frontendChannel),
      m_enabled(false) {}

V8ConsoleAgentImpl::~V8ConsoleAgentImpl() = default;

Response V8ConsoleAgentImpl::enable() {
  if (m_enabled) return Response::Success();
  m_state->setBoolean(ConsoleAgentState::consoleEnabled, true);
  m_enabled = true;
  m_session->inspector()->enableStackCapturingIfNeeded();
  reportAllMessages();
  return Response::Success();
}

Response V8ConsoleAgentImpl::disable() {
  if (!m_enabled) return Response::Success();
  m_session->inspector()->disableStackCapturingIfNeeded();
  m_state->setBoolean(ConsoleAgentState::consoleEnabled, false);
  m_enabled = false;
  return Response::Success();
}

Response V8ConsoleAgentImpl::clearMessages() { return Response::Success(); }

void V8ConsoleAgentImpl::restore() {
  if (!m_state->booleanProperty(ConsoleAgentState::consoleEnabled, false))
    return;
  enable();
}

void V8ConsoleAgentImpl::messageAdded(V8ConsoleMessage* message) {
  if (m_enabled) reportMessage(message, true);
}

bool V8ConsoleAgentImpl::enabled() { return m_enabled; }

void V8ConsoleAgentImpl::reportAllMessages() {
  V8ConsoleMessageStorage* storage =
      m_session->inspector()->ensureConsoleMessageStorage(
          m_session->contextGroupId());
  for (const auto& message : storage->messages()) {
    if (message->origin() == V8MessageOrigin::kConsole) {
      if (!reportMessage(message.get(), false)) return;
    }
  }
}

bool V8ConsoleAgentImpl::reportMessage(V8ConsoleMessage* message,
                                       bool generatePreview) {
  DCHECK_EQ(V8MessageOrigin::kConsole, message->origin());
  message->reportToFrontend(&m_frontend);
  m_frontend.flush();
  return m_session->inspector()->hasConsoleMessageStorage(
      m_session->contextGroupId());
}

}  // namespace v8_inspector
