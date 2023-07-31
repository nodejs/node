// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_CODE_MEMORY_ACCESS_H_
#define V8_COMMON_CODE_MEMORY_ACCESS_H_

#include <map>

#include "include/v8-internal.h"
#include "include/v8-platform.h"
#include "src/base/build_config.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/heap/memory-chunk.h"

namespace v8 {
namespace internal {

// We protect writes to executable memory in some configurations and whenever
// we write to it, we need to explicitely allow it first.
//
// For this purposed, there are a few scope objects with different semantics:
//
// - CodePageHeaderModificationScope:
//     Used when we write to the page header of CodeSpace pages. Only needed on
//     Apple Silicon where we can't have RW- pages in the RWX space.
// - CodePageMemoryModificationScope:
//     Allows access to the allocation area of the CodeSpace pages.
// - CodePageMemoryModificationScopeForPerf:
//     A scope to mark places where we switch permissions more broadly for
//     performance reasons.
// - CodePageMemoryModificationScopeForDebugging:
//     A scope only used in non-release builds, e.g. for code zapping.
// - wasm::CodeSpaceWriteScope:
//     Allows access to Wasm code
//
// - RwxMemoryWriteScope:
//     A scope that uses per-thread permissions to allow access. Should not be
//     used directly, but rather is the implementation of one of the above.
// - RwxMemoryWriteScopeForTesting:
//     Same, but for use in testing.

class CodePageMemoryModificationScope;
class RwxMemoryWriteScopeForTesting;
namespace wasm {
class CodeSpaceWriteScope;
}

#if V8_HAS_PKU_JIT_WRITE_PROTECT

// Alignment macros.
// Adapted from partition_allocator/thread_isolation/alignment.h.

// Page size is not a compile time constant, but we need it for alignment and
// padding of our global memory.
// We use the maximum expected value here (currently x64 only) and test in
// ThreadIsolation::Initialize() that it's a multiple of the real pagesize.
#define THREAD_ISOLATION_ALIGN_SZ 0x1000
#define THREAD_ISOLATION_ALIGN alignas(THREAD_ISOLATION_ALIGN_SZ)
#define THREAD_ISOLATION_ALIGN_OFFSET_MASK (THREAD_ISOLATION_ALIGN_SZ - 1)
#define THREAD_ISOLATION_FILL_PAGE_SZ(size)                                    \
  ((THREAD_ISOLATION_ALIGN_SZ - ((size)&THREAD_ISOLATION_ALIGN_OFFSET_MASK)) % \
   THREAD_ISOLATION_ALIGN_SZ)

#else  // V8_HAS_PKU_JIT_WRITE_PROTECT

#define THREAD_ISOLATION_ALIGN_SZ 0
#define THREAD_ISOLATION_ALIGN
#define THREAD_ISOLATION_FILL_PAGE_SZ(size) 0

#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT

// The ThreadIsolation API is used to protect executable memory using per-thread
// memory permissions and perform validation for any writes into it.
//
// It keeps metadata about all JIT regions in write-protected memory and will
// use it to validate that the writes are safe from a CFI perspective.
// Its tasks are:
// * track JIT pages and allocations and check for validity
// * check for dangling pointers on the shadow stack (not implemented)
// * validate code writes like code creation, relocation, etc. (not implemented)
class V8_EXPORT ThreadIsolation {
 public:
  static bool Enabled();
  static void Initialize(ThreadIsolatedAllocator* allocator);

  // Register a new JIT region.
  static void RegisterJitPage(Address address, size_t size);
  // Unregister a JIT region that is about to be unmpapped.
  static void UnregisterJitPage(Address address, size_t size);
  // Make a page executable. Needs to be registered first. Should only be called
  // if Enabled() is true.
  V8_NODISCARD static bool MakeExecutable(Address address, size_t size);

  // Track individual allocations. The caller needs to hold an rwx write scope.
  static void RegisterInstructionStreamAllocation(Address addr, size_t size);
  static void UnregisterInstructionStreamsInPageExcept(
      MemoryChunk* chunk, const std::vector<Address>& keep);
  static void RegisterWasmAllocation(Address addr, size_t size);
  static void UnregisterWasmAllocation(Address addr, size_t size);

  // For testing.
  static void UnregisterAllocationsInPageExcept(
      Address page, size_t page_size, const std::vector<Address>& keep);

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  static int pkey() { return trusted_data_.pkey; }
  // A copy of the pkey, but taken from untrusted memory. This function should
  // only be used to grant read access to the pkey, never for write access.
  static int untrusted_pkey() { return untrusted_data_.pkey; }
#endif

#if DEBUG
  static bool initialized() { return untrusted_data_.initialized; }
  static void CheckTrackedMemoryEmpty();
#endif

