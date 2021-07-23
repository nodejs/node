// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_MEMORY_PROTECTION_KEY_H_
#define V8_WASM_MEMORY_PROTECTION_KEY_H_

#if defined(V8_OS_LINUX) && defined(V8_HOST_ARCH_X64)
#include <sys/mman.h>  // For STATIC_ASSERT of permission values.
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h
#endif

#include "include/v8-platform.h"
#include "src/base/address-region.h"

namespace v8 {
namespace internal {
namespace wasm {

// TODO(dlehmann): Move this to base/platform/platform.h {OS} (lower-level API)
// and {base::PageAllocator} (higher-level, exported API) once the API is more
// stable and we have converged on a better design (e.g., typed class wrapper
// around int memory protection key).

// Sentinel value if there is no PKU support or allocation of a key failed.
// This is also the return value on an error of pkey_alloc() and has the
// benefit that calling pkey_mprotect() with -1 behaves the same as regular
// mprotect().
constexpr int kNoMemoryProtectionKey = -1;

// Permissions for memory protection keys on top of the page's permissions.
// NOTE: Since there is no executable bit, the executable permission cannot be
// withdrawn by memory protection keys.
enum MemoryProtectionKeyPermission {
  kNoRestrictions = 0,
  kDisableAccess = 1,
  kDisableWrite = 2,
};

// If sys/mman.h has PKEY support (on newer Linux distributions), ensure that
// our definitions of the permissions is consistent with the ones in glibc.
#if defined(PKEY_DISABLE_ACCESS)
STATIC_ASSERT(kDisableAccess == PKEY_DISABLE_ACCESS);
STATIC_ASSERT(kDisableWrite == PKEY_DISABLE_WRITE);
#endif

// Allocates a memory protection key on platforms with PKU support, returns
// {kNoMemoryProtectionKey} on platforms without support or when allocation
// failed at runtime.
int AllocateMemoryProtectionKey();

// Frees the given memory protection key, to make it available again for the
// next call to {AllocateMemoryProtectionKey()}. Note that this does NOT
// invalidate access rights to pages that are still tied to that key. That is,
// if the key is reused and pages with that key are still accessable, this might
// be a security issue. See
// https://www.gnu.org/software/libc/manual/html_mono/libc.html#Memory-Protection-Keys
void FreeMemoryProtectionKey(int key);

// Associates a memory protection {key} with the given {region}.
// If {key} is {kNoMemoryProtectionKey} this behaves like "plain"
// {SetPermissions()} and associates the default key to the region. That is,
// explicitly calling with {kNoMemoryProtectionKey} can be used to disassociate
// any protection key from a region. This also means "plain" {SetPermissions()}
// disassociates the key from a region, making the key's access restrictions
// irrelevant/inactive for that region.
// Returns true if changing permissions and key was successful. (Returns a bool
// to be consistent with {SetPermissions()}).
// The {page_permissions} are the permissions of the page, not the key. For
// changing the permissions of the key, use
// {SetPermissionsForMemoryProtectionKey()} instead.
bool SetPermissionsAndMemoryProtectionKey(
    PageAllocator* page_allocator, base::AddressRegion region,
    PageAllocator::Permission page_permissions, int key);

// Set the key's permissions. {key} must be valid, i.e. not
// {kNoMemoryProtectionKey}.
void SetPermissionsForMemoryProtectionKey(
    int key, MemoryProtectionKeyPermission permissions);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MEMORY_PROTECTION_KEY_H_
