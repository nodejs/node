// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorSession_h
#define V8InspectorSession_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/v8_inspector/protocol/Runtime.h"

#include <v8.h>

namespace blink {

class PLATFORM_EXPORT V8InspectorSession {
public:
    virtual ~V8InspectorSession() { }

    // Cross-context inspectable values (DOM nodes in different worlds, etc.).
    class Inspectable {
    public:
        virtual v8::Local<v8::Value> get(v8::Local<v8::Context>) = 0;
        virtual ~Inspectable() { }
    };
    virtual void addInspectedObject(std::unique_ptr<Inspectable>) = 0;

    // Dispatching protocol messages.
    // TODO(dgozman): generate this one.
    static bool isV8ProtocolMethod(const String16& method);
    virtual void dispatchProtocolMessage(const String16& message) = 0;
    virtual String16 stateJSON() = 0;

    // Debugger actions.
    virtual void schedulePauseOnNextStatement(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data) = 0;
    virtual void cancelPauseOnNextStatement() = 0;
    virtual void breakProgram(const String16& breakReason, std::unique_ptr<protocol::DictionaryValue> data) = 0;
    virtual void setSkipAllPauses(bool) = 0;
    virtual void resume() = 0;
    virtual void stepOver() = 0;

    // Remote objects.
    static const char backtraceObjectGroup[];
    virtual std::unique_ptr<protocol::Runtime::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String16& groupName, bool generatePreview) = 0;
    virtual v8::Local<v8::Value> findObject(ErrorString*, const String16& objectId, v8::Local<v8::Context>* = nullptr, String16* objectGroup = nullptr) = 0;
    virtual void releaseObjectGroup(const String16&) = 0;
};

} // namespace blink

#endif // V8InspectorSession_h
