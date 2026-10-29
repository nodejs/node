// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/hardware-support.h"

#include "src/base/platform/memory-protection-key.h"
#include "src/base/platform/platform.h"
#include "src/flags/flags.h"

#ifdef V8_OS_LINUX
#include <sys/utsname.h>
#endif  // V8_OS_LINUX

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

int SandboxHardwareSupport::sandbox_pkey_ =
    base::MemoryProtectionKey::kNoMemoryProtectionKey;
int SandboxHardwareSupport::out_of_sandbox_pkey_ =
    base::MemoryProtectionKey::kNoMemoryProtectionKey;
int SandboxHardwareSupport::extension_pkey_ =
    base::MemoryProtectionKey::kNoMemoryProtectionKey;
uint32_t SandboxHardwareSupport::sandboxed_mode_pkey_mask_ = 0;

// static
bool SandboxHardwareSupport::TryActivateBeforeThreadCreation() {
  bool success = TryActivate();
  CHECK_IMPLIES(v8_flags.force_memory_protection_keys, success);
  return success;
}

// static
bool SandboxHardwareSupport::IsActive() {
  return sandbox_pkey_ != base::MemoryProtectionKey::kNoMemoryProtectionKey;
}

// static
bool SandboxHardwareSupport::IsStrict() {
  // In strict sandboxing mode, sandboxed code does not have access to any
  // out-of-sandbox memory (except for "sandbox extension" memory). This is
  // achieved by using the default pkey as out-of-sandbox pkey.
  return out_of_sandbox_pkey_ ==
         base::MemoryProtectionKey::kDefaultProtectionKey;
}

// static
void SandboxHardwareSupport::EnableForCurrentThread(void* limit_address) {
  if (!IsActive()) return;

  // Per-thread setup is only required if the strict sandboxing mode is used.
  if (!IsStrict()) return;

  // TODO(350324877): it would be nice if we could guard against multi-enabling
  // here. That should be easy if we ever need per-thread data for hardware
  // sandboxing, for example the address of the thread's untrusted stack.

  // Signal handlers may run without access to any non-default pkeys (for
  // example on older Linux kernels). As such, they must all be registered with
  // SA_ONSTACK and we must have an alternative stack available. Otherwise,
  // they will immediately segfault as they cannot access stack memory.
  // Note: this should happen before we assign a pkey to the stack below in
  // case a signal is ever delivered between these two operations.
  base::OS::EnsureAlternativeSignalStackIsAvailableForCurrentThread();

  // We only need to set a pkey on stack memory in strict sandboxing mode.
  //
  // Note: we need an alternative stack in any case as for example Wasm stacks
  // always use the sandbox extension PKEY and so a signal handler would not
  // have access to them.
  if (!IsStrict()) return;

  // The current hardware sandboxing prototype still requires full write access
  // to the regular stack in sandboxed mode. To support that, we need to assign
  // the extension_pkey_ to it here.
  // Note: this is unsafe. For any production use, we'd probably need to use
  // untrusted stacks. See https://crbug.com/428680013.
  CHECK(base::MemoryProtectionKey::SetKeyForCurrentThreadsStack(extension_pkey_,
                                                                limit_address));
}

void SandboxHardwareSupport::RegisterOutOfSandboxMemory(
    Address addr, size_t size, PagePermissions permissions) {
  if (!IsActive()) return;

  CHECK(base::MemoryProtectionKey::SetPermissionsAndKey(
      {addr, size}, permissions, out_of_sandbox_pkey_));
}

void SandboxHardwareSupport::RegisterUnsafeSandboxExtensionMemory(Address addr,
                                                                  size_t size) {
  if (!IsActive()) return;

  CHECK(base::MemoryProtectionKey::SetPermissionsAndKey(
      {addr, size}, PagePermissions::kReadWrite, extension_pkey_));
}

// static
void SandboxHardwareSupport::RegisterReadOnlyMemoryInsideSandbox(
    Address addr, size_t size, PagePermissions current_permissions) {
  if (!IsActive()) return;

  // Reset the pkey of the read-only page to the default pkey, since some
  // SBXCHECKs will safely read read-only data from the heap.
  CHECK(base::MemoryProtectionKey::SetPermissionsAndKey(
      {addr, size}, current_permissions,
      base::MemoryProtectionKey::kDefaultProtectionKey));
}

