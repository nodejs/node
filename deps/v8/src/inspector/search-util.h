// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_SEARCHUTIL_H_
#define V8_INSPECTOR_SEARCHUTIL_H_

#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/string-util.h"

namespace v8_inspector {

class V8InspectorSession;

String16 findSourceURL(const String16& content, bool multiline);
String16 findSourceMapURL(const String16& content, bool multiline);
std::vector<std::unique_ptr<protocol::Debugger::SearchMatch>>
searchInTextByLinesImpl(V8InspectorSession*, const String16& text,
                        const String16& query, bool caseSensitive,
                        bool isRegex);

}  //  namespace v8_inspector

#endif  // V8_INSPECTOR_SEARCHUTIL_H_
