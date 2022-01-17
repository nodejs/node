// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/code-space-access.h"

#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"

namespace v8 {
namespace internal {
namespace wasm {

thread_local int CodeSpaceWriteScope::code_space_write_nesting_level_ = 0;
// The thread-local counter (above) is only valid if a single thread only works
// on one module at a time. This second thread-local checks that.
#if defined(DEBUG) && !V8_HAS_PTHREAD_JIT_WRITE_PROTECT
thread_local NativeModule* CodeSpaceWriteScope::current_native_module_ =
    nullptr;
#endif

// TODO(jkummerow): Background threads could permanently stay in
// writable mode; only the main thread has to switch back and forth.
#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT
CodeSpaceWriteScope::CodeSpaceWriteScope(NativeModule*) {
#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT
CodeSpaceWriteScope::CodeSpaceWriteScope(NativeModule* native_module)
    : native_module_(native_module) {
#ifdef DEBUG
  if (code_space_write_nesting_level_ == 0) {
    current_native_module_ = native_module;
  }
  DCHECK_EQ(native_module, current_native_module_);
#endif  // DEBUG
#endif  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT
  if (code_space_write_nesting_level_ == 0) SetWritable();
  code_space_write_nesting_level_++;
}

CodeSpaceWriteScope::~CodeSpaceWriteScope() {
  code_space_write_nesting_level_--;
  if (code_space_write_nesting_level_ == 0) SetExecutable();
}

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// Ignoring this warning is considered better than relying on
// __builtin_available.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
void CodeSpaceWriteScope::SetWritable() const {
  pthread_jit_write_protect_np(0);
}

void CodeSpaceWriteScope::SetExecutable() const {
  pthread_jit_write_protect_np(1);
}
#pragma clang diagnostic pop

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

void CodeSpaceWriteScope::SetWritable() const {
  DCHECK_NOT_NULL(native_module_);
  auto* code_manager = GetWasmCodeManager();
  if (code_manager->HasMemoryProtectionKeySupport()) {
    DCHECK(FLAG_wasm_memory_protection_keys);
    code_manager->SetThreadWritable(true);
  } else if (FLAG_wasm_write_protect_code_memory) {
    native_module_->AddWriter();
  }
}

void CodeSpaceWriteScope::SetExecutable() const {
  auto* code_manager = GetWasmCodeManager();
  if (code_manager->HasMemoryProtectionKeySupport()) {
    DCHECK(FLAG_wasm_memory_protection_keys);
    code_manager->SetThreadWritable(false);
  } else if (FLAG_wasm_write_protect_code_memory) {
    native_module_->RemoveWriter();
  }
}

#endif  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
