// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/memory-protection-key.h"

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
#include <sys/mman.h>  // For {mprotect()} protection macros.
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
// See comment on the import on feature testing for PKEY support.
#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
  // Try to to find {pkey_alloc()} support in glibc.
  typedef int (*pkey_alloc_t)(unsigned int, unsigned int);
  // Cache the {dlsym()} lookup in a {static} variable.
  static auto* pkey_alloc =
      bit_cast<pkey_alloc_t>(dlsym(RTLD_DEFAULT, "pkey_alloc"));
  if (pkey_alloc != nullptr) {
    // If there is support in glibc, try to allocate a new key.
    // This might still return -1, e.g., because the kernel does not support
    // PKU or because there is no more key available.
    // Different reasons for why {pkey_alloc()} failed could be checked with
    // errno, e.g., EINVAL vs ENOSPC vs ENOSYS. See manpages and glibc manual
    // (the latter is the authorative source):
    // https://www.gnu.org/software/libc/manual/html_mono/libc.html#Memory-Protection-Keys
    return pkey_alloc(/* flags, unused */ 0, kDisableAccess);
  }
#endif
  return kNoMemoryProtectionKey;
}

DISABLE_CFI_ICALL
void FreeMemoryProtectionKey(int key) {
  // Only free the key if one was allocated.
  if (key == kNoMemoryProtectionKey) return;

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
  typedef int (*pkey_free_t)(int);
  static auto* pkey_free =
      bit_cast<pkey_free_t>(dlsym(RTLD_DEFAULT, "pkey_free"));
  // If a valid key was allocated, {pkey_free()} must also be available.
  DCHECK_NOT_NULL(pkey_free);

  int ret = pkey_free(key);
  CHECK_EQ(/* success */ 0, ret);
#else
  // On platforms without PKU support, we should have already returned because
  // the key must be {kNoMemoryProtectionKey}.
  UNREACHABLE();
#endif
}

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
// TODO(dlehmann): Copied from base/platform/platform-posix.cc. Should be
// removed once this code is integrated in base/platform/platform-linux.cc.
int GetProtectionFromMemoryPermission(base::OS::MemoryPermission access) {
  switch (access) {
    case base::OS::MemoryPermission::kNoAccess:
    case base::OS::MemoryPermission::kNoAccessWillJitLater:
      return PROT_NONE;
    case base::OS::MemoryPermission::kRead:
      return PROT_READ;
    case base::OS::MemoryPermission::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case base::OS::MemoryPermission::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    case base::OS::MemoryPermission::kReadExecute:
      return PROT_READ | PROT_EXEC;
  }
  UNREACHABLE();
}
#endif

DISABLE_CFI_ICALL
bool SetPermissionsAndMemoryProtectionKey(
    PageAllocator* page_allocator, base::AddressRegion region,
    PageAllocator::Permission page_permissions, int key) {
  DCHECK_NOT_NULL(page_allocator);

  void* address = reinterpret_cast<void*>(region.begin());
  size_t size = region.size();

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
  typedef int (*pkey_mprotect_t)(void*, size_t, int, int);
  static auto* pkey_mprotect =
      bit_cast<pkey_mprotect_t>(dlsym(RTLD_DEFAULT, "pkey_mprotect"));

  if (pkey_mprotect == nullptr) {
    // If there is no runtime support for {pkey_mprotect()}, no key should have
    // been allocated in the first place.
    DCHECK_EQ(kNoMemoryProtectionKey, key);

    // Without PKU support, fallback to regular {mprotect()}.
    return page_allocator->SetPermissions(address, size, page_permissions);
  }

  // Copied with slight modifications from base/platform/platform-posix.cc
  // {OS::SetPermissions()}.
  // TODO(dlehmann): Move this block into its own function at the right
  // abstraction boundary (likely some static method in platform.h {OS})
  // once the whole PKU code is moved into base/platform/.
  DCHECK_EQ(0, region.begin() % page_allocator->CommitPageSize());
  DCHECK_EQ(0, size % page_allocator->CommitPageSize());

  int protection = GetProtectionFromMemoryPermission(
      static_cast<base::OS::MemoryPermission>(page_permissions));

  int ret = pkey_mprotect(address, size, protection, key);

  return ret == /* success */ 0;
#else
  // Without PKU support, fallback to regular {mprotect()}.
  return page_allocator->SetPermissions(address, size, page_permissions);
#endif
}

DISABLE_CFI_ICALL
void SetPermissionsForMemoryProtectionKey(
    int key, MemoryProtectionKeyPermission permissions) {
  CHECK_NE(kNoMemoryProtectionKey, key);

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
  typedef int (*pkey_set_t)(int, unsigned int);
  static auto* pkey_set = bit_cast<pkey_set_t>(dlsym(RTLD_DEFAULT, "pkey_set"));
  // If a valid key was allocated, {pkey_set()} must also be available.
  DCHECK_NOT_NULL(pkey_set);

  int ret = pkey_set(key, permissions);
  CHECK_EQ(0 /* success */, ret);
#else
  // On platforms without PKU support, we should have failed the CHECK above
  // because the key must be {kNoMemoryProtectionKey}.
  UNREACHABLE();
#endif
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