// static
void SandboxHardwareSupport::EnterSandboxedExecutionModeForCurrentThread() {
  if (!IsActive()) return;

  DCHECK_EQ(CurrentSandboxingMode(), CodeSandboxingMode::kUnsandboxed);
  base::MemoryProtectionKey::SetPermissionsForKey(
      out_of_sandbox_pkey_,
      base::MemoryProtectionKey::Permission::kDisableWrite);
}

// static
void SandboxHardwareSupport::ExitSandboxedExecutionModeForCurrentThread() {
  if (!IsActive()) return;

  DCHECK_EQ(CurrentSandboxingMode(), CodeSandboxingMode::kSandboxed);
  base::MemoryProtectionKey::SetPermissionsForKey(
      out_of_sandbox_pkey_,
      base::MemoryProtectionKey::Permission::kNoRestrictions);
}

// static
CodeSandboxingMode SandboxHardwareSupport::CurrentSandboxingMode() {
  if (!IsActive()) return CodeSandboxingMode::kUnsandboxed;

  auto key_permissions =
      base::MemoryProtectionKey::GetKeyPermission(out_of_sandbox_pkey_);
  if (key_permissions == base::MemoryProtectionKey::Permission::kDisableWrite) {
    return CodeSandboxingMode::kSandboxed;
  } else {
    DCHECK_EQ(key_permissions,
              base::MemoryProtectionKey::Permission::kNoRestrictions);
    return CodeSandboxingMode::kUnsandboxed;
  }
}

// static
bool SandboxHardwareSupport::CurrentSandboxingModeIs(
    CodeSandboxingMode expected_mode) {
  if (!IsActive()) return true;
  return CurrentSandboxingMode() == expected_mode;
}

// static
bool SandboxHardwareSupport::KernelSupportsSignalDeliveryInSandbox() {
#ifdef V8_OS_LINUX
  struct utsname buffer;
  CHECK_EQ(uname(&buffer), 0);

  int major, minor;
  CHECK_EQ(sscanf(buffer.release, "%d.%d", &major, &minor), 2);

  // Kernel 6.12 seems to be the first release to have the necessary fixes
  // that support signal delivery when the stack uses a non-default pkey:
  // https://github.com/torvalds/linux/commit/24cf2bc982ffe02aeffb4a3885c71751a2c7023b
  return major > 6 || (major == 6 && minor >= 12);
#else
  // Hardware-assisted sandboxing is currently only available on Linux.
  UNREACHABLE();
#endif  // V8_OS_LINUX
}

