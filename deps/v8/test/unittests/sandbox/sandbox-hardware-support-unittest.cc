// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/hardware-support.h"
#include "src/sandbox/sandbox.h"
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

namespace v8 {
namespace internal {

TEST(SandboxHardwareSupportTest, Initialization) {
  if (!base::MemoryProtectionKey::HasMemoryProtectionKeyAPIs() ||
      !base::MemoryProtectionKey::TestKeyAllocation())
    return;

  // If PKEYs are supported at runtime (and V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // is enabled at compile-time) we expect hardware sandbox support to work.
  ASSERT_TRUE(SandboxHardwareSupport::InitializeBeforeThreadCreation());
  base::VirtualAddressSpace vas;
  Sandbox sandbox;
  sandbox.Initialize(&vas);
  ASSERT_TRUE(SandboxHardwareSupport::IsEnabled());
  sandbox.TearDown();
}

TEST(SandboxHardwareSupportTest, DisallowSandboxAccess) {
  // DisallowSandboxAccess is only enforced in DEBUG builds.
  if (!DEBUG_BOOL) return;

  // Skip this test if hardware sandboxing support cannot be enabled (likely
  // because the system doesn't support PKEYs, see the Initialization test).
  if (!SandboxHardwareSupport::InitializeBeforeThreadCreation()) return;

  base::VirtualAddressSpace global_vas;

  Sandbox sandbox;
  sandbox.Initialize(&global_vas);
  ASSERT_TRUE(SandboxHardwareSupport::IsEnabled());

  VirtualAddressSpace* sandbox_vas = sandbox.address_space();
  size_t size = sandbox_vas->allocation_granularity();
  size_t alignment = sandbox_vas->allocation_granularity();
  Address ptr =
      sandbox_vas->AllocatePages(VirtualAddressSpace::kNoHint, size, alignment,
                                 PagePermissions::kReadWrite);
  EXPECT_NE(ptr, kNullAddress);
  EXPECT_TRUE(sandbox.Contains(ptr));

  volatile int* in_sandbox_ptr = reinterpret_cast<int*>(ptr);

  // Accessing in-sandbox memory should be possible.
  int value = *in_sandbox_ptr;

  // In debug builds, any (read) access to the sandbox address space should
  // crash while a DisallowSandboxAccess scope is active. This is useful to
  // ensure that a given piece of code cannot be influenced by (potentially)
  // attacker-controlled data inside the sandbox.
  {
    DisallowSandboxAccess no_sandbox_access;
    ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
    {
      // Also test that nesting of DisallowSandboxAccess scopes works correctly.
      DisallowSandboxAccess nested_no_sandbox_access;
      ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
    }
    ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
  }
  // Access should be possible again now.
  value += *in_sandbox_ptr;

  // Simple example involving a heap-allocated DisallowSandboxAccess object.
  DisallowSandboxAccess* heap_no_sandbox_access = new DisallowSandboxAccess;
  ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
  delete heap_no_sandbox_access;
  value += *in_sandbox_ptr;

  // Somewhat more elaborate example that involves a mix of stack- and
  // heap-allocated DisallowSandboxAccess objects.
  {
    DisallowSandboxAccess no_sandbox_access;
    heap_no_sandbox_access = new DisallowSandboxAccess;
    ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
  }
  // Heap-allocated DisallowSandboxAccess scope is still active.
  ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
  {
    DisallowSandboxAccess no_sandbox_access;
    delete heap_no_sandbox_access;
    ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
  }
  value += *in_sandbox_ptr;

  // Mostly just needed to force a use of |value|.
  EXPECT_EQ(value, 0);

  sandbox.TearDown();
}

TEST(SandboxHardwareSupportTest, AllowSandboxAccess) {
  // DisallowSandboxAccess/AllowSandboxAccess is only enforced in DEBUG builds.
  if (!DEBUG_BOOL) return;

  // Skip this test if hardware sandboxing support cannot be enabled (likely
  // because the system doesn't support PKEYs, see the Initialization test).
  if (!SandboxHardwareSupport::InitializeBeforeThreadCreation()) return;

  base::VirtualAddressSpace global_vas;

  Sandbox sandbox;
  sandbox.Initialize(&global_vas);
  ASSERT_TRUE(SandboxHardwareSupport::IsEnabled());

  VirtualAddressSpace* sandbox_vas = sandbox.address_space();
  size_t size = sandbox_vas->allocation_granularity();
  size_t alignment = sandbox_vas->allocation_granularity();
  Address ptr =
      sandbox_vas->AllocatePages(VirtualAddressSpace::kNoHint, size, alignment,
                                 PagePermissions::kReadWrite);
  EXPECT_NE(ptr, kNullAddress);
  EXPECT_TRUE(sandbox.Contains(ptr));

  volatile int* in_sandbox_ptr = reinterpret_cast<int*>(ptr);

  // Accessing in-sandbox memory should be possible.
  int value = *in_sandbox_ptr;

  // AllowSandboxAccess can be used to temporarily enable sandbox access in the
  // presence of a DisallowSandboxAccess scope.
  {
    DisallowSandboxAccess no_sandbox_access;
    ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
    {
      AllowSandboxAccess temporary_sandbox_access;
      value += *in_sandbox_ptr;
    }
    ASSERT_DEATH_IF_SUPPORTED(value += *in_sandbox_ptr, "");
  }

  // AllowSandboxAccess scopes cannot be nested. They should only be used for
  // short sequences of code that read/write some data from/to the sandbox.
  {
    DisallowSandboxAccess no_sandbox_access;
    {
      AllowSandboxAccess temporary_sandbox_access;
      {
        ASSERT_DEATH_IF_SUPPORTED(new AllowSandboxAccess(), "");
      }
    }
  }

  // AllowSandboxAccess scopes can be used even if there is no active
  // DisallowSandboxAccess, in which case they do nothing.
  AllowSandboxAccess no_op_sandbox_access;

  // Mostly just needed to force a use of |value|.
  EXPECT_EQ(value, 0);

  sandbox.TearDown();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
