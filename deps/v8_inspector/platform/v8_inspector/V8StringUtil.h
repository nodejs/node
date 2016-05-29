// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8StringUtil_h
#define V8StringUtil_h

#include "platform/inspector_protocol/String16.h"
#include <v8.h>

namespace blink {

v8::Local<v8::String> toV8String(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String16&);

String16 toProtocolString(v8::Local<v8::String>);
String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value>);

} //  namespace blink


#endif // !defined(V8StringUtil_h)
