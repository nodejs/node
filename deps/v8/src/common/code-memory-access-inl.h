// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_CODE_MEMORY_ACCESS_INL_H_
#define V8_COMMON_CODE_MEMORY_ACCESS_INL_H_

#include "src/common/code-memory-access.h"
#include "src/flags/flags.h"
#if V8_HAS_PKU_JIT_WRITE_PROTECT
#include "src/base/platform/memory-protection-key.h"
#endif
#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT
#include "src/base/platform/platform.h"
#endif

namespace v8 {
namespace internal {

RwxMemoryWriteScope::RwxMemoryWriteScope(const char* comment) {
  DCHECK(is_key_permissions_initialized_for_current_thread());
  if (!v8_flags.jitless) {
    SetWritable();
  }
}

RwxMemoryWriteScope::~RwxMemoryWriteScope() {
  if (!v8_flags.jitless) {
    SetExecutable();
  }
}

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT

// static
bool RwxMemoryWriteScope::IsSupported() { return true; }

// static
void RwxMemoryWriteScope::SetWritable() {
  if (code_space_write_nesting_level_ == 0) {
    base::SetJitWriteProtected(0);
  }
  code_space_write_nesting_level_++;
}

// static
void RwxMemoryWriteScope::SetExecutable() {
  code_space_write_nesting_level_--;
  if (code_space_write_nesting_level_ == 0) {
    base::SetJitWriteProtected(1);
  }
}

#elif V8_HAS_PKU_JIT_WRITE_PROTECT
// static
bool RwxMemoryWriteScope::IsSupported() {
  static_assert(base::MemoryProtectionKey::kNoMemoryProtectionKey == -1);
  DCHECK(pkey_initialized);
  return memory_protection_key_ >= 0;
}

// static
void RwxMemoryWriteScope::SetWritable() {
  DCHECK(pkey_initialized);
  if (!IsSupported()) return;
  if (code_space_write_nesting_level_ == 0) {
    DCHECK_NE(
        base::MemoryProtectionKey::GetKeyPermission(memory_protection_key_),
        base::MemoryProtectionKey::kNoRestrictions);
    base::MemoryProtectionKey::SetPermissionsForKey(
        memory_protection_key_, base::MemoryProtectionKey::kNoRestrictions);
  }
  code_space_write_nesting_level_++;
}

// static
void RwxMemoryWriteScope::SetExecutable() {
  DCHECK(pkey_initialized);
  if (!IsSupported()) return;
  code_space_write_nesting_level_--;
  if (code_space_write_nesting_level_ == 0) {
    DCHECK_EQ(
        base::MemoryProtectionKey::GetKeyPermission(memory_protection_key_),
        base::MemoryProtectionKey::kNoRestrictions);
    base::MemoryProtectionKey::SetPermissionsForKey(
        memory_protection_key_, base::MemoryProtectionKey::kDisableWrite);
  }
}

#else  // !V8_HAS_PTHREAD_JIT_WRITE_PROTECT && !V8_TRY_USE_PKU_JIT_WRITE_PROTECT

// static
bool RwxMemoryWriteScope::IsSupported() { return false; }

// static
void RwxMemoryWriteScope::SetWritable() {}

// static
void RwxMemoryWriteScope::SetExecutable() {}

#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_CODE_MEMORY_ACCESS_INL_H_
