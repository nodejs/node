// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/memory-protection-key.h"

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
#include <sys/mman.h>     // For {mprotect()} protection macros.
#include <sys/utsname.h>  // For {uname()}.
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h
#endif

#include "src/base/build_config.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"

// Runtime-detection of PKU support with {dlsym()}.
//
// For now, we support memory protection keys/PKEYs/PKU only for Linux on x64
// based on glibc functions {pkey_alloc()}, {pkey_free()}, etc.
// Those functions are only available since glibc version 2.27:
// https://man7.org/linux/man-pages/man2/pkey_alloc.2.html
// However, if we check the glibc verison with V8_GLIBC_PREPREQ here at compile
// time, this causes two problems due to dynamic linking of glibc:
// 1) If the compiling system _has_ a new enough glibc, the binary will include
// calls to {pkey_alloc()} etc., and then the runtime system must supply a
// new enough glibc version as well. That is, this potentially breaks runtime
// compatability on older systems (e.g., Ubuntu 16.04 with glibc 2.23).
// 2) If the compiling system _does not_ have a new enough glibc, PKU support
// will not be compiled in, even though the runtime system potentially _does_
// have support for it due to a new enough Linux kernel and glibc version.
// That is, this results in non-optimal security (PKU available, but not used).
// Hence, we do _not_ check the glibc version during compilation, and instead
// only at runtime try to load {pkey_alloc()} etc. with {dlsym()}.
// TODO(dlehmann): Move this import and freestanding functions below to
// base/platform/platform.h {OS} (lower-level functions) and
// {base::PageAllocator} (exported API).
#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
#include <dlfcn.h>
#endif

