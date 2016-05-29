// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Inspector_h
#define V8Inspector_h

#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "platform/v8_inspector/public/V8InspectorSessionClient.h"
#include "platform/v8_inspector/public/V8InspectorSession.h"

#include "wtf/PtrUtil.h"
#include <v8.h>

namespace blink {

namespace protocol {
class Dispatcher;
class Frontend;
class FrontendChannel;
}

class V8Debugger;
class V8HeapProfilerAgent;
class V8ProfilerAgent;

class V8Inspector : public V8DebuggerClient, V8InspectorSessionClient {
public:
    V8Inspector(v8::Isolate*, v8::Local<v8::Context>);
    ~V8Inspector();

    // Transport interface.
    void connectFrontend(protocol::FrontendChannel*);
    void disconnectFrontend();
    void dispatchMessageFromFrontend(const String16& message);

private:
    void eventListeners(v8::Local<v8::Value>, V8EventListenerInfoList&) override;
    bool callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target) override;
    String16 valueSubtype(v8::Local<v8::Value>) override;
    bool formatAccessorsAsProperties(v8::Local<v8::Value>) override;
    void muteWarningsAndDeprecations() override { }
    void unmuteWarningsAndDeprecations() override { }
    double currentTimeMS() override { return 0; };

    void muteConsole() override;
    void unmuteConsole() override;
    bool isExecutionAllowed() override;
    int ensureDefaultContextInGroup(int contextGroupId) override;
    void beginUserGesture() override { }
    void endUserGesture() override { }
    bool isInspectableHeapObject(v8::Local<v8::Object>) override { return true; }
    void consoleTime(const String16& title) override { }
    void consoleTimeEnd(const String16& title) override { }
    void consoleTimeStamp(const String16& title) override { }
    v8::MaybeLocal<v8::Value> memoryInfo(v8::Isolate*, v8::Local<v8::Context>, v8::Local<v8::Object> creationContext) override
    {
        return v8::MaybeLocal<v8::Value>();
    }
    void reportMessageToConsole(v8::Local<v8::Context>, MessageType, MessageLevel, const String16& message, const v8::FunctionCallbackInfo<v8::Value>* arguments, unsigned skipArgumentCount, int maxStackSize) override { }

    // V8InspectorSessionClient
    void startInstrumenting() override { }
    void stopInstrumenting() override { }
    void resumeStartup() override { }
    bool canExecuteScripts() override { return true; }
    void profilingStarted() override { }
    void profilingStopped() override { }
    void startRepeatingTimer(double, TimerCallback, void* data) override { }
    void cancelTimer(void* data) override { }

    std::unique_ptr<V8Debugger> m_debugger;
    std::unique_ptr<V8InspectorSession> m_session;
    std::unique_ptr<protocol::Dispatcher> m_dispatcher;
    std::unique_ptr<protocol::Frontend> m_frontend;
    std::unique_ptr<protocol::DictionaryValue> m_state;
};

}

#endif // V8Inspector_h
