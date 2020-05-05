// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_TARGET_H_
#define V8_DEBUG_WASM_GDB_SERVER_TARGET_H_

#include <atomic>
#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

class GdbServer;
class Session;

// Class Target represents a debugging target. It contains the logic to decode
// incoming GDB-remote packets, execute them forwarding the debugger commands
// and queries to the Wasm engine, and send back GDB-remote packets.
class Target {
 public:
  // Contruct a Target object.
  explicit Target(GdbServer* gdb_server);

  // This function spin on a debugging session, until it closes.
  void Run(Session* ses);

  void Terminate();
  bool IsTerminated() const { return status_ == Status::Terminated; }

 private:
  // Blocks waiting for one of these two events to occur:
  // - A network packet arrives from the debugger, or the debugger connection is
  //   closed;
  // - The debuggee suspends execution because of a trap or breakpoint.
  void WaitForDebugEvent();

  // Processes GDB-remote packets that arrive from the debugger.
  // This method should be called when the debuggee has suspended its execution.
  void ProcessCommands();

  enum class Status { Running, Terminated };
  std::atomic<Status> status_;

  Session* session_;  // Session object not owned by the Target.

  DISALLOW_COPY_AND_ASSIGN(Target);
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_TARGET_H_
