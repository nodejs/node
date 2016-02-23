// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module contains the platform-specific code. This make the rest of the
// code less dependent on operating system, compilers and runtime libraries.
// This module does specifically not deal with differences between different
// processor architecture.
// The platform classes have the same definition for all platforms. The
// implementation for a particular platform is put in platform_<os>.cc.
// The build system then uses the implementation for the target platform.
//
// This design has been chosen because it is simple and fast. Alternatively,
// the platform dependent classes could have been implemented using abstract
// superclasses with virtual methods and having specializations for each
// platform. This design was rejected because it was more complicated and
// slower. It would require factory methods for selecting the right
// implementation and the overhead of virtual methods for performance
// sensitive like mutex locking/unlocking.

#ifndef V8_BASE_PLATFORM_PLATFORM_H_
#define V8_BASE_PLATFORM_PLATFORM_H_

#include <cstdarg>
#include <string>
#include <vector>

#include "src/base/build_config.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"

#if V8_OS_QNX
#include "src/base/qnx-math.h"
#endif

namespace v8 {
namespace base {

// ----------------------------------------------------------------------------
// Fast TLS support

#ifndef V8_NO_FAST_TLS

#if V8_CC_MSVC && V8_HOST_ARCH_IA32

#define V8_FAST_TLS_SUPPORTED 1

INLINE(intptr_t InternalGetExistingThreadLocal(intptr_t index));

inline intptr_t InternalGetExistingThreadLocal(intptr_t index) {
  const intptr_t kTibInlineTlsOffset = 0xE10;
  const intptr_t kTibExtraTlsOffset = 0xF94;
  const intptr_t kMaxInlineSlots = 64;
  const intptr_t kMaxSlots = kMaxInlineSlots + 1024;
  const intptr_t kPointerSize = sizeof(void*);
  DCHECK(0 <= index && index < kMaxSlots);
  if (index < kMaxInlineSlots) {
    return static_cast<intptr_t>(__readfsdword(kTibInlineTlsOffset +
                                               kPointerSize * index));
  }
  intptr_t extra = static_cast<intptr_t>(__readfsdword(kTibExtraTlsOffset));
  DCHECK(extra != 0);
  return *reinterpret_cast<intptr_t*>(extra +
                                      kPointerSize * (index - kMaxInlineSlots));
}

#elif defined(__APPLE__) && (V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64)

#define V8_FAST_TLS_SUPPORTED 1

extern intptr_t kMacTlsBaseOffset;

INLINE(intptr_t InternalGetExistingThreadLocal(intptr_t index));

inline intptr_t InternalGetExistingThreadLocal(intptr_t index) {
  intptr_t result;
#if V8_HOST_ARCH_IA32
  asm("movl %%gs:(%1,%2,4), %0;"
      :"=r"(result)  // Output must be a writable register.
      :"r"(kMacTlsBaseOffset), "r"(index));
#else
  asm("movq %%gs:(%1,%2,8), %0;"
      :"=r"(result)
      :"r"(kMacTlsBaseOffset), "r"(index));
#endif
  return result;
}

#endif

#endif  // V8_NO_FAST_TLS


class TimezoneCache;


// ----------------------------------------------------------------------------
// OS
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class OS {
 public:
  // Initialize the OS class.
  // - random_seed: Used for the GetRandomMmapAddress() if non-zero.
  // - hard_abort: If true, OS::Abort() will crash instead of aborting.
  // - gc_fake_mmap: Name of the file for fake gc mmap used in ll_prof.
  static void Initialize(int64_t random_seed,
                         bool hard_abort,
                         const char* const gc_fake_mmap);

  // Returns the accumulated user time for thread. This routine
  // can be used for profiling. The implementation should
  // strive for high-precision timer resolution, preferable
  // micro-second resolution.
  static int GetUserTime(uint32_t* secs,  uint32_t* usecs);

  // Returns current time as the number of milliseconds since
  // 00:00:00 UTC, January 1, 1970.
  static double TimeCurrentMillis();

  static TimezoneCache* CreateTimezoneCache();
  static void DisposeTimezoneCache(TimezoneCache* cache);
  static void ClearTimezoneCache(TimezoneCache* cache);

  // Returns a string identifying the current time zone. The
  // timestamp is used for determining if DST is in effect.
  static const char* LocalTimezone(double time, TimezoneCache* cache);

  // Returns the local time offset in milliseconds east of UTC without
  // taking daylight savings time into account.
  static double LocalTimeOffset(TimezoneCache* cache);

  // Returns the daylight savings offset for the given time.
  static double DaylightSavingsOffset(double time, TimezoneCache* cache);

  // Returns last OS error.
  static int GetLastError();

  static FILE* FOpen(const char* path, const char* mode);
  static bool Remove(const char* path);

  static bool isDirectorySeparator(const char ch);

  // Opens a temporary file, the file is auto removed on close.
  static FILE* OpenTemporaryFile();

  // Log file open mode is platform-dependent due to line ends issues.
  static const char* const LogFileOpenMode;

  // Print output to console. This is mostly used for debugging output.
  // On platforms that has standard terminal output, the output
  // should go to stdout.
  static void Print(const char* format, ...);
  static void VPrint(const char* format, va_list args);

  // Print output to a file. This is mostly used for debugging output.
  static void FPrint(FILE* out, const char* format, ...);
  static void VFPrint(FILE* out, const char* format, va_list args);

  // Print error output to console. This is mostly used for error message
  // output. On platforms that has standard terminal output, the output
  // should go to stderr.
  static void PrintError(const char* format, ...);
  static void VPrintError(const char* format, va_list args);

  // Allocate/Free memory used by JS heap. Pages are readable/writable, but
  // they are not guaranteed to be executable unless 'executable' is true.
  // Returns the address of allocated memory, or NULL if failed.
  static void* Allocate(const size_t requested,
                        size_t* allocated,
                        bool is_executable);
  static void Free(void* address, const size_t size);

  // This is the granularity at which the ProtectCode(...) call can set page
  // permissions.
  static intptr_t CommitPageSize();

  // Mark code segments non-writable.
  static void ProtectCode(void* address, const size_t size);

  // Assign memory as a guard page so that access will cause an exception.
  static void Guard(void* address, const size_t size);

  // Generate a random address to be used for hinting mmap().
  static void* GetRandomMmapAddr();

  // Get the Alignment guaranteed by Allocate().
  static size_t AllocateAlignment();

  // Sleep for a specified time interval.
  static void Sleep(TimeDelta interval);

  // Abort the current process.
  V8_NORETURN static void Abort();

  // Debug break.
  static void DebugBreak();

  // Walk the stack.
  static const int kStackWalkError = -1;
  static const int kStackWalkMaxNameLen = 256;
  static const int kStackWalkMaxTextLen = 256;
  struct StackFrame {
    void* address;
    char text[kStackWalkMaxTextLen];
  };

  class MemoryMappedFile {
   public:
    virtual ~MemoryMappedFile() {}
    virtual void* memory() const = 0;
    virtual size_t size() const = 0;

    static MemoryMappedFile* open(const char* name);
    static MemoryMappedFile* create(const char* name, size_t size,
                                    void* initial);
  };

  // Safe formatting print. Ensures that str is always null-terminated.
  // Returns the number of chars written, or -1 if output was truncated.
  static int SNPrintF(char* str, int length, const char* format, ...);
  static int VSNPrintF(char* str,
                       int length,
                       const char* format,
                       va_list args);

  static char* StrChr(char* str, int c);
  static void StrNCpy(char* dest, int length, const char* src, size_t n);

  // Support for the profiler.  Can do nothing, in which case ticks
  // occuring in shared libraries will not be properly accounted for.
  struct SharedLibraryAddress {
    SharedLibraryAddress(
        const std::string& library_path, uintptr_t start, uintptr_t end)
        : library_path(library_path), start(start), end(end) {}

    std::string library_path;
    uintptr_t start;
    uintptr_t end;
  };

  static std::vector<SharedLibraryAddress> GetSharedLibraryAddresses();

  // Support for the profiler.  Notifies the external profiling
  // process that a code moving garbage collection starts.  Can do
  // nothing, in which case the code objects must not move (e.g., by
  // using --never-compact) if accurate profiling is desired.
  static void SignalCodeMovingGC();

  // Support runtime detection of whether the hard float option of the
  // EABI is used.
  static bool ArmUsingHardFloat();

  // Returns the activation frame alignment constraint or zero if
  // the platform doesn't care. Guaranteed to be a power of two.
  static int ActivationFrameAlignment();

  static int GetCurrentProcessId();

  static int GetCurrentThreadId();

 private:
  static const int msPerSecond = 1000;

#if V8_OS_POSIX
  static const char* GetGCFakeMMapFile();
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(OS);
};


// Represents and controls an area of reserved memory.
// Control of the reserved memory can be assigned to another VirtualMemory
// object by assignment or copy-contructing. This removes the reserved memory
// from the original object.
class VirtualMemory {
 public:
  // Empty VirtualMemory object, controlling no reserved memory.
  VirtualMemory();

