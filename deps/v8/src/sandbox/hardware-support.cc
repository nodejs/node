// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/hardware-support.h"

#include "src/base/platform/memory-protection-key.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

int SandboxHardwareSupport::pkey_ =
    base::MemoryProtectionKey::kNoMemoryProtectionKey;

// static
bool SandboxHardwareSupport::InitializeBeforeThreadCreation() {
  DCHECK_EQ(pkey_, base::MemoryProtectionKey::kNoMemoryProtectionKey);
  pkey_ = base::MemoryProtectionKey::AllocateKey();
  return pkey_ != base::MemoryProtectionKey::kNoMemoryProtectionKey;
}

// static
bool SandboxHardwareSupport::TryEnable(Address addr, size_t size) {
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return false;
  }

  // If we have a valid PKEY, we expect this to always succeed.
  CHECK(base::MemoryProtectionKey::SetPermissionsAndKey(
      {addr, size}, v8::PageAllocator::Permission::kNoAccess, pkey_));
  return true;
}

// static
bool SandboxHardwareSupport::IsEnabled() {
  return pkey_ != base::MemoryProtectionKey::kNoMemoryProtectionKey;
}

// static
void SandboxHardwareSupport::NotifyReadOnlyPageCreated(
    Address addr, size_t size, PageAllocator::Permission perm) {
  if (!IsEnabled()) return;

  // Reset the pkey of the read-only page to the default pkey, since some
  // SBXCHECKs will safely read read-only data from the heap.
  CHECK(base::MemoryProtectionKey::SetPermissionsAndKey(
      {addr, size}, perm, base::MemoryProtectionKey::kDefaultProtectionKey));
}

#ifdef DEBUG
// DisallowSandboxAccess scopes can be arbitrarily nested and even attached to
// heap-allocated objects (so their lifetime isn't necessarily tied to a stack
// frame). For that to work correctly, we need to track the activation count in
// a per-thread global variable.
thread_local unsigned disallow_sandbox_access_activation_counter_ = 0;
// AllowSandboxAccess scopes on the other hand cannot be nested. There must be
// at most a single one active at any point in time. These are supposed to only
// be used for short sequences of code that's otherwise running with an active
// DisallowSandboxAccess scope.
thread_local bool has_active_allow_sandbox_access_scope_ = false;

DisallowSandboxAccess::DisallowSandboxAccess() {
  pkey_ = SandboxHardwareSupport::pkey_;
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return;
  }

  // Using a DisallowSandboxAccess inside an AllowSandboxAccess isn't currently
  // allowed, but we could add support for that in the future if needed.
  DCHECK_WITH_MSG(!has_active_allow_sandbox_access_scope_,
                  "DisallowSandboxAccess cannot currently be nested inside an "
                  "AllowSandboxAccess");

  if (disallow_sandbox_access_activation_counter_ == 0) {
    DCHECK_EQ(base::MemoryProtectionKey::GetKeyPermission(pkey_),
              base::MemoryProtectionKey::Permission::kNoRestrictions);
    base::MemoryProtectionKey::SetPermissionsForKey(
        pkey_, base::MemoryProtectionKey::Permission::kDisableAccess);
  }
  disallow_sandbox_access_activation_counter_ += 1;
}

DisallowSandboxAccess::~DisallowSandboxAccess() {
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return;
  }

  DCHECK_NE(disallow_sandbox_access_activation_counter_, 0);
  disallow_sandbox_access_activation_counter_ -= 1;
  if (disallow_sandbox_access_activation_counter_ == 0) {
    DCHECK_EQ(base::MemoryProtectionKey::GetKeyPermission(pkey_),
              base::MemoryProtectionKey::Permission::kDisableAccess);
    base::MemoryProtectionKey::SetPermissionsForKey(
        pkey_, base::MemoryProtectionKey::Permission::kNoRestrictions);
  }
}

AllowSandboxAccess::AllowSandboxAccess() {
  if (disallow_sandbox_access_activation_counter_ == 0) {
    // This means that either scope enforcement is disabled due to a lack of
    // PKEY support on the system or that there is no active
    // DisallowSandboxAccess. In both cases, we don't need to do anything here
    // and this scope object is just a no-op.
    pkey_ = base::MemoryProtectionKey::kNoMemoryProtectionKey;
    return;
  }

  DCHECK_WITH_MSG(!has_active_allow_sandbox_access_scope_,
                  "AllowSandboxAccess scopes cannot be nested");
  has_active_allow_sandbox_access_scope_ = true;

  pkey_ = SandboxHardwareSupport::pkey_;
  // We must have an active DisallowSandboxAccess so PKEYs must be supported.
  DCHECK_NE(pkey_, base::MemoryProtectionKey::kNoMemoryProtectionKey);

  DCHECK_EQ(base::MemoryProtectionKey::GetKeyPermission(pkey_),
            base::MemoryProtectionKey::Permission::kDisableAccess);
  base::MemoryProtectionKey::SetPermissionsForKey(
      pkey_, base::MemoryProtectionKey::Permission::kNoRestrictions);
}

AllowSandboxAccess::~AllowSandboxAccess() {
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    // There was no DisallowSandboxAccess scope active when this
    // AllowSandboxAccess scope was created, and we don't expect one to have
    // been created in the meantime.
    DCHECK_EQ(disallow_sandbox_access_activation_counter_, 0);
    return;
  }

  // There was an active DisallowSandboxAccess scope when this
  // AllowSandboxAccess scope was created, and we expect it to still be there.
  DCHECK_GT(disallow_sandbox_access_activation_counter_, 0);

  DCHECK(has_active_allow_sandbox_access_scope_);
  has_active_allow_sandbox_access_scope_ = false;

  DCHECK_EQ(base::MemoryProtectionKey::GetKeyPermission(pkey_),
            base::MemoryProtectionKey::Permission::kNoRestrictions);
  base::MemoryProtectionKey::SetPermissionsForKey(
      pkey_, base::MemoryProtectionKey::Permission::kDisableAccess);
}
#endif  // DEBUG

#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

}  // namespace internal
}  // namespace v8
