// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Debugger_h
#define V8Debugger_h

#include "platform/PlatformExport.h"
#include "platform/inspector_protocol/Frontend.h"
#include "wtf/PtrUtil.h"

#include <v8.h>

namespace blink {

class V8ContextInfo;
class V8DebuggerClient;
class V8InspectorSession;
class V8StackTrace;

namespace protocol {
class DictionaryValue;
}

class PLATFORM_EXPORT V8Debugger {
public:
    template <typename T>
    class Agent {
    public:
        virtual void setInspectorState(protocol::DictionaryValue*) = 0;
        virtual void setFrontend(T*) = 0;
        virtual void clearFrontend() = 0;
        virtual void restore() = 0;
    };

    static std::unique_ptr<V8Debugger> create(v8::Isolate*, V8DebuggerClient*);
    virtual ~V8Debugger() { }

    // TODO(dgozman): make this non-static?
    static int contextId(v8::Local<v8::Context>);

    virtual void contextCreated(const V8ContextInfo&) = 0;
    virtual void contextDestroyed(v8::Local<v8::Context>) = 0;
    // TODO(dgozman): remove this one.
    virtual void resetContextGroup(int contextGroupId) = 0;
    virtual void willExecuteScript(v8::Local<v8::Context>, int scriptId) = 0;
    virtual void didExecuteScript(v8::Local<v8::Context>) = 0;
    virtual void idleStarted() = 0;
    virtual void idleFinished() = 0;

    virtual std::unique_ptr<V8InspectorSession> connect(int contextGroupId) = 0;
    virtual bool isPaused() = 0;

    static v8::Local<v8::Private> scopeExtensionPrivate(v8::Isolate*);
    static bool isCommandLineAPIMethod(const String16& name);
    static bool isCommandLineAPIGetter(const String16& name);

    virtual std::unique_ptr<V8StackTrace> createStackTrace(v8::Local<v8::StackTrace>, size_t maxStackSize) = 0;
    virtual std::unique_ptr<V8StackTrace> captureStackTrace(size_t maxStackSize) = 0;
};

} // namespace blink


#endif // V8Debugger_h