  // A std::allocator implementation that wraps the ThreadIsolated allocator.
  // This is needed to create STL containers backed by ThreadIsolated memory.
  template <class T>
  struct StlAllocator {
    typedef T value_type;

    StlAllocator() = default;
    template <class U>
    explicit StlAllocator(const StlAllocator<U>&) noexcept {}

    static value_type* allocate(size_t n) {
      return reinterpret_cast<value_type*>(
          ThreadIsolation::allocator()->Allocate(n * sizeof(value_type)));
    }
    static void deallocate(value_type* ptr, size_t n) {
      ThreadIsolation::allocator()->Free(ptr);
    }
  };

  class JitAllocation {
   public:
    explicit JitAllocation(size_t size) : size_(size) {}
    size_t Size() const { return size_; }

   private:
    size_t size_;
  };

  class JitPage;

  // All accesses to the JitPage go through the JitPageReference class, which
  // will guard it against concurrent access.
  class JitPageReference {
   public:
    JitPageReference(class JitPage* page, Address address);
    JitPageReference(JitPageReference&&) V8_NOEXCEPT = default;
    JitPageReference(const JitPageReference&) = delete;
    JitPageReference& operator=(const JitPageReference&) = delete;

    base::Address Address() const { return address_; }
    size_t Size() const;
    base::Address End() const { return Address() + Size(); }
    void RegisterAllocation(base::Address addr, size_t size);
    void UnregisterAllocation(base::Address addr);
    void UnregisterAllocationsExcept(base::Address start, size_t size,
                                     const std::vector<base::Address>& addr);
    bool Empty() const;
    void Shrink(class JitPage* tail);
    void Expand(size_t offset);
    void Merge(JitPageReference& next);
    class JitPage* JitPage() { return jit_page_; }

   private:
    base::MutexGuard page_lock_;
    class JitPage* jit_page_;
    // We get the address from the key of the map when we do a JitPage lookup.
    // We can save some memory by storing it as part of the reference instead.
    base::Address address_;
  };

  class JitPage {
   public:
    explicit JitPage(size_t size) : size_(size) {}
    ~JitPage();

   private:
    base::Mutex mutex_;
    typedef std::map<Address, JitAllocation, std::less<Address>,
                     StlAllocator<std::pair<const Address, JitAllocation>>>
        AllocationMap;
    AllocationMap allocations_;
    size_t size_;

    friend class JitPageReference;
  };

 private:
  static ThreadIsolatedAllocator* allocator() {
    return trusted_data_.allocator;
  }

  // We store pointers in the map since we want to use the entries without
  // keeping the map locked.
  typedef std::map<Address, JitPage*, std::less<Address>,
                   StlAllocator<std::pair<const Address, JitPage*>>>
      JitPageMap;

  // The TrustedData needs to be page aligned so that we can protect it using
  // per-thread memory permissions (e.g. pkeys on x64).
  struct THREAD_ISOLATION_ALIGN TrustedData {
    ThreadIsolatedAllocator* allocator = nullptr;

#if V8_HAS_PKU_JIT_WRITE_PROTECT
    int pkey = -1;
#endif

    base::Mutex* jit_pages_mutex_;
    JitPageMap* jit_pages_;
  };

  struct UntrustedData {
#if DEBUG
    bool initialized = false;
#endif
#if V8_HAS_PKU_JIT_WRITE_PROTECT
    int pkey = -1;
#endif
  };

  static struct TrustedData trusted_data_;
  static struct UntrustedData untrusted_data_;

  static_assert(THREAD_ISOLATION_ALIGN_SZ == 0 ||
                sizeof(trusted_data_) == THREAD_ISOLATION_ALIGN_SZ);

  // Allocate and construct C++ objects using memory backed by the
  // ThreadIsolated allocator.
  template <typename T, typename... Args>
  static void ConstructNew(T** ptr, Args&&... args);
  template <typename T>
  static void Delete(T* ptr);

  static void RegisterJitAllocation(Address obj, size_t size);

  static JitPageReference LookupJitPage(Address addr, size_t size);
  // The caller needs to hold a lock of the jit_pages_mutex_
  static JitPageReference LookupJitPageLocked(Address addr, size_t size);

