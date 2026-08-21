// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_HARDWARE_SUPPORT_H_
#define V8_SANDBOX_HARDWARE_SUPPORT_H_

#include "include/v8-platform.h"
#include "src/common/globals.h"
#include "src/sandbox/code-sandboxing-mode.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

class V8_EXPORT_PRIVATE SandboxHardwareSupport {
 public:
  // Try to active hardware sandboxing support. This will attempt to allocate
  // the memory protection keys needed for sandbox hardware support.
  //
  // This function should be called once before any threads are created so that
  // new threads inherit access to the new pkeys.
  //
  // This function returns true on success, false otherwise. It can generally
  // fail for one of two reasons:
  //   1. the current system doesn't support memory protection keys, or
  //   2. there are no allocatable memory protection keys left.
  static bool TryActivateBeforeThreadCreation();

  // Returns true if sandbox hardware support is active.
  //
  // This will return true iff TryActivateBeforeThreadCreation() was called
  // before and succeeded in allocating the memory protection keys.
  static bool IsActive();

  // Returns true if strict sandboxing mode is enabled.
  //
  // In strict mode, we use the default pkey (key zero) as out-of-sandbox pkey,
  // thereby removing write access to all memory outside the sandbox in
  // sandboxed execution mode, with the exception of "sandbox extension" memory
  // (opt-out). If strict mode is not active, we use a dedicated out-of-sandbox
  // pkey and require memory that should be inaccessible to manually be tagged
  // with this key (opt-in).
  static bool IsStrict();

  // Enable sandbox hardware support for the current thread.
  //
  // Any thread that wishes to use EnterSandbox/ExitSandbox must first call
  // this function. Internally, this will ensure that the thread's stack memory
  // is correctly set up for hardware sandboxing.
  static void EnableForCurrentThread();

  // Register memory outside of the sandbox.
  //
  // When in sandboxed execution mode, memory outside of the sandbox cannot be
  // written to. For now, all such memory needs to be registered through this
  // method, which will assign the proper memory protection key to it. Once we
  // always use the default pkey as out_of_sandbox_pkey_, this method will no
  // longer be required.
  static void RegisterOutOfSandboxMemory(Address addr, size_t size,
                                         PagePermissions permissions);

  // Make additional out-of-sandbox memory accessible to sandboxed code.
  //
  // Any use of this function should be considered to be a temporary exception
  // and be accompanied with a comment describing how and when to get rid of it
  // (ideally linking to a bug). Eventually, for hardware sandbox support to
  // become robust, there should be no remaining uses of this function, and
  // this therefore also serves as documentation where code should be
  // refactored to not require out-of-sandbox writes from sandboxed code.
  static void RegisterUnsafeSandboxExtensionMemory(Address addr, size_t size);

  // Register read-only memory inside the sandbox.
  // Currently read-only in-sandbox memory is still accessible even with an
  // active DisallowSandboxAccess scope, and this function configures the
  // memory protection keys accordingly.
  static void RegisterReadOnlyMemoryInsideSandbox(
      Address addr, size_t size, PagePermissions current_permissions);

  // Enter and exit sandboxed execution mode for the current thread.
  //
  // When in sandboxed execution mode, out-of-sandbox memory is no longer
  // writable by the current thread (with the exception of "extension" memory
  // registered via RegisterUnsafeSandboxExtensionMemory). This is achieved by
  // removing write access for the out-of-sandbox key.
  static void EnterSandboxedExecutionModeForCurrentThread();
  static void ExitSandboxedExecutionModeForCurrentThread();

  // Returns the sandboxing mode of the current thread.
  //
  // If sandbox hardware support is not active, this will always return
  // CodeSandboxingMode::kUnsandboxed.
  static CodeSandboxingMode CurrentSandboxingMode();

  // Returns whether the current sandboxing mode matches the given mode.
  //
  // If sandbox hardware support is not active, this will always return true.
  // As such, this method can be used in for example DCHECKs without having to
  // first check if sandbox hardware support is active.
  static bool CurrentSandboxingModeIs(CodeSandboxingMode expected_mode);

  // For use in generated code.
  static Address sandboxed_mode_pkey_mask_address() {
    return reinterpret_cast<Address>(&sandboxed_mode_pkey_mask_);
  }

  // Returns whether the kernel supports signal delivery in sandboxed code.
  //
  // Older Linux kernel versions wouldn't correctly handle the case of signal
  // delivery on an alternate stack when the executing code doesn't have access
  // to the default pkey. This routine checks if the kernel version is new
  // enough. If not, signal handlers should not be used if they could trigger
  // during execution of sandboxed code. See also https://crbug.com/429173713.
  static bool KernelSupportsSignalDeliveryInSandbox();

 private:
  friend class AllowSandboxAccess;
  friend class DisallowSandboxAccess;
  friend class Sandbox;

  static bool TryActivate();

  // Returns the pkey used for in-sandbox memory.
  //
  // This should only be used by the Sandbox class during initialization.
  // TODO(416209124): this should probably return an
  // std::optional<MemoryProtectionKeyId> or similar.
  static int SandboxPkey() { return sandbox_pkey_; }

