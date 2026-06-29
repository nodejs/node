// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/trusted-range.h"
#include "src/sandbox/hardware-support.h"
#include "src/sandbox/sandbox.h"
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

namespace v8 {
namespace internal {

class SandboxHardwareSupportTest : public TestWithPlatform {};

TEST_F(SandboxHardwareSupportTest, Initialization) {
  if (!base::MemoryProtectionKey::HasMemoryProtectionKeyAPIs() ||
      !base::MemoryProtectionKey::TestKeyAllocation())
    return;

  // If PKEYs are supported at runtime (and V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // is enabled at compile-time) we expect hardware sandbox support to work.
  ASSERT_TRUE(SandboxHardwareSupport::IsActive());
}

// The ASSERT_DEATH_IF_SUPPORTED macro is somewhat complicated and for example
// performs heap allocations. As such, we cannot run that macro while in
// sandboxed mode. Instead, we have to enter (and exit) sandboxed mode as part
// of the operation performed within ASSERT_DEATH_IF_SUPPORTED.
#define RUN_SANDBOXED(stmt) \
  {                         \
    EnterSandbox();         \
    stmt;                   \
    ExitSandbox();          \
  }

TEST_F(SandboxHardwareSupportTest, SimpleSandboxedCPPCode) {
  // Skip this test if hardware sandboxing support cannot be enabled (likely
  // because the system doesn't support PKEYs, see the Initialization test).
  CHECK(Sandbox::GetDefault()->is_initialized());
  CHECK_IMPLIES(v8_flags.force_memory_protection_keys,
                SandboxHardwareSupport::IsActive());
  if (!SandboxHardwareSupport::IsActive()) return;

  int* in_sandbox_memory = SandboxAlloc<int>();
  int* out_of_sandbox_memory = new int;

  // Use a volatile pointer to ensure the memory accesses are performed.
  volatile int* in_sandbox_ptr = in_sandbox_memory;
  volatile int* out_of_sandbox_ptr = out_of_sandbox_memory;

  // Both memory locations can be written to normally.
  *in_sandbox_ptr = 1;
  *out_of_sandbox_ptr = 2;

  // Out-of-sandbox memory cannot be written to in sandboxed mode.
  ASSERT_DEATH_IF_SUPPORTED(RUN_SANDBOXED(*out_of_sandbox_ptr = 3), "");
  // In-sandbox memory on the other hand can be written to.
  RUN_SANDBOXED(*in_sandbox_ptr = 4);

  SandboxFree(in_sandbox_memory);
  delete out_of_sandbox_memory;
}

TEST_F(SandboxHardwareSupportTest, SandboxedCodeNoWriteAccessToTrustedSpace) {
  // Skip this test if hardware sandboxing support cannot be enabled (likely
  // because the system doesn't support PKEYs, see the Initialization test).
  CHECK(Sandbox::GetDefault()->is_initialized());
  CHECK_IMPLIES(v8_flags.force_memory_protection_keys,
                SandboxHardwareSupport::IsActive());
  if (!SandboxHardwareSupport::IsActive()) return;
  auto trusted_space_allocator =
      IsolateGroup::current()->GetTrustedPtrComprCage()->page_allocator();
  size_t size = trusted_space_allocator->AllocatePageSize();
  void* page_in_trusted_space = trusted_space_allocator->AllocatePages(
      nullptr, size, size, PageAllocator::kReadWrite);
  CHECK_NE(page_in_trusted_space, nullptr);

  // Use a volatile pointer to ensure the memory accesses are performed.
  volatile int* trusted_space_ptr =
      reinterpret_cast<int*>(page_in_trusted_space);

  // Trusted space memory can be written to from (normal) C++ code...
  *trusted_space_ptr = 42;
  // ... but not from sandboxed code.
  ASSERT_DEATH_IF_SUPPORTED(RUN_SANDBOXED(*trusted_space_ptr = 43), "");

  trusted_space_allocator->FreePages(page_in_trusted_space, size);
}

TEST_F(SandboxHardwareSupportTest, DisallowSandboxAccess) {
  // DisallowSandboxAccess is only enforced in DEBUG builds.
  if (!DEBUG_BOOL) return;

  // Skip this test if hardware sandboxing support cannot be enabled (likely
  // because the system doesn't support PKEYs, see the Initialization test).
  CHECK(Sandbox::GetDefault()->is_initialized());
  CHECK_IMPLIES(v8_flags.force_memory_protection_keys,
                SandboxHardwareSupport::IsActive());
  if (!SandboxHardwareSupport::IsActive()) return;

  int* in_sandbox_memory = SandboxAlloc<int>();
  // Use a volatile pointer to ensure the memory accesses are performed.
  volatile int* in_sandbox_ptr = in_sandbox_memory;

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

  SandboxFree(in_sandbox_memory);
}

TEST_F(SandboxHardwareSupportTest, AllowSandboxAccess) {
  // DisallowSandboxAccess/AllowSandboxAccess is only enforced in DEBUG builds.
  if (!DEBUG_BOOL) return;

  // Skip this test if hardware sandboxing support cannot be enabled (likely
  // because the system doesn't support PKEYs, see the Initialization test).
  CHECK(Sandbox::GetDefault()->is_initialized());
  CHECK_IMPLIES(v8_flags.force_memory_protection_keys,
                SandboxHardwareSupport::IsActive());
  if (!SandboxHardwareSupport::IsActive()) return;

  int* in_sandbox_memory = SandboxAlloc<int>();
  // Use a volatile pointer to ensure the memory accesses are performed.
  volatile int* in_sandbox_ptr = in_sandbox_memory;

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

  SandboxFree(in_sandbox_memory);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
