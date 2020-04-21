// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/wasm/gdb-server/gdb-server.h"

#include "src/debug/wasm/gdb-server/gdb-server-thread.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace gdb_server {

// static
std::unique_ptr<GdbServer> GdbServer::Create() {
  DCHECK(FLAG_wasm_gdb_remote);

  std::unique_ptr<GdbServer> gdb_server(new GdbServer());
  gdb_server->thread_ = std::make_unique<GdbServerThread>(gdb_server.get());
  if (!gdb_server->thread_->StartAndInitialize()) {
    TRACE_GDB_REMOTE(
        "Cannot initialize thread, GDB-remote debugging will be disabled.\n");
    return nullptr;
  }
  return gdb_server;
}

GdbServer::~GdbServer() {
  if (thread_) {
    thread_->Stop();
    thread_->Join();
  }
}

// All the following methods require interaction with the actual V8 Wasm engine.
// They will be implemented later.

std::vector<GdbServer::WasmModuleInfo> GdbServer::GetLoadedModules() const {
  // TODO(paolosev)
  return {};
}

bool GdbServer::GetWasmGlobal(uint32_t wasm_module_id, uint32_t index,
                              uint64_t* value) {
  // TODO(paolosev)
  return false;
}

bool GdbServer::GetWasmLocal(uint32_t wasm_module_id, uint32_t frame_index,
                             uint32_t index, uint64_t* value) {
  // TODO(paolosev)
  return false;
}

uint32_t GdbServer::GetWasmMemory(uint32_t wasm_module_id, uint32_t offset,
                                  uint8_t* buffer, uint32_t size) {
  // TODO(paolosev)
  return 0;
}

uint32_t GdbServer::GetWasmModuleBytes(wasm_addr_t address, uint8_t* buffer,
                                       uint32_t size) {
  // TODO(paolosev)
  return 0;
}

bool GdbServer::AddBreakpoint(uint32_t module_id, uint32_t offset) {
  // TODO(paolosev)
  return false;
}

bool GdbServer::RemoveBreakpoint(uint32_t module_id, uint32_t offset) {
  // TODO(paolosev)
  return false;
}

std::vector<wasm_addr_t> GdbServer::GetWasmCallStack() const {
  // TODO(paolosev)
  return {};
}

}  // namespace gdb_server
}  // namespace wasm
}  // namespace internal
}  // namespace v8
