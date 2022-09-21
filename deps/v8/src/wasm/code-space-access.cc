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

namespace {
// For PKU and if MAP_JIT is available, the CodeSpaceWriteScope does not
// actually make use of the supplied {NativeModule}. In fact, there are
// situations where we can't provide a specific {NativeModule} to the scope. For
// those situations, we use this dummy pointer instead.
NativeModule* GetDummyNativeModule() {
  static struct alignas(NativeModule) DummyNativeModule {
    char content;
  } dummy_native_module;
  return reinterpret_cast<NativeModule*>(&dummy_native_module);
}
}  // namespace

thread_local NativeModule* CodeSpaceWriteScope::current_native_module_ =
    nullptr;

// TODO(jkummerow): Background threads could permanently stay in
// writable mode; only the main thread has to switch back and forth.
CodeSpaceWriteScope::CodeSpaceWriteScope(NativeModule* native_module)
    : previous_native_module_(current_native_module_) {
  if (!native_module) {
    // Passing in a {nullptr} is OK if we don't use that pointer anyway.
    // Internally, we need a non-nullptr though to know whether a scope is
    // already open from looking at {current_native_module_}.
    DCHECK(!SwitchingPerNativeModule());
    native_module = GetDummyNativeModule();
  }
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

// static
void CodeSpaceWriteScope::SetWritable() { RwxMemoryWriteScope::SetWritable(); }

// static
void CodeSpaceWriteScope::SetExecutable() {
  RwxMemoryWriteScope::SetExecutable();
}

// static
bool CodeSpaceWriteScope::SwitchingPerNativeModule() { return false; }

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// static
void CodeSpaceWriteScope::SetWritable() {
  if (WasmCodeManager::MemoryProtectionKeysEnabled()) {
    RwxMemoryWriteScope::SetWritable();
  } else if (v8_flags.wasm_write_protect_code_memory) {
    current_native_module_->AddWriter();
  }
}

// static
void CodeSpaceWriteScope::SetExecutable() {
  if (WasmCodeManager::MemoryProtectionKeysEnabled()) {
    DCHECK(v8_flags.wasm_memory_protection_keys);
    RwxMemoryWriteScope::SetExecutable();
  } else if (v8_flags.wasm_write_protect_code_memory) {
    current_native_module_->RemoveWriter();
  }
}

// static
bool CodeSpaceWriteScope::SwitchingPerNativeModule() {
  return !WasmCodeManager::MemoryProtectionKeysEnabled() &&
         v8_flags.wasm_write_protect_code_memory;
}

#endif  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
