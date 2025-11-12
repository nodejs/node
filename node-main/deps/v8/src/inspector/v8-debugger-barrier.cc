// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-debugger-barrier.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

V8DebuggerBarrier::V8DebuggerBarrier(V8InspectorClient* client,
                                     int contextGroupId)
    : m_client(client), m_contextGroupId(contextGroupId) {}

V8DebuggerBarrier::~V8DebuggerBarrier() {
  m_client->runIfWaitingForDebugger(m_contextGroupId);
}

}  // namespace v8_inspector
