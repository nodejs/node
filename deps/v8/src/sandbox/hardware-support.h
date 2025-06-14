// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_HARDWARE_SUPPORT_H_
#define V8_SANDBOX_HARDWARE_SUPPORT_H_

#include "include/v8-platform.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

class V8_EXPORT_PRIVATE SandboxHardwareSupport {
 public:
  // Allocates a pkey that will be used to optionally block sandbox access. This
  // function should be called once before any threads are created so that new
  // threads inherit access to the new pkey.
  // This function returns true on success, false otherwise. It can generally
  // fail for one of two reasons:
  //   1. the current system doesn't support memory protection keys, or
  //   2. there are no allocatable memory protection keys left.
  static bool InitializeBeforeThreadCreation();

  // Try to set up hardware permissions to the sandbox address space. If
  // successful, future calls to MaybeBlockAccess will block the current thread
  // from accessing the memory.
  // This will only fail if InitializeBeforeThreadCreation was unable to
  // allocate a memory protection key.
  static bool TryEnable(Address addr, size_t size);

  // Returns true if hardware sandboxing is enabled.
  static bool IsEnabled();

  // Removes the pkey from read only pages. We (currently) still allow read
  // access to read-only pages inside the sandbox even with an active
  // DisallowSandboxAccess scope.
  static void NotifyReadOnlyPageCreated(
      Address addr, size_t size, PageAllocator::Permission current_permissions);

 private:
  friend class DisallowSandboxAccess;
  friend class AllowSandboxAccess;
  static int pkey_;
};
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

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

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_HARDWARE_SUPPORT_H_
