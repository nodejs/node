// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8StringUtil_h
#define V8StringUtil_h

#include "platform/inspector_protocol/InspectorProtocol.h"
#include "platform/v8_inspector/protocol/Debugger.h"
#include <v8.h>

namespace v8_inspector {

class V8InspectorSession;

namespace protocol = blink::protocol;

std::unique_ptr<protocol::Value> toProtocolValue(v8::Local<v8::Context>, v8::Local<v8::Value>, int maxDepth = protocol::Value::maxDepth);

v8::Local<v8::String> toV8String(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const String16&);
v8::Local<v8::String> toV8StringInternalized(v8::Isolate*, const char*);

String16 toProtocolString(v8::Local<v8::String>);
String16 toProtocolStringWithTypeCheck(v8::Local<v8::Value>);

String16 findSourceURL(const String16& content, bool multiline);
String16 findSourceMapURL(const String16& content, bool multiline);
std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>> searchInTextByLinesImpl(V8InspectorSession*, const String16& text, const String16& query, bool caseSensitive, bool isRegex);

} //  namespace v8_inspector


#endif // !defined(V8StringUtil_h)
