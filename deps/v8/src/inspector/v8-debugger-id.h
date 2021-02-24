// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEBUGGER_ID_H_
#define V8_INSPECTOR_V8_DEBUGGER_ID_H_

#include <utility>

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {

class V8InspectorImpl;

// This debugger id tries to be unique by generating two random
// numbers, which should most likely avoid collisions.
// Debugger id has a 1:1 mapping to context group. It is used to
// attribute stack traces to a particular debugging, when doing any
// cross-debugger operations (e.g. async step in).
// See also Runtime.UniqueDebuggerId in the protocol.
class V8DebuggerId {
 public:
  V8DebuggerId() = default;
  explicit V8DebuggerId(std::pair<int64_t, int64_t>);
  explicit V8DebuggerId(const String16&);
  V8DebuggerId(const V8DebuggerId&) V8_NOEXCEPT = default;
  ~V8DebuggerId() = default;

  static V8DebuggerId generate(V8InspectorImpl*);

  String16 toString() const;
  bool isValid() const;
  std::pair<int64_t, int64_t> pair() const;

 private:
  int64_t m_first = 0;
  int64_t m_second = 0;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEBUGGER_ID_H_
