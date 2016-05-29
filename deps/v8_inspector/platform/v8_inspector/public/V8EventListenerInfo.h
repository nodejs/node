// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8EventListenerInfo_h
#define V8EventListenerInfo_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/String16.h"

#include <v8.h>

namespace blink {

class V8EventListenerInfo {
public:
    V8EventListenerInfo(const String16& eventType, bool useCapture, bool passive, v8::Local<v8::Object> handler)
        : eventType(eventType)
        , useCapture(useCapture)
        , passive(passive)
        , handler(handler)
    {
    }

    const String16 eventType;
    bool useCapture;
    bool passive;
    v8::Local<v8::Object> handler;

};

using V8EventListenerInfoList = protocol::Vector<V8EventListenerInfo>;

} // namespace blink

#endif // V8EventListenerInfo_h
