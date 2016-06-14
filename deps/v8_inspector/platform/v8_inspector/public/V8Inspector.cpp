// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "platform/v8_inspector/public/V8Inspector.h"

#include "platform/inspector_protocol/DispatcherBase.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

V8Inspector::V8Inspector(v8::Isolate* isolate, v8::Local<v8::Context> context)
{
    m_debugger = V8Debugger::create(isolate, this);
    m_debugger->contextCreated(V8ContextInfo(context, 1, true, "",
        "NodeJS Main Context", "", false));
}

V8Inspector::~V8Inspector()
{
    disconnectFrontend();
}

bool V8Inspector::callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target)
{
    return true;
}

String16 V8Inspector::valueSubtype(v8::Local<v8::Value> value)
{
    return String16();
}

bool V8Inspector::formatAccessorsAsProperties(v8::Local<v8::Value> value)
{
    return false;
}

void V8Inspector::connectFrontend(protocol::FrontendChannel* channel)
{
    m_session = m_debugger->connect(1, channel, this, &m_state);
}

void V8Inspector::disconnectFrontend()
{
    m_session.reset();
}

void V8Inspector::dispatchMessageFromFrontend(const String16& message)
{
    if (m_session)
        m_session->dispatchProtocolMessage(message);
}

int V8Inspector::ensureDefaultContextInGroup(int contextGroupId)
{
    return 1;
}

void V8Inspector::muteConsole()
{
}

void V8Inspector::unmuteConsole()
{

}

bool V8Inspector::isExecutionAllowed()
{
    return true;
}

} // namespace blink
