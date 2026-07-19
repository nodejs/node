// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEBUGGER_ID_H_
#define V8_INSPECTOR_V8_DEBUGGER_ID_H_

#include <utility>

#include "include/v8-inspector.h"
#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"

namespace v8_inspector {
class V8InspectorImpl;

namespace internal {

class V8DebuggerId {
 public:
  V8DebuggerId() = default;
  explicit V8DebuggerId(std::pair<int64_t, int64_t>);
  explicit V8DebuggerId(const String16&);
  V8DebuggerId(const V8DebuggerId&) V8_NOEXCEPT = default;
  V8DebuggerId& operator=(const V8DebuggerId&) V8_NOEXCEPT = default;

  static V8DebuggerId generate(V8InspectorImpl*);

  v8_inspector::V8DebuggerId toV8DebuggerId() const { return m_debugger_id; }
  String16 toString() const;
  bool isValid() const;
  std::pair<int64_t, int64_t> pair() const;

 private:
  v8_inspector::V8DebuggerId m_debugger_id;
};

}  // namespace internal
}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEBUGGER_ID_H_
