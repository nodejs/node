// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/memory-protection-key.h"

#if V8_HAS_PKU_SUPPORT

#include <pthread.h>  // For SetKeyForCurrentThreadsStack.
#include <sys/mman.h>  // For {mprotect()} protection macros.
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h

#include "src/base/logging.h"
#include "src/base/macros.h"

// Declare all the pkey functions as weak to support older glibc versions where
// they don't exist yet.
int pkey_mprotect(void* addr, size_t len, int prot, int pkey) V8_WEAK;
int pkey_get(int key) V8_WEAK;
int pkey_set(int, unsigned) V8_WEAK;
int pkey_alloc(unsigned int, unsigned int) V8_WEAK;
int pkey_free(int) V8_WEAK;

namespace v8 {
namespace base {

namespace {

int GetProtectionFromMemoryPermission(PagePermissions permission) {
  // Mappings for PKU are either RWX (for code), no access (for uncommitted
  // memory), or RO for globals.
  switch (permission) {
    case PagePermissions::kNoAccess:
      return PROT_NONE;
    case PagePermissions::kRead:
      return PROT_READ;
    case PagePermissions::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case PagePermissions::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      UNREACHABLE();
  }
}

}  // namespace

// 16 keys on x64, 8 keys on arm64.
constexpr int kMaxAvailableKeys = 16;
std::array<bool, kMaxAvailableKeys> g_active_keys = {false};

bool MemoryProtectionKey::HasMemoryProtectionKeyAPIs() {
  if (!pkey_mprotect) return false;
  // If {pkey_mprotect} is available, the others must also be available.
  CHECK(pkey_get && pkey_set && pkey_alloc && pkey_free);

  return true;
}

// static
bool MemoryProtectionKey::TestKeyAllocation() {
  if (!HasMemoryProtectionKeyAPIs()) {
    return false;
  }
  int key = AllocateKey();
  if (key == kNoMemoryProtectionKey) {
    return false;
  }
  FreeKey(key);
  return true;
}

// static
int MemoryProtectionKey::AllocateKey() {
  if (!pkey_alloc) {
    return kNoMemoryProtectionKey;
  }

  int key = pkey_alloc(0, kNoRestrictions);
  if (key != kNoMemoryProtectionKey) {
    CHECK_LT(key, kMaxAvailableKeys);
    DCHECK(!g_active_keys[key]);
    g_active_keys[key] = true;
  }

  return key;
}

// static
void MemoryProtectionKey::FreeKey(int key) {
  DCHECK_NE(key, kNoMemoryProtectionKey);
  DCHECK(g_active_keys[key]);
  CHECK_EQ(pkey_free(key), 0);
  g_active_keys[key] = false;
}

// static
void MemoryProtectionKey::RegisterExternallyAllocatedKey(int key) {
  CHECK_LT(key, kMaxAvailableKeys);
  DCHECK(!g_active_keys[key]);
  g_active_keys[key] = true;
}

// static
bool MemoryProtectionKey::SetPermissionsAndKey(base::AddressRegion region,
                                               PagePermissions permissions,
                                               int key) {
  DCHECK_NE(key, kNoMemoryProtectionKey);
  CHECK_NOT_NULL(pkey_mprotect);

  void* address = reinterpret_cast<void*>(region.begin());
  size_t size = region.size();

  int protection = GetProtectionFromMemoryPermission(permissions);

  return pkey_mprotect(address, size, protection, key) == 0;
}

// static
void MemoryProtectionKey::SetPermissionsForKey(int key,
                                               Permission permissions) {
  DCHECK_NE(kNoMemoryProtectionKey, key);

  // If a valid key was allocated, {pkey_set()} must also be available.
  DCHECK_NOT_NULL(pkey_set);

  CHECK_EQ(0 /* success */, pkey_set(key, permissions));
}

// static
MemoryProtectionKey::Permission MemoryProtectionKey::GetKeyPermission(int key) {
  DCHECK_NE(kNoMemoryProtectionKey, key);

  // If a valid key was allocated, {pkey_get()} must also be available.
  DCHECK_NOT_NULL(pkey_get);

  int permission = pkey_get(key);
  CHECK(permission == kNoRestrictions || permission == kDisableAccess ||
        permission == kDisableWrite);
  return static_cast<Permission>(permission);
}

// static
uint32_t MemoryProtectionKey::ComputeRegisterMaskForPermissionSwitch(
    int key, Permission permissions) {
  DCHECK_NE(permissions, kNoRestrictions);

#if defined(V8_TARGET_ARCH_X64)
  constexpr int kBitsPerKey = 2;
  return permissions << (key * kBitsPerKey);
#else
  // Currently we only support x86 memory protection keys here.
  FATAL("Unsupported architecture");
#endif
}

// static
void MemoryProtectionKey::SetDefaultPermissionsForAllKeysInSignalHandler(
    bool needs_full_access) {
  // NOTE: This code MUST be async-signal safe

  // As a future optimization, we could compute the register state first (or
  // even let g_active_keys already resemble the final register state), and
  // then perform a single WRPKRU instruction.
  Permission permission = needs_full_access ? kNoRestrictions : kDisableWrite;
  for (int key = 0; key < kMaxAvailableKeys; key++) {
    if (g_active_keys[key]) {
      SetPermissionsForKey(key, permission);
    }
  }
}

bool MemoryProtectionKey::SetKeyForCurrentThreadsStack(int key) {
  DCHECK_NE(kNoMemoryProtectionKey, key);

  pthread_attr_t attr;
  void* stackaddr;
  size_t stacksize;

  // Obtain this thread's stack bounds through the pthreads API.
  // TODO(saelo): consider generalizing this and moving it into the platform
  // API once we support other platforms here.
  CHECK_EQ(pthread_getattr_np(pthread_self(), &attr), 0);
  CHECK_EQ(pthread_attr_getstack(&attr, &stackaddr, &stacksize), 0);
  CHECK_EQ(pthread_attr_destroy(&attr), 0);

  int flags = PROT_READ | PROT_WRITE;
  bool success = pkey_mprotect(stackaddr, stacksize, flags, key) == 0;
  if (!success) {
    // Retry with PROT_GROWSDOWN. This is typically required for the main
    // thread's stack.
    flags |= PROT_GROWSDOWN;
    success = pkey_mprotect(stackaddr, stacksize, flags, key) == 0;
  }
  return success;
}

}  // namespace base
}  // namespace v8

#endif  // V8_HAS_PKU_SUPPORT
