// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8StackTrace_h
#define V8StackTrace_h

#include "platform/inspector_protocol/TypeBuilder.h"
#include "wtf/PtrUtil.h"

#include <v8.h>

namespace blink {

class TracedValue;

const v8::StackTrace::StackTraceOptions stackTraceOptions = static_cast<v8::StackTrace::StackTraceOptions>(
    v8::StackTrace::kLineNumber |
    v8::StackTrace::kColumnOffset |
    v8::StackTrace::kScriptId |
    v8::StackTrace::kScriptNameOrSourceURL |
    v8::StackTrace::kFunctionName);

class V8StackTrace {
public:
    static const size_t maxCallStackSizeToCapture = 200;

    virtual bool isEmpty() const = 0;
    virtual String16 topSourceURL() const = 0;
    virtual int topLineNumber() const = 0;
    virtual int topColumnNumber() const = 0;
    virtual String16 topScriptId() const = 0;
    virtual String16 topFunctionName() const = 0;

    virtual ~V8StackTrace() { }
    virtual std::unique_ptr<protocol::Runtime::StackTrace> buildInspectorObject() const = 0;
    virtual String16 toString() const = 0;
};

} // namespace blink

#endif // V8StackTrace_h
