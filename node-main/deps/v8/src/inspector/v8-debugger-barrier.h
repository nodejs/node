// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEBUGGER_BARRIER_H_
#define V8_INSPECTOR_V8_DEBUGGER_BARRIER_H_

namespace v8_inspector {

class V8InspectorClient;

// This class is used to synchronize multiple sessions issuing
// `Runtime.runIfWaitingForDebbuger` so that the global client
// `runIfWaitingForDebugger` method is only invoked when all
// sessions have invoked `Runtime.runIfWaitingForDebugger`.
class V8DebuggerBarrier {
 public:
  V8DebuggerBarrier(V8InspectorClient* client, int contextGroupId);
  ~V8DebuggerBarrier();

 private:
  V8InspectorClient* const m_client;
  int m_contextGroupId;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEBUGGER_BARRIER_H_
