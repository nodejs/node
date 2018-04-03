// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WASM_ENGINE_H_
#define WASM_ENGINE_H_

#include <memory>

#include "src/wasm/compilation-manager.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-memory.h"

namespace v8 {
namespace internal {

namespace wasm {

// The central data structure that represents an engine instance capable of
// loading, instantiating, and executing WASM code.
class V8_EXPORT_PRIVATE WasmEngine {
 public:
  explicit WasmEngine(std::unique_ptr<WasmCodeManager> code_manager)
      : code_manager_(std::move(code_manager)) {}

  bool SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes);

  CompilationManager* compilation_manager() { return &compilation_manager_; }

  WasmCodeManager* code_manager() const { return code_manager_.get(); }

  WasmAllocationTracker* allocation_tracker() { return &allocation_tracker_; }

 private:
  CompilationManager compilation_manager_;
  std::unique_ptr<WasmCodeManager> code_manager_;
  WasmAllocationTracker allocation_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WasmEngine);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif
