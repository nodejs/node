// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_CONSOLE_AGENT_IMPL_H_
#define V8_INSPECTOR_V8_CONSOLE_AGENT_IMPL_H_

#include "src/base/macros.h"
#include "src/inspector/protocol/Console.h"
#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {

class V8ConsoleMessage;
class V8InspectorSessionImpl;

using protocol::Response;

class V8ConsoleAgentImpl : public protocol::Console::Backend {
 public:
  V8ConsoleAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                     protocol::DictionaryValue* state);
  ~V8ConsoleAgentImpl() override;

  Response enable() override;
  Response disable() override;
  Response clearMessages() override;

  void restore();
  void messageAdded(V8ConsoleMessage*);
  void reset();
  bool enabled();

 private:
  void reportAllMessages();
  bool reportMessage(V8ConsoleMessage*, bool generatePreview);

  V8InspectorSessionImpl* m_session;
  protocol::DictionaryValue* m_state;
  protocol::Console::Frontend m_frontend;
  bool m_enabled;

  DISALLOW_COPY_AND_ASSIGN(V8ConsoleAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_CONSOLE_AGENT_IMPL_H_