  // Reserves virtual memory with size.
  explicit VirtualMemory(size_t size);

  // Reserves virtual memory containing an area of the given size that
  // is aligned per alignment. This may not be at the position returned
  // by address().
  VirtualMemory(size_t size, size_t alignment);

  // Releases the reserved memory, if any, controlled by this VirtualMemory
  // object.
  ~VirtualMemory();

  // Returns whether the memory has been reserved.
  bool IsReserved();

  // Initialize or resets an embedded VirtualMemory object.
  void Reset();

  // Returns the start address of the reserved memory.
  // If the memory was reserved with an alignment, this address is not
  // necessarily aligned. The user might need to round it up to a multiple of
  // the alignment to get the start of the aligned block.
  void* address() {
    DCHECK(IsReserved());
    return address_;
  }

  // Returns the size of the reserved memory. The returned value is only
  // meaningful when IsReserved() returns true.
  // If the memory was reserved with an alignment, this size may be larger
  // than the requested size.
  size_t size() { return size_; }

  // Commits real memory. Returns whether the operation succeeded.
  bool Commit(void* address, size_t size, bool is_executable);

  // Uncommit real memory.  Returns whether the operation succeeded.
  bool Uncommit(void* address, size_t size);

  // Creates a single guard page at the given address.
  bool Guard(void* address);

