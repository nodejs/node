// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/code-space-access.h"

#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {

thread_local NativeModule* CodeSpaceWriteScope::current_native_module_ =
    nullptr;

// TODO(jkummerow): Background threads could permanently stay in
// writable mode; only the main thread has to switch back and forth.
CodeSpaceWriteScope::CodeSpaceWriteScope(NativeModule* native_module)
    : previous_native_module_(current_native_module_) {
  DCHECK_NOT_NULL(native_module);
  if (previous_native_module_ == native_module) return;
  current_native_module_ = native_module;
  if (previous_native_module_ == nullptr || SwitchingPerNativeModule()) {
    SetWritable();
  }
}

CodeSpaceWriteScope::~CodeSpaceWriteScope() {
  if (previous_native_module_ == current_native_module_) return;
  if (previous_native_module_ == nullptr || SwitchingPerNativeModule()) {
    SetExecutable();
  }
  current_native_module_ = previous_native_module_;
}

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// Ignoring this warning is considered better than relying on
// __builtin_available.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
// static
void CodeSpaceWriteScope::SetWritable() {
  pthread_jit_write_protect_np(0);
}

// static
void CodeSpaceWriteScope::SetExecutable() {
  pthread_jit_write_protect_np(1);
}
#pragma clang diagnostic pop

// static
bool CodeSpaceWriteScope::SwitchingPerNativeModule() { return false; }

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// static
void CodeSpaceWriteScope::SetWritable() {
  auto* code_manager = GetWasmCodeManager();
  if (code_manager->MemoryProtectionKeysEnabled()) {
    code_manager->SetThreadWritable(true);
  } else if (FLAG_wasm_write_protect_code_memory) {
    current_native_module_->AddWriter();
  }
}

// static
void CodeSpaceWriteScope::SetExecutable() {
  auto* code_manager = GetWasmCodeManager();
  if (code_manager->MemoryProtectionKeysEnabled()) {
    DCHECK(FLAG_wasm_memory_protection_keys);
    code_manager->SetThreadWritable(false);
  } else if (FLAG_wasm_write_protect_code_memory) {
    current_native_module_->RemoveWriter();
  }
}

// static
bool CodeSpaceWriteScope::SwitchingPerNativeModule() {
  return !GetWasmCodeManager()->MemoryProtectionKeysEnabled() &&
         FLAG_wasm_write_protect_code_memory;
}

#endif  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
