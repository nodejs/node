// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/target.h"

#include "src/base/platform/time.h"
#include "src/debug/wasm/gdb-server/gdb-server.h"
#include "src/debug/wasm/gdb-server/session.h"
#include "src/debug/wasm/gdb-server/transport.h"
#include "src/debug/wasm/gdb-server/util.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

Target::Target(GdbServer* gdb_server)
    : status_(Status::Running), session_(nullptr) {}

void Target::Terminate() {
  // Executed in the Isolate thread.
  status_ = Status::Terminated;
}

void Target::Run(Session* session) {
  // Executed in the GdbServer thread.

  session_ = session;
  do {
    WaitForDebugEvent();
    ProcessCommands();
  } while (!IsTerminated() && session_->IsConnected());
  session_ = nullptr;
}

void Target::WaitForDebugEvent() {
  // Executed in the GdbServer thread.

  if (status_ != Status::Terminated) {
    // Wait for either:
    //   * the thread to fault (or single-step)
    //   * an interrupt from LLDB
    session_->WaitForDebugStubEvent();
  }
}

void Target::ProcessCommands() {
  // GDB-remote messages are processed in the GDBServer thread.

  if (IsTerminated()) {
    return;
  }

  // TODO(paolosev)
  // For the moment just discard any packet we receive from the debugger.
  do {
    if (!session_->GetPacket()) continue;
  } while (session_->IsConnected());
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