// static
bool SandboxHardwareSupport::TryActivate() {
  DCHECK(!IsActive());

  if (!base::MemoryProtectionKey::HasMemoryProtectionKeyAPIs()) {
    return false;
  }

  sandbox_pkey_ = base::MemoryProtectionKey::AllocateKey();
  if (sandbox_pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return false;
  }

  extension_pkey_ = base::MemoryProtectionKey::AllocateKey();
  if (extension_pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    base::MemoryProtectionKey::FreeKey(sandbox_pkey_);
    sandbox_pkey_ = base::MemoryProtectionKey::kNoMemoryProtectionKey;
    return false;
  }

  if (v8_flags.strict_pkey_sandbox) {
    // In strict mode, we use the default pkey as out-of-sandbox pkey so that
    // sandboxed code does not have write access to any out-of-sandbox memory
    // except for "sandbox extension" memory (opt-out).
    out_of_sandbox_pkey_ = base::MemoryProtectionKey::kDefaultProtectionKey;

#ifdef V8_OS_LINUX
    // Strict mode currently requires some special environment variables.
    // See https://crbug.com/428179540 for more details. Check for them here.
    char* bind_now = getenv("LD_BIND_NOW");
    char* glibc_tunables = getenv("GLIBC_TUNABLES");
    if (!bind_now || !glibc_tunables || strcmp(bind_now, "1") != 0 ||
        strstr(glibc_tunables, "pthread.rseq=0") == nullptr) {
      FATAL(
          "Missing necessary environment variables: `LD_BIND_NOW=1` and "
          "`GLIBC_TUNABLES=glibc.pthread.rseq=0`. See crbug.com/428179540");
    }
#endif  // V8_OS_LINUX
  } else {
    // In non-strict mode, we use a dedicated pkey as out-of-sandbox pkey.
    // Memory that should be inaccessible to sandboxed code must manually be
    // tagged with this key (opt-in).
    out_of_sandbox_pkey_ = base::MemoryProtectionKey::AllocateKey();
    if (out_of_sandbox_pkey_ ==
        base::MemoryProtectionKey::kNoMemoryProtectionKey) {
      base::MemoryProtectionKey::FreeKey(sandbox_pkey_);
      sandbox_pkey_ = base::MemoryProtectionKey::kNoMemoryProtectionKey;
      base::MemoryProtectionKey::FreeKey(extension_pkey_);
      extension_pkey_ = base::MemoryProtectionKey::kNoMemoryProtectionKey;
      return false;
    }
  }

  // Compute the PKEY mask for entering sandboxed execution mode. For that, we
  // simply need to remove write access for the out-of-sandbox pkey.
  sandboxed_mode_pkey_mask_ =
      base::MemoryProtectionKey::ComputeRegisterMaskForPermissionSwitch(
          out_of_sandbox_pkey_,
          base::MemoryProtectionKey::Permission::kDisableWrite);
  // We use zero to indicate that sandbox hardware support is inactive.
  CHECK_NE(sandboxed_mode_pkey_mask_, 0);

  // Enable sandboxing support for the current thread. We assume here that this
  // function will run on the main thread and that the main thread will want to
  // execute sandboxed code.
  EnableForCurrentThread();

  CHECK(IsActive());
  CHECK_EQ(v8_flags.strict_pkey_sandbox, IsStrict());
  return true;
}

#ifdef DEBUG
DisallowSandboxAccess::DisallowSandboxAccess(const char* reason) {
  pkey_ = SandboxHardwareSupport::sandbox_pkey_;
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return;
  }

  previous_permission_ = base::MemoryProtectionKey::GetKeyPermission(pkey_);
  if (previous_permission_ != base::MemoryProtectionKey::kDisableAccess) {
    base::MemoryProtectionKey::SetPermissionsForKey(
        pkey_, base::MemoryProtectionKey::Permission::kDisableAccess);
  }
}

DisallowSandboxAccess::~DisallowSandboxAccess() {
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return;
  }

  // When this scope is destroyed, the pkey must be in the state that we set it
  // to in the constructor. If this check fails, then the scopes were not used
  // in a strict LIFO order (i.e. an inner scope outlived an outer scope).
  base::MemoryProtectionKey::Permission current_permission =
      base::MemoryProtectionKey::GetKeyPermission(pkey_);
  DCHECK_EQ(current_permission, base::MemoryProtectionKey::kDisableAccess);

  if (current_permission != previous_permission_) {
    base::MemoryProtectionKey::SetPermissionsForKey(pkey_,
                                                    previous_permission_);
  }
}

AllowSandboxAccess::AllowSandboxAccess(const char* justification) {
  pkey_ = SandboxHardwareSupport::sandbox_pkey_;
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return;
  }

  previous_permission_ = base::MemoryProtectionKey::GetKeyPermission(pkey_);
  if (previous_permission_ != base::MemoryProtectionKey::kNoRestrictions) {
    base::MemoryProtectionKey::SetPermissionsForKey(
        pkey_, base::MemoryProtectionKey::Permission::kNoRestrictions);
  }
}

AllowSandboxAccess::~AllowSandboxAccess() {
  if (pkey_ == base::MemoryProtectionKey::kNoMemoryProtectionKey) {
    return;
  }

  // When this scope is destroyed, the pkey must be in the state that we set it
  // to in the constructor. If this check fails, then the scopes were not used
  // in a strict LIFO order (i.e. an inner scope outlived an outer scope).
  base::MemoryProtectionKey::Permission current_permission =
      base::MemoryProtectionKey::GetKeyPermission(pkey_);
  DCHECK_EQ(current_permission, base::MemoryProtectionKey::kNoRestrictions);

  if (current_permission != previous_permission_) {
    base::MemoryProtectionKey::SetPermissionsForKey(pkey_,
                                                    previous_permission_);
  }
}
#endif  // DEBUG

#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

}  // namespace internal
}  // namespace v8
