// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ConsoleAgentImpl_h
#define V8ConsoleAgentImpl_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/protocol/Console.h"

namespace blink {

class V8ConsoleMessage;
class V8InspectorSessionImpl;

class V8ConsoleAgentImpl : public protocol::Console::Backend {
    PROTOCOL_DISALLOW_COPY(V8ConsoleAgentImpl);
public:
    V8ConsoleAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*, protocol::DictionaryValue* state);
    ~V8ConsoleAgentImpl() override;

    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void clearMessages(ErrorString*) override;

    void restore();
    void messageAdded(V8ConsoleMessage*);
    void reset();
    bool enabled();

private:
    void reportAllMessages();
    void reportMessage(V8ConsoleMessage*, bool generatePreview);

    V8InspectorSessionImpl* m_session;
    protocol::DictionaryValue* m_state;
    protocol::Console::Frontend m_frontend;
    bool m_enabled;
};

} // namespace blink


#endif // !defined(V8ConsoleAgentImpl_h)
