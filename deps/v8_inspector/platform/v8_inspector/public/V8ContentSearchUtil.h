// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8ContentSearchUtil_h
#define V8ContentSearchUtil_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"

namespace blink {

class V8InspectorSession;

namespace V8ContentSearchUtil {

PLATFORM_EXPORT String16 findSourceURL(const String16& content, bool multiline, bool* deprecated = nullptr);
PLATFORM_EXPORT String16 findSourceMapURL(const String16& content, bool multiline, bool* deprecated = nullptr);
PLATFORM_EXPORT std::unique_ptr<protocol::Array<protocol::Debugger::SearchMatch>> searchInTextByLines(V8InspectorSession*, const String16& text, const String16& query, const bool caseSensitive, const bool isRegex);

}

}

#endif // !defined(V8ContentSearchUtil_h)
