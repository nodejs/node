// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/code-memory-access-inl.h"

namespace v8 {
namespace internal {

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT
thread_local int RwxMemoryWriteScope::code_space_write_nesting_level_ = 0;
#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT

#if V8_HAS_PKU_JIT_WRITE_PROTECT
int RwxMemoryWriteScope::memory_protection_key_ =
    base::MemoryProtectionKey::kNoMemoryProtectionKey;

#if DEBUG
thread_local bool
    RwxMemoryWriteScope::is_key_permissions_initialized_for_current_thread_ =
        false;
bool RwxMemoryWriteScope::pkey_initialized = false;

bool RwxMemoryWriteScope::is_key_permissions_initialized_for_current_thread() {
  return is_key_permissions_initialized_for_current_thread_;
}
#endif  // DEBUG

void RwxMemoryWriteScope::InitializeMemoryProtectionKey() {
  // Flip {pkey_initialized} (in debug mode) and check the new value.
  DCHECK_EQ(true, pkey_initialized = !pkey_initialized);
  memory_protection_key_ = base::MemoryProtectionKey::AllocateKey();
  DCHECK(memory_protection_key_ > 0 ||
         memory_protection_key_ ==
             base::MemoryProtectionKey::kNoMemoryProtectionKey);
}

bool RwxMemoryWriteScope::IsPKUWritable() {
  DCHECK(pkey_initialized);
  return base::MemoryProtectionKey::GetKeyPermission(memory_protection_key_) ==
         base::MemoryProtectionKey::kNoRestrictions;
}

ResetPKUPermissionsForThreadSpawning::ResetPKUPermissionsForThreadSpawning() {
  if (!RwxMemoryWriteScope::IsSupported()) return;
  was_writable_ = RwxMemoryWriteScope::IsPKUWritable();
  if (was_writable_) {
    base::MemoryProtectionKey::SetPermissionsForKey(
        RwxMemoryWriteScope::memory_protection_key(),
        base::MemoryProtectionKey::kDisableWrite);
  }
}

ResetPKUPermissionsForThreadSpawning::~ResetPKUPermissionsForThreadSpawning() {
  if (!RwxMemoryWriteScope::IsSupported()) return;
  if (was_writable_) {
    base::MemoryProtectionKey::SetPermissionsForKey(
        RwxMemoryWriteScope::memory_protection_key(),
        base::MemoryProtectionKey::kNoRestrictions);
  }
}

void RwxMemoryWriteScope::SetDefaultPermissionsForNewThread() {
  // TODO(v8:13023): consider initializing the permissions only once per thread
  // if the SetPermissionsForKey() call is too heavy.
  if (RwxMemoryWriteScope::IsSupported() &&
      base::MemoryProtectionKey::GetKeyPermission(
          RwxMemoryWriteScope::memory_protection_key()) ==
          base::MemoryProtectionKey::kDisableAccess) {
    base::MemoryProtectionKey::SetPermissionsForKey(
        RwxMemoryWriteScope::memory_protection_key(),
        base::MemoryProtectionKey::kDisableWrite);
  }
#if DEBUG
  is_key_permissions_initialized_for_current_thread_ = true;
#endif
}

void RwxMemoryWriteScope::SetDefaultPermissionsForSignalHandler() {
  DCHECK(pkey_initialized);
  DCHECK(is_key_permissions_initialized_for_current_thread_);
  if (!RwxMemoryWriteScope::IsSupported()) return;
  base::MemoryProtectionKey::SetPermissionsForKey(
      memory_protection_key_, base::MemoryProtectionKey::kDisableWrite);
}
#else  // !V8_HAS_PKU_JIT_WRITE_PROTECT

void RwxMemoryWriteScope::SetDefaultPermissionsForNewThread() {}

#if DEBUG
bool RwxMemoryWriteScope::is_key_permissions_initialized_for_current_thread() {
  return true;
}
#endif  // DEBUG
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT

RwxMemoryWriteScopeForTesting::RwxMemoryWriteScopeForTesting()
    : RwxMemoryWriteScope("For Testing") {}

RwxMemoryWriteScopeForTesting::~RwxMemoryWriteScopeForTesting() {}

}  // namespace internal
}  // namespace v8
