// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_
#define V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_

#include <memory>
#include "src/debug/wasm/gdb-server/gdb-server-thread.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

// class GdbServer acts as a manager for the GDB-remote stub. It is instantiated
// as soon as the first Wasm module is loaded in the Wasm engine and spawns a
// separate thread to accept connections and exchange messages with a debugger.
// It will contain the logic to serve debugger queries and access the state of
// the Wasm engine.
class GdbServer {
 public:
  // Spawns a "GDB-remote" thread that will be used to communicate with the
  // debugger. This should be called once, the first time a Wasm module is
  // loaded in the Wasm engine.
  GdbServer();

  // Stops the "GDB-remote" thread and waits for it to complete. This should be
  // called once, when the Wasm engine shuts down.
  ~GdbServer();

 private:
  std::unique_ptr<GdbServerThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(GdbServer);
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_
