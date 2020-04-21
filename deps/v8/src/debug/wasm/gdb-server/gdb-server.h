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
  // Factory method: creates and returns a GdbServer. Spawns a "GDB-remote"
  // thread that will be used to communicate with the debugger.
  // May return null on failure.
  // This should be called once, the first time a Wasm module is loaded in the
  // Wasm engine.
  static std::unique_ptr<GdbServer> Create();

  // Stops the "GDB-remote" thread and waits for it to complete. This should be
  // called once, when the Wasm engine shuts down.
  ~GdbServer();

  // Queries the set of the Wasm modules currently loaded. Each module is
  // identified by a unique integer module id.
  struct WasmModuleInfo {
    uint32_t module_id;
    std::string module_name;
  };
  std::vector<WasmModuleInfo> GetLoadedModules() const;

  // Queries the value of the {index} global value in the Wasm module identified
  // by {wasm_module_id}.
  bool GetWasmGlobal(uint32_t wasm_module_id, uint32_t index, uint64_t* value);

  // Queries the value of the {index} local value in the {frame_index}th stack
  // frame in the Wasm module identified by {wasm_module_id}.
  bool GetWasmLocal(uint32_t wasm_module_id, uint32_t frame_index,
                    uint32_t index, uint64_t* value);

  // Reads {size} bytes, starting from {offset}, from the Memory instance
  // associated to the Wasm module identified by {wasm_module_id}.
  // Returns the number of bytes copied to {buffer}, or 0 is case of error.
  // Note: only one Memory for Module is currently supported.
  uint32_t GetWasmMemory(uint32_t wasm_module_id, uint32_t offset,
                         uint8_t* buffer, uint32_t size);

  // Reads {size} bytes, starting from {address}, from the Code space of the
  // Wasm module identified by {wasm_module_id}.
  // Returns the number of bytes copied to {buffer}, or 0 is case of error.
  uint32_t GetWasmModuleBytes(wasm_addr_t address, uint8_t* buffer,
                              uint32_t size);

  // Inserts a breakpoint at the offset {offset} of the Wasm module identified
  // by {wasm_module_id}.
  // Returns true if the breakpoint was successfully added.
  bool AddBreakpoint(uint32_t module_id, uint32_t offset);

  // Removes a breakpoint at the offset {offset} of the Wasm module identified
  // by {wasm_module_id}.
  // Returns true if the breakpoint was successfully removed.
  bool RemoveBreakpoint(uint32_t module_id, uint32_t offset);

  // Returns the current call stack as a vector of program counters.
  std::vector<wasm_addr_t> GetWasmCallStack() const;

 private:
  GdbServer() {}

  std::unique_ptr<GdbServerThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(GdbServer);
};

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_WASM_GDB_SERVER_GDB_SERVER_H_