  void Release() {
    DCHECK(IsReserved());
    // Notice: Order is important here. The VirtualMemory object might live
    // inside the allocated region.
    void* address = address_;
    size_t size = size_;
    CHECK(InVM(address, size));
    Reset();
    bool result = ReleaseRegion(address, size);
    USE(result);
    DCHECK(result);
  }

  // Assign control of the reserved region to a different VirtualMemory object.
  // The old object is no longer functional (IsReserved() returns false).
  void TakeControl(VirtualMemory* from) {
    DCHECK(!IsReserved());
    address_ = from->address_;
    size_ = from->size_;
    from->Reset();
  }

  static void* ReserveRegion(size_t size);

  static bool CommitRegion(void* base, size_t size, bool is_executable);

  static bool UncommitRegion(void* base, size_t size);

  // Must be called with a base pointer that has been returned by ReserveRegion
  // and the same size it was reserved with.
  static bool ReleaseRegion(void* base, size_t size);

  // Returns true if OS performs lazy commits, i.e. the memory allocation call
  // defers actual physical memory allocation till the first memory access.
  // Otherwise returns false.
  static bool HasLazyCommits();

 private:
  bool InVM(void* address, size_t size) {
    return (reinterpret_cast<uintptr_t>(address_) <=
            reinterpret_cast<uintptr_t>(address)) &&
           ((reinterpret_cast<uintptr_t>(address_) + size_) >=
            (reinterpret_cast<uintptr_t>(address) + size));
  }

  void* address_;  // Start address of the virtual memory.
  size_t size_;  // Size of the virtual memory.
};


// ----------------------------------------------------------------------------
// Thread
//
// Thread objects are used for creating and running threads. When the start()
// method is called the new thread starts running the run() method in the new
// thread. The Thread object should not be deallocated before the thread has
// terminated.

class Thread {
 public:
  // Opaque data type for thread-local storage keys.
  typedef int32_t LocalStorageKey;

  class Options {
   public:
    Options() : name_("v8:<unknown>"), stack_size_(0) {}
    explicit Options(const char* name, int stack_size = 0)
        : name_(name), stack_size_(stack_size) {}

    const char* name() const { return name_; }
    int stack_size() const { return stack_size_; }

   private:
    const char* name_;
    int stack_size_;
  };

  // Create new thread.
  explicit Thread(const Options& options);
  virtual ~Thread();

  // Start new thread by calling the Run() method on the new thread.
  void Start();

  // Start new thread and wait until Run() method is called on the new thread.
  void StartSynchronously() {
    start_semaphore_ = new Semaphore(0);
    Start();
    start_semaphore_->Wait();
    delete start_semaphore_;
    start_semaphore_ = NULL;
  }

  // Wait until thread terminates.
  void Join();

  inline const char* name() const {
    return name_;
  }

  // Abstract method for run handler.
  virtual void Run() = 0;

  // Thread-local storage.
  static LocalStorageKey CreateThreadLocalKey();
  static void DeleteThreadLocalKey(LocalStorageKey key);
  static void* GetThreadLocal(LocalStorageKey key);
  static int GetThreadLocalInt(LocalStorageKey key) {
    return static_cast<int>(reinterpret_cast<intptr_t>(GetThreadLocal(key)));
  }
  static void SetThreadLocal(LocalStorageKey key, void* value);
  static void SetThreadLocalInt(LocalStorageKey key, int value) {
    SetThreadLocal(key, reinterpret_cast<void*>(static_cast<intptr_t>(value)));
  }
  static bool HasThreadLocal(LocalStorageKey key) {
    return GetThreadLocal(key) != NULL;
  }

#ifdef V8_FAST_TLS_SUPPORTED
  static inline void* GetExistingThreadLocal(LocalStorageKey key) {
    void* result = reinterpret_cast<void*>(
        InternalGetExistingThreadLocal(static_cast<intptr_t>(key)));
    DCHECK(result == GetThreadLocal(key));
    return result;
  }
#else
  static inline void* GetExistingThreadLocal(LocalStorageKey key) {
    return GetThreadLocal(key);
  }
#endif

  // The thread name length is limited to 16 based on Linux's implementation of
  // prctl().
  static const int kMaxThreadNameLength = 16;

  class PlatformData;
  PlatformData* data() { return data_; }

  void NotifyStartedAndRun() {
    if (start_semaphore_) start_semaphore_->Signal();
    Run();
  }

 private:
  void set_name(const char* name);

  PlatformData* data_;

  char name_[kMaxThreadNameLength];
  int stack_size_;
  Semaphore* start_semaphore_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_PLATFORM_H_
