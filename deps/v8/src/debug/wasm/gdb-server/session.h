// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_SESSION_H_
#define V8_DEBUG_WASM_GDB_SERVER_SESSION_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

class Transport;

// Represents a gdb-remote debugging session.
class Session {
 public:
  explicit Session(Transport* transport);

  // Attempt to receive a packet.
  // For the moment this method is only used to check whether the TCP connection
  // is still active; all bytes read are discarded.
  bool GetPacket();

  // Return true if there is data to read.
  bool IsDataAvailable() const;

  // Return true if the connection still valid.
  bool IsConnected() const;

  // Shutdown the connection.
  void Disconnect();

  // When a debugging session is active, the GDB-remote thread can block waiting
  // for events and it will resume execution when one of these two events arise:
  // - A network event (a new packet arrives, or the connection is dropped)
  // - A thread event (the execution stopped because of a trap or breakpoint).
  void WaitForDebugStubEvent();

  // Signal that the debuggee execution stopped because of a trap or breakpoint.
  bool SignalThreadEvent();

 private:
  bool GetChar(char* ch);

  Transport* io_;   // Transport object not owned by the Session.
  bool connected_;  // Is the connection still valid.

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_SESSION_H_
