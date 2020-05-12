// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/session.h"
#include "src/debug/wasm/gdb-server/transport.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

Session::Session(Transport* transport) : io_(transport), connected_(true) {}

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

bool Session::GetPacket() {
  char ch;
  if (!GetChar(&ch)) return false;

  // discard the input
  return true;
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
