// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectedContext_h
#define InspectedContext_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"
#include <v8.h>

namespace blink {

class InjectedScript;
class InjectedScriptHost;
class V8ContextInfo;
class V8DebuggerImpl;

class InspectedContext {
    PROTOCOL_DISALLOW_COPY(InspectedContext);
public:
    ~InspectedContext();

    v8::Local<v8::Context> context() const;
    int contextId() const { return m_contextId; }
    int contextGroupId() const { return m_contextGroupId; }
    bool isDefault() const { return m_isDefault; }
    String16 origin() const { return m_origin; }
    String16 humanReadableName() const { return m_humanReadableName; }
    String16 frameId() const { return m_frameId; }

    bool isReported() const { return m_reported; }
    void setReported(bool reported) { m_reported = reported; }

    v8::Isolate* isolate() const;
    V8DebuggerImpl* debugger() const { return m_debugger; }

    InjectedScript* getInjectedScript() { return m_injectedScript.get(); }
    void createInjectedScript();
    void discardInjectedScript();

private:
    friend class V8DebuggerImpl;
    InspectedContext(V8DebuggerImpl*, const V8ContextInfo&, int contextId);
    static void weakCallback(const v8::WeakCallbackInfo<InspectedContext>&);
    static void consoleWeakCallback(const v8::WeakCallbackInfo<InspectedContext>&);

    V8DebuggerImpl* m_debugger;
    v8::Global<v8::Context> m_context;
    int m_contextId;
    int m_contextGroupId;
    bool m_isDefault;
    const String16 m_origin;
    const String16 m_humanReadableName;
    const String16 m_frameId;
    bool m_reported;
    std::unique_ptr<InjectedScript> m_injectedScript;
    v8::Global<v8::Object> m_console;
};

} // namespace blink

#endif
