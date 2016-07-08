// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ContextInfo_h
#define V8ContextInfo_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"

#include <v8.h>

namespace blink {

class V8ContextInfo {
public:
    V8ContextInfo(v8::Local<v8::Context> context, int contextGroupId, bool isDefault, const String16& origin, const String16& humanReadableName, const String16& frameId, bool hasMemoryOnConsole)
        : context(context)
        , contextGroupId(contextGroupId)
        , isDefault(isDefault)
        , origin(origin)
        , humanReadableName(humanReadableName)
        , frameId(frameId)
        , hasMemoryOnConsole(hasMemoryOnConsole)
    {
    }

    v8::Local<v8::Context> context;
    // Each v8::Context is a part of a group. The group id is used to find appropriate
    // V8DebuggerAgent to notify about events in the context.
    // |contextGroupId| must be non-0.
    int contextGroupId;
    bool isDefault;
    const String16 origin;
    const String16 humanReadableName;
    const String16 frameId;
    bool hasMemoryOnConsole;
};

} // namespace blink

#endif // V8ContextInfo_h
