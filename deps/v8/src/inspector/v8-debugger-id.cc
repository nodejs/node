// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-id.h"

#include "src/debug/debug-interface.h"
#include "src/inspector/v8-inspector-impl.h"

namespace v8_inspector {

V8DebuggerId::V8DebuggerId(std::pair<int64_t, int64_t> pair)
    : m_first(pair.first), m_second(pair.second) {}

// static
V8DebuggerId V8DebuggerId::generate(V8InspectorImpl* inspector) {
  return V8DebuggerId(std::make_pair(inspector->generateUniqueId(),
                                     inspector->generateUniqueId()));
}

V8DebuggerId::V8DebuggerId(const String16& debuggerId) {
  const UChar dot = '.';
  size_t pos = debuggerId.find(dot);
  if (pos == String16::kNotFound) return;
  bool ok = false;
  int64_t first = debuggerId.substring(0, pos).toInteger64(&ok);
  if (!ok) return;
  int64_t second = debuggerId.substring(pos + 1).toInteger64(&ok);
  if (!ok) return;
  m_first = first;
  m_second = second;
}

String16 V8DebuggerId::toString() const {
  return String16::fromInteger64(m_first) + "." +
         String16::fromInteger64(m_second);
}

bool V8DebuggerId::isValid() const { return m_first || m_second; }

std::pair<int64_t, int64_t> V8DebuggerId::pair() const {
  return std::make_pair(m_first, m_second);
}

}  // namespace v8_inspector