  template <class T>
  friend struct StlAllocator;
};

template <class T>
bool operator==(const ThreadIsolation::StlAllocator<T>&,
                const ThreadIsolation::StlAllocator<T>&) {
  return true;
}

template <class T>
bool operator!=(const ThreadIsolation::StlAllocator<T>&,
                const ThreadIsolation::StlAllocator<T>&) {
  return false;
}

// This scope is a wrapper for APRR/MAP_JIT machinery on MacOS on ARM64
// ("Apple M1"/Apple Silicon) or Intel PKU (aka. memory protection keys)
// with respective low-level semantics.
//
// The semantics on MacOS on ARM64 is the following:
// The scope switches permissions between writable and executable for all the
// pages allocated with RWX permissions. Only current thread is affected.
// This achieves "real" W^X and it's fast (see pthread_jit_write_protect_np()
// for details).
// By default it is assumed that the state is executable.
// It's also assumed that the process has the "com.apple.security.cs.allow-jit"
// entitlement.
//
// The semantics on Intel with PKU support is the following:
// When Intel PKU is available, the scope switches the protection key's
// permission between writable and not writable. The executable permission
// cannot be retracted with PKU. That is, this "only" achieves write
// protection, but is similarly thread-local and fast.
//
// On other platforms the scope is a no-op and thus it's allowed to be used.
//
// The scope is reentrant and thread safe.
class V8_NODISCARD RwxMemoryWriteScope {
 public:
  // The comment argument is used only for ensuring that explanation about why
  // the scope is needed is given at particular use case.
  V8_INLINE explicit RwxMemoryWriteScope(const char* comment);
  V8_INLINE ~RwxMemoryWriteScope();

  // Disable copy constructor and copy-assignment operator, since this manages
  // a resource and implicit copying of the scope can yield surprising errors.
  RwxMemoryWriteScope(const RwxMemoryWriteScope&) = delete;
  RwxMemoryWriteScope& operator=(const RwxMemoryWriteScope&) = delete;

  // Returns true if current configuration supports fast write-protection of
  // executable pages.
  V8_INLINE static bool IsSupported();
  // An untrusted version of this check, i.e. the result might be
  // attacker-controlled if we assume memory corruption. This is needed in
  // signal handlers in which we might not have read access to the trusted
  // memory.
  V8_INLINE static bool IsSupportedUntrusted();

#if V8_HAS_PKU_JIT_WRITE_PROTECT
  static int memory_protection_key() { return ThreadIsolation::pkey(); }

  static bool IsPKUWritable();

  // Linux resets key's permissions to kDisableAccess before executing signal
  // handlers. If the handler requires access to code page bodies it should take
  // care of changing permissions to the default state (kDisableWrite).
  static V8_EXPORT void SetDefaultPermissionsForSignalHandler();
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT

 private:
  friend class CodePageMemoryModificationScope;
  friend class RwxMemoryWriteScopeForTesting;
  friend class wasm::CodeSpaceWriteScope;

  // {SetWritable} and {SetExecutable} implicitly enters/exits the scope.
  // These methods are exposed only for the purpose of implementing other
  // scope classes that affect executable pages permissions.
  V8_INLINE static void SetWritable();
  V8_INLINE static void SetExecutable();

#if V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT
  // This counter is used for supporting scope reentrance.
  V8_EXPORT_PRIVATE static thread_local int code_space_write_nesting_level_;
#endif  // V8_HAS_PTHREAD_JIT_WRITE_PROTECT || V8_HAS_PKU_JIT_WRITE_PROTECT
};

// This class is a no-op version of the RwxMemoryWriteScope class above.
// It's used as a target type for other scope type definitions when a no-op
// semantics is required.
class V8_NODISCARD NopRwxMemoryWriteScope final {
 public:
  V8_INLINE explicit NopRwxMemoryWriteScope(const char* comment) {
    // Define a constructor to avoid unused variable warnings.
  }
};

#if V8_HEAP_USE_PTHREAD_JIT_WRITE_PROTECT || V8_HEAP_USE_PKU_JIT_WRITE_PROTECT
using CodePageMemoryModificationScopeForPerf = RwxMemoryWriteScope;
#else
// Without per-thread write permissions, we only use permission switching for
// debugging and the perf impact of this doesn't matter.
using CodePageMemoryModificationScopeForPerf = NopRwxMemoryWriteScope;
#endif

// Same as the RwxMemoryWriteScope but without inlining the code.
// This is a workaround for component build issue (crbug/1316800), when
// a thread_local value can't be properly exported.
class V8_NODISCARD RwxMemoryWriteScopeForTesting final
    : public RwxMemoryWriteScope {
 public:
  V8_EXPORT_PRIVATE RwxMemoryWriteScopeForTesting();
  V8_EXPORT_PRIVATE ~RwxMemoryWriteScopeForTesting();

  // Disable copy constructor and copy-assignment operator, since this manages
  // a resource and implicit copying of the scope can yield surprising errors.
  RwxMemoryWriteScopeForTesting(const RwxMemoryWriteScopeForTesting&) = delete;
  RwxMemoryWriteScopeForTesting& operator=(
      const RwxMemoryWriteScopeForTesting&) = delete;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_CODE_MEMORY_ACCESS_H_
