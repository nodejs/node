// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/code-space-access.h"

#include "src/common/code-memory-access-inl.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {

CodeSpaceWriteScope::CodeSpaceWriteScope() { SetWritable(); }

CodeSpaceWriteScope::~CodeSpaceWriteScope() { SetExecutable(); }

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// static
void CodeSpaceWriteScope::SetWritable() { RwxMemoryWriteScope::SetWritable(); }

// static
void CodeSpaceWriteScope::SetExecutable() {
  RwxMemoryWriteScope::SetExecutable();
}

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// static
void CodeSpaceWriteScope::SetWritable() {
  if (WasmCodeManager::MemoryProtectionKeysEnabled()) {
    DCHECK(!WasmCodeManager::MemoryProtectionKeyWritable());
    RwxMemoryWriteScope::SetWritable();
  }
}

// static
void CodeSpaceWriteScope::SetExecutable() {
  if (WasmCodeManager::MemoryProtectionKeysEnabled()) {
    DCHECK(v8_flags.wasm_memory_protection_keys);
    RwxMemoryWriteScope::SetExecutable();
  }
}

#endif  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
