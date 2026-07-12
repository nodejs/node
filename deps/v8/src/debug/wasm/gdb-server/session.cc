// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/session.h"
#include "src/debug/wasm/gdb-server/packet.h"
#include "src/debug/wasm/gdb-server/transport.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

Session::Session(TransportBase* transport)
    : io_(transport), connected_(true), ack_enabled_(true) {}

void Session::WaitForDebugStubEvent() { io_->WaitForDebugStubEvent(); }

bool Session::SignalThreadEvent() { return io_->SignalThreadEvent(); }

bool Session::IsDataAvailable() const { return io_->IsDataAvailable(); }

bool Session::IsConnected() const { return connected_; }

void Session::Disconnect() {
  io_->Disconnect();
  connected_ = false;
}

bool Session::GetChar(char* ch) {
  if (!io_->Read(ch, 1)) {
    Disconnect();
    return false;
  }

  return true;
}

bool Session::SendPacket(Packet* pkt, bool expect_ack) {
  char ch;
  do {
    std::string data = pkt->GetPacketData();

    TRACE_GDB_REMOTE("TX %s\n", data.size() < 160
                                    ? data.c_str()
                                    : (data.substr(0, 160) + "...").c_str());
    if (!io_->Write(data.data(), static_cast<int32_t>(data.length()))) {
      return false;
    }

    // If ACKs are off, we are done.
    if (!expect_ack || !ack_enabled_) {
      break;
    }

    // Otherwise, poll for '+'
    if (!GetChar(&ch)) {
      return false;
    }

    // Retry if we didn't get a '+'
  } while (ch != '+');

  return true;
}

bool Session::GetPayload(Packet* pkt, uint8_t* checksum) {
  pkt->Clear();
  *checksum = 0;

  // Stream in the characters
  char ch;
  while (GetChar(&ch)) {
    if (ch == '#') {
      // If we see a '#' we must be done with the data.
      return true;
    } else if (ch == '$') {
      // If we see a '$' we must have missed the last cmd, let's retry.
      TRACE_GDB_REMOTE("RX Missing $, retry.\n");
      *checksum = 0;
      pkt->Clear();
    } else {
      // Keep a running XSUM.
      *checksum += ch;
      pkt->AddRawChar(ch);
    }
  }
  return false;
}

bool Session::GetPacket(Packet* pkt) {
  while (true) {
    // Toss characters until we see a start of command
    char ch;
    do {
      if (!GetChar(&ch)) {
        return false;
      }
    } while (ch != '$');

    uint8_t running_checksum = 0;
    if (!GetPayload(pkt, &running_checksum)) {
      return false;
    }

    // Get two nibble checksum
    uint8_t trailing_checksum = 0;
    char chars[2];
    if (!GetChar(&chars[0]) || !GetChar(&chars[1]) ||
        !HexToUInt8(chars, &trailing_checksum)) {
      return false;
    }

    TRACE_GDB_REMOTE("RX $%s#%c%c\n", pkt->GetPayload(), chars[0], chars[1]);

    pkt->ParseSequence();

    // If ACKs are off, we are done.
    if (!ack_enabled_) {
      return true;
    }

    // If the XSUMs don't match, signal bad packet
    if (trailing_checksum == running_checksum) {
      char out[3] = {'+', 0, 0};

      // If we have a sequence number
      int32_t seq;
      if (pkt->GetSequence(&seq)) {
        // Respond with sequence number
        UInt8ToHex(seq, &out[1]);
        return io_->Write(out, 3);
      } else {
        return io_->Write(out, 1);
      }
    } else {
      // Resend a bad XSUM and look for retransmit
      TRACE_GDB_REMOTE("RX Bad XSUM, retry\n");
      io_->Write("-", 1);
      // retry...
    }
  }
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