namespace v8 {
namespace internal {
namespace wasm {

namespace {
using pkey_alloc_t = int (*)(unsigned, unsigned);
using pkey_free_t = int (*)(int);
using pkey_mprotect_t = int (*)(void*, size_t, int, int);
using pkey_get_t = int (*)(int);
using pkey_set_t = int (*)(int, unsigned);

pkey_alloc_t pkey_alloc = nullptr;
pkey_free_t pkey_free = nullptr;
pkey_mprotect_t pkey_mprotect = nullptr;
pkey_get_t pkey_get = nullptr;
pkey_set_t pkey_set = nullptr;

#ifdef DEBUG
bool pkey_initialized = false;
#endif
}  // namespace

void InitializeMemoryProtectionKeySupport() {
  // Flip {pkey_initialized} (in debug mode) and check the new value.
  DCHECK_EQ(true, pkey_initialized = !pkey_initialized);
#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
  // PKU was broken on Linux kernels before 5.13 (see
  // https://lore.kernel.org/all/20210623121456.399107624@linutronix.de/).
  // A fix is also included in the 5.4.182 and 5.10.103 versions ("x86/fpu:
  // Correct pkru/xstate inconsistency" by Brian Geffon <bgeffon@google.com>).
  // Thus check the kernel version we are running on, and bail out if does not
  // contain the fix.
  struct utsname uname_buffer;
  CHECK_EQ(0, uname(&uname_buffer));
  int kernel, major, minor;
  // Conservatively return if the release does not match the format we expect.
  if (sscanf(uname_buffer.release, "%d.%d.%d", &kernel, &major, &minor) != 3) {
    return;
  }
  bool kernel_has_pkru_fix =
      kernel > 5 || (kernel == 5 && major >= 13) ||   // anything >= 5.13
      (kernel == 5 && major == 4 && minor >= 182) ||  // 5.4 >= 5.4.182
      (kernel == 5 && major == 10 && minor >= 103);   // 5.10 >= 5.10.103
  if (!kernel_has_pkru_fix) return;

  // Try to find the pkey functions in glibc.
  void* pkey_alloc_ptr = dlsym(RTLD_DEFAULT, "pkey_alloc");
  if (!pkey_alloc_ptr) return;

  // If {pkey_alloc} is available, the others must also be available.
  void* pkey_free_ptr = dlsym(RTLD_DEFAULT, "pkey_free");
  void* pkey_mprotect_ptr = dlsym(RTLD_DEFAULT, "pkey_mprotect");
  void* pkey_get_ptr = dlsym(RTLD_DEFAULT, "pkey_get");
  void* pkey_set_ptr = dlsym(RTLD_DEFAULT, "pkey_set");
  CHECK(pkey_free_ptr && pkey_mprotect_ptr && pkey_get_ptr && pkey_set_ptr);

  pkey_alloc = reinterpret_cast<pkey_alloc_t>(pkey_alloc_ptr);
  pkey_free = reinterpret_cast<pkey_free_t>(pkey_free_ptr);
  pkey_mprotect = reinterpret_cast<pkey_mprotect_t>(pkey_mprotect_ptr);
  pkey_get = reinterpret_cast<pkey_get_t>(pkey_get_ptr);
  pkey_set = reinterpret_cast<pkey_set_t>(pkey_set_ptr);
#endif
}

// TODO(dlehmann) Security: Are there alternatives to disabling CFI altogether
// for the functions below? Since they are essentially an arbitrary indirect
// call gadget, disabling CFI should be only a last resort. In Chromium, there
// was {base::ProtectedMemory} to protect the function pointer from being
// overwritten, but t seems it was removed to not begin used and AFAICT no such
// thing exists in V8 to begin with. See
// https://www.chromium.org/developers/testing/control-flow-integrity and
// https://crrev.com/c/1884819.
// What is the general solution for CFI + {dlsym()}?
// An alternative would be to not rely on glibc and instead implement PKEY
// directly on top of Linux syscalls + inline asm, but that is quite some low-
// level code (probably in the order of 100 lines).
DISABLE_CFI_ICALL
int AllocateMemoryProtectionKey() {
  DCHECK(pkey_initialized);
  if (!pkey_alloc) return kNoMemoryProtectionKey;

  // If there is support in glibc, try to allocate a new key.
  // This might still return -1, e.g., because the kernel does not support
  // PKU or because there is no more key available.
  // Different reasons for why {pkey_alloc()} failed could be checked with
  // errno, e.g., EINVAL vs ENOSPC vs ENOSYS. See manpages and glibc manual
  // (the latter is the authorative source):
  // https://www.gnu.org/software/libc/manual/html_mono/libc.html#Memory-Protection-Keys
  STATIC_ASSERT(kNoMemoryProtectionKey == -1);
  return pkey_alloc(/* flags, unused */ 0, kDisableAccess);
}

DISABLE_CFI_ICALL
void FreeMemoryProtectionKey(int key) {
  DCHECK(pkey_initialized);
  // Only free the key if one was allocated.
  if (key == kNoMemoryProtectionKey) return;

  // On platforms without PKU support, we should have already returned because
  // the key must be {kNoMemoryProtectionKey}.
  DCHECK_NOT_NULL(pkey_free);
  CHECK_EQ(/* success */ 0, pkey_free(key));
}

int GetProtectionFromMemoryPermission(PageAllocator::Permission permission) {
#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
  // Mappings for PKU are either RWX (on this level) or no access.
  switch (permission) {
    case PageAllocator::kNoAccess:
      return PROT_NONE;
    case PageAllocator::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      UNREACHABLE();
  }
#endif
  // Other platforms do not use PKU.
  UNREACHABLE();
}

DISABLE_CFI_ICALL
bool SetPermissionsAndMemoryProtectionKey(
    PageAllocator* page_allocator, base::AddressRegion region,
    PageAllocator::Permission page_permissions, int key) {
  DCHECK(pkey_initialized);

  void* address = reinterpret_cast<void*>(region.begin());
  size_t size = region.size();

  if (pkey_mprotect) {
    // Copied with slight modifications from base/platform/platform-posix.cc
    // {OS::SetPermissions()}.
    // TODO(dlehmann): Move this block into its own function at the right
    // abstraction boundary (likely some static method in platform.h {OS})
    // once the whole PKU code is moved into base/platform/.
    DCHECK_EQ(0, region.begin() % page_allocator->CommitPageSize());
    DCHECK_EQ(0, size % page_allocator->CommitPageSize());

    int protection = GetProtectionFromMemoryPermission(page_permissions);

    int ret = pkey_mprotect(address, size, protection, key);

    if (ret == 0 && page_permissions == PageAllocator::kNoAccess) {
      // Similar to {OS::SetPermissions}, also discard the pages after switching
      // to no access. This is advisory; ignore errors and continue execution.
      USE(page_allocator->DiscardSystemPages(address, size));
    }

    return ret == /* success */ 0;
  }

  // If there is no runtime support for {pkey_mprotect()}, no key should have
  // been allocated in the first place.
  DCHECK_EQ(kNoMemoryProtectionKey, key);

  // Without PKU support, fallback to regular {mprotect()}.
  return page_allocator->SetPermissions(address, size, page_permissions);
}

DISABLE_CFI_ICALL
void SetPermissionsForMemoryProtectionKey(
    int key, MemoryProtectionKeyPermission permissions) {
  DCHECK(pkey_initialized);
  DCHECK_NE(kNoMemoryProtectionKey, key);

  // If a valid key was allocated, {pkey_set()} must also be available.
  DCHECK_NOT_NULL(pkey_set);

  CHECK_EQ(0 /* success */, pkey_set(key, permissions));
}

DISABLE_CFI_ICALL
MemoryProtectionKeyPermission GetMemoryProtectionKeyPermission(int key) {
  DCHECK(pkey_initialized);
  DCHECK_NE(kNoMemoryProtectionKey, key);

  // If a valid key was allocated, {pkey_get()} must also be available.
  DCHECK_NOT_NULL(pkey_get);

  int permission = pkey_get(key);
  CHECK(permission == kNoRestrictions || permission == kDisableAccess ||
        permission == kDisableWrite);
  return static_cast<MemoryProtectionKeyPermission>(permission);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
