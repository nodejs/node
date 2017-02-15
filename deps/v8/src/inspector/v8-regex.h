// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8REGEX_H_
#define V8_INSPECTOR_V8REGEX_H_

#include "src/base/macros.h"
#include "src/inspector/string-16.h"

#include "include/v8.h"

namespace v8_inspector {

class V8InspectorImpl;

enum MultilineMode { MultilineDisabled, MultilineEnabled };

class V8Regex {
 public:
  V8Regex(V8InspectorImpl*, const String16&, bool caseSensitive,
          bool multiline = false);
  int match(const String16&, int startFrom = 0, int* matchLength = 0) const;
  bool isValid() const { return !m_regex.IsEmpty(); }
  const String16& errorMessage() const { return m_errorMessage; }

 private:
  V8InspectorImpl* m_inspector;
  v8::Global<v8::RegExp> m_regex;
  String16 m_errorMessage;

  DISALLOW_COPY_AND_ASSIGN(V8Regex);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8REGEX_H_
