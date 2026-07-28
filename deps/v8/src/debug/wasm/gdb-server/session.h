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

class Packet;
class TransportBase;

// Represents a gdb-remote debugging session.
class V8_EXPORT_PRIVATE Session {
 public:
  explicit Session(TransportBase* transport);
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  // Attempt to send a packet and optionally wait for an ACK from the receiver.
  bool SendPacket(Packet* packet, bool expect_ack = true);

  // Attempt to receive a packet.
  bool GetPacket(Packet* packet);

  // Return true if there is data to read.
  bool IsDataAvailable() const;

  // Return true if the connection is still valid.
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

  // By default, when either the debugger or the GDB-stub sends a packet,
  // the first response expected is an acknowledgment: either '+' (to indicate
  // the packet was received correctly) or '-' (to request retransmission).
  // When a transport is reliable, the debugger may request that acknowledgement
  // be disabled by means of the 'QStartNoAckMode' packet.
  void EnableAck(bool ack_enabled) { ack_enabled_ = ack_enabled; }

 private:
  // Read a single character from the transport.
  bool GetChar(char* ch);

  // Read the content of a packet, from a leading '$' to a trailing '#'.
  bool GetPayload(Packet* pkt, uint8_t* checksum);

  TransportBase* io_;  // Transport object not owned by the Session.
  bool connected_;     // Is the connection still valid.
  bool ack_enabled_;   // If true, emit or wait for '+' from RSP stream.
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_SESSION_H_
