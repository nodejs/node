// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_MEMORY_PROTECTION_KEY_H_
#define V8_BASE_PLATFORM_MEMORY_PROTECTION_KEY_H_

#include "src/base/build_config.h"

#if V8_HAS_PKU_SUPPORT

#include "include/v8-platform.h"
#include "src/base/address-region.h"

namespace v8 {
namespace base {

// ----------------------------------------------------------------------------
// MemoryProtectionKey
//
// This class has static methods for the different platform specific
// functions related to memory protection key support.
//
// TODO(416209124): Once this has stabilized further, consider moving it into a
// MemoryProtectionKeyProvider or similar class expsed via the Platform and
// provided by the Embedder.
class V8_BASE_EXPORT MemoryProtectionKey {
 public:
  // Sentinel value if there is no PKU support or allocation of a key failed.
  // This is also the return value on an error of pkey_alloc() and has the
  // benefit that calling pkey_mprotect() with -1 behaves the same as regular
  // mprotect().
  static constexpr int kNoMemoryProtectionKey = -1;

  // The default ProtectionKey can be used to remove pkey assignments.
  static constexpr int kDefaultProtectionKey = 0;

  // Permissions for memory protection keys on top of the page's permissions.
  // NOTE: Since there is no executable bit, the executable permission cannot be
  // withdrawn by memory protection keys.
  enum Permission {
    kNoRestrictions = 0,
    kDisableAccess = 1,
    kDisableWrite = 2,
  };

// If sys/mman.h has PKEY support (on newer Linux distributions), ensure that
// our definitions of the permissions is consistent with the ones in glibc.
#if defined(PKEY_DISABLE_ACCESS)
  static_assert(kDisableAccess == PKEY_DISABLE_ACCESS);
  static_assert(kDisableWrite == PKEY_DISABLE_WRITE);
#endif

  // Determine if the operating systems exposes the memory protection key APIs.
  //
  // This is a cheap test to see if the necessary library routines are
  // available. It does not test whether the CPU and/or the kernel support
  // PKEYs. For that, use the more expensive TestKeyAllocation() routine.
  static bool HasMemoryProtectionKeyAPIs();

  // Test whether memory protection keys can be successfully allocated on this
  // system at runtime, implying that both the CPU and the kernel support PKEYs.
  // This is a somewhat expensive test as it generally involves two syscalls
  // (e.g. pkey_alloc and pkey_free on Linux).
  // Note: as this involves allocating a PKEY, and since there's a limited
  // number of keys, it will fail if all keys have already been allocated.
  // Similarly, if this succeeds, it does not guarantee that a PKEY can be
  // allocated in the future.
  static bool TestKeyAllocation();

  // Allocates a new key. Returns kNoMemoryProtectionKey on error.
  static int AllocateKey();

  // Register a memory protection key that was allocated through other means.
  //
  // This is currently needed for SetDefaultPermissionsForAllKeysInSignalHandler
  // to work correctly. In the future, we should obtain a
  // MemoryProtectionKeyProvider from the embedder so that there is a single
  // entity in the process responsible for allocating keys. Then this will no
  // longer be needed. See also https://crbug.com/416209124.
  static void RegisterExternallyAllocatedKey(int key);

  // Frees the given key which must have been obtained through AllocateKey.
  static void FreeKey(int key);

  // Associates a memory protection {key} with the given {region}.
  // If {key} is {kNoMemoryProtectionKey} this behaves like "plain"
  // {SetPermissions()} and associates the default key to the region. That is,
  // explicitly calling with {kNoMemoryProtectionKey} can be used to
  // disassociate any protection key from a region. This also means "plain"
  // {SetPermissions()} disassociates the key from a region, making the key's
  // access restrictions irrelevant/inactive for that region. Returns true if
  // changing permissions and key was successful. (Returns a bool to be
  // consistent with {SetPermissions()}). The {page_permissions} are the
  // permissions of the page, not the key. For changing the permissions of the
  // key, use {SetPermissionsForKey()} instead.
  static bool SetPermissionsAndKey(
      base::AddressRegion region,
      v8::PageAllocator::Permission page_permissions, int key);

  // Set the key's permissions. {key} must be valid, i.e. not
  // {kNoMemoryProtectionKey}.
  static void SetPermissionsForKey(int key, Permission permissions);

  // Get the permissions of the protection key {key} for the current thread.
  static Permission GetKeyPermission(int key);

  // Set the default permissions for all active keys in a signal handler.
  //
  // Signal handlers on some platforms, for example older Linux kernels, run
  // without access to any non-default pkey. As such, they will crash when
  // trying to access any memory using another key. To work around that, this
  // method should be called at the start of a signal handler to restore the
  // default permissions for all active keys.
  //
  // At the moment, the default permission for a key is read-only access
  // (kDisableWrite). If necessary in the future, we could make that
  // configurable by passing in the default permissions into AllocateKey and
  // remembering them alongside the key.
  static void SetDefaultPermissionsForAllKeysInSignalHandler();
};

}  // namespace base
}  // namespace v8

#endif  // V8_HAS_PKU_SUPPORT

#endif  // V8_BASE_PLATFORM_MEMORY_PROTECTION_KEY_H_