  // This PKEY is used for all (writable) memory inside the sandbox. It can be
  // used for two different purposes:
  //
  // 1. To allow write access to in-sandbox memory while removing write access
  //    to out-of-sandbox memory when executing sandboxed code.
  //
  // 2. To remove read access from in-sandbox memory in privileged code. This
  //    can be used to get a certain level of guarantees that code cannot be
  //    influenced by attacker-controlled data. See MaybeBlockAccess().
  //
  static int sandbox_pkey_;

  // This PKEY is used for out-of-sandbox memory, which must not be writable
  // when executing sandboxed code.
  //
  // Ideally, this would simply be the default pkey (key 0) to guarantee that
  // all out-of-sandbox memory is inaccessible. However, this is somewhat
  // complicated to get to work, so for now this is a "regular" pkey pwith
  // which out-of-sandbox memory must manually be tagged.
  static int out_of_sandbox_pkey_;

  // This PKEY is used for out-of-sandbox memory that must still be writable
  // even to sandboxed code.
  //
  // Ideally, at some point in the future this key will no longer be needed
  // because sandboxed code should not need to write any out-of-sandbox memory.
  static int extension_pkey_;

  // The mask to apply to the pkru register to enter sandboxed execution mode.
  //
  // This is used for the implementation of EnterSandbox and ExitSandbox in
  // generated code. When hardware sandbox support is not active, this mask
  // will simply be zero in which case the EnterSandbox and ExitSandbox
  // routines (in generated code) become nops.
  static uint32_t sandboxed_mode_pkey_mask_;
};
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

// Enter sandboxed execution mode.
//
// If hardware sandbox support is active, this will disallow write access to
// non-sandbox memory for the current thread until ExitSandbox() is called.
inline void EnterSandbox() {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  SandboxHardwareSupport::EnterSandboxedExecutionModeForCurrentThread();
#endif
}

// Exit sandboxed execution mode.
//
// This will re-enable write access to out-of-sandbox memory.
inline void ExitSandbox() {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  SandboxHardwareSupport::ExitSandboxedExecutionModeForCurrentThread();
#endif
}

// Scope object to enter sandboxed execution mode during its lifetime.
class V8_NODISCARD V8_ALLOW_UNUSED EnterSandboxScope {
 public:
  EnterSandboxScope() { EnterSandbox(); }
  ~EnterSandboxScope() { ExitSandbox(); }

  EnterSandboxScope(const EnterSandboxScope&) = delete;
  EnterSandboxScope& operator=(const EnterSandboxScope&) = delete;
};

// Scope object to exit sandboxed execution mode during its lifetime.
class V8_NODISCARD V8_ALLOW_UNUSED ExitSandboxScope {
 public:
  ExitSandboxScope() { ExitSandbox(); }
  ~ExitSandboxScope() { EnterSandbox(); }

  ExitSandboxScope(const ExitSandboxScope&) = delete;
  ExitSandboxScope& operator=(const ExitSandboxScope&) = delete;
};

// Scope object to document and enforce that code does not access in-sandbox
// data. This provides a certain level of guarantees that code cannot be
// influenced by (possibly) attacker-controlled data inside the sandbox.
// In DEBUG builds with sandbox hardware support enabled, this property is
// enforced at runtime by removing read and write access to the sandbox address
// space. The only exception are read-only pages, which will still be readable.
//
// These scopes can be arbitrarily nested and can be allocated both on the
// stack and on the heap. Sandbox access will be disallowed as long as at least
// one scope remains active.
class V8_NODISCARD V8_ALLOW_UNUSED DisallowSandboxAccess {
 public:
#if defined(DEBUG) && defined(V8_ENABLE_SANDBOX_HARDWARE_SUPPORT)
  DisallowSandboxAccess();
  ~DisallowSandboxAccess();

  // Copying and assigning these scope objects is not allowed as it would not
  // work correctly.
  DisallowSandboxAccess(const DisallowSandboxAccess&) = delete;
  DisallowSandboxAccess& operator=(const DisallowSandboxAccess&) = delete;

 private:
  int pkey_;
#endif  // DEBUG && V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
};

// Scope object to grant a temporary exception from a DisallowSandboxAccess.
// This scope object will re-enable access to in-sandbox memory during its
// lifetime even if one or more DisallowSandboxAccess scopes are currently
// active. However, in contrast to DisallowSandboxAccess these scopes cannot be
// nested and should only be used sparingly and for short durations. It is also
// currently not possible to have have another DisallowSandboxAccess inside an
// AllowSandboxAccess scope, although that could be implemented if needed.
class V8_NODISCARD V8_ALLOW_UNUSED AllowSandboxAccess {
 public:
#if defined(DEBUG) && defined(V8_ENABLE_SANDBOX_HARDWARE_SUPPORT)
  AllowSandboxAccess();
  ~AllowSandboxAccess();

  // Copying and assigning these scope objects is not allowed as it would not
  // work correctly.
  AllowSandboxAccess(const AllowSandboxAccess&) = delete;
  AllowSandboxAccess& operator=(const AllowSandboxAccess&) = delete;

 private:
  int pkey_;
#endif  // DEBUG && V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
};

// Convenience function to test whether we can use signal handlers from
// sandboxed code. See https://crbug.com/429173713 for more details.
inline bool HardwareSandboxingDisabledOrSupportsSignalDeliveryInSandbox() {
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  return !SandboxHardwareSupport::IsActive() ||
         SandboxHardwareSupport::KernelSupportsSignalDeliveryInSandbox();
#else
  return true;
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_HARDWARE_SUPPORT_H_
