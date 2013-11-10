// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

#ifndef V8_PLATFORM_H_
#define V8_PLATFORM_H_

#include <cstdarg>

#include "platform/mutex.h"
#include "platform/semaphore.h"
#include "utils.h"
#include "v8globals.h"

#ifdef __sun
# ifndef signbit
namespace std {
int signbit(double x);
}
# endif
#endif

// Microsoft Visual C++ specific stuff.
#if V8_CC_MSVC

#include "win32-headers.h"
#include "win32-math.h"

int strncasecmp(const char* s1, const char* s2, int n);

// Visual C++ 2013 and higher implement this function.
#if (_MSC_VER < 1800)
inline int lrint(double flt) {
  int intgr;
#if V8_TARGET_ARCH_IA32
  __asm {
    fld flt
    fistp intgr
  };
#else
  intgr = static_cast<int>(flt + 0.5);
  if ((intgr & 1) != 0 && intgr - flt == 0.5) {
    // If the number is halfway between two integers, round to the even one.
    intgr--;
  }
#endif
  return intgr;
}

#endif  // _MSC_VER < 1800

#endif  // V8_CC_MSVC

namespace v8 {
namespace internal {

double ceiling(double x);
double modulo(double x, double y);

// Custom implementation of math functions.
double fast_sin(double input);
double fast_cos(double input);
double fast_tan(double input);
double fast_log(double input);
double fast_exp(double input);
double fast_sqrt(double input);
// The custom exp implementation needs 16KB of lookup data; initialize it
// on demand.
void lazily_initialize_fast_exp();

// ----------------------------------------------------------------------------
// Fast TLS support

#ifndef V8_NO_FAST_TLS

#if defined(_MSC_VER) && V8_HOST_ARCH_IA32

#define V8_FAST_TLS_SUPPORTED 1

INLINE(intptr_t InternalGetExistingThreadLocal(intptr_t index));

inline intptr_t InternalGetExistingThreadLocal(intptr_t index) {
  const intptr_t kTibInlineTlsOffset = 0xE10;
  const intptr_t kTibExtraTlsOffset = 0xF94;
  const intptr_t kMaxInlineSlots = 64;
  const intptr_t kMaxSlots = kMaxInlineSlots + 1024;
  ASSERT(0 <= index && index < kMaxSlots);
  if (index < kMaxInlineSlots) {
    return static_cast<intptr_t>(__readfsdword(kTibInlineTlsOffset +
                                               kPointerSize * index));
  }
  intptr_t extra = static_cast<intptr_t>(__readfsdword(kTibExtraTlsOffset));
  ASSERT(extra != 0);
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


// ----------------------------------------------------------------------------
// OS
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class OS {
 public:
  // Initializes the platform OS support that depend on CPU features. This is
  // called after CPU initialization.
  static void PostSetUp();

  // Returns the accumulated user time for thread. This routine
  // can be used for profiling. The implementation should
  // strive for high-precision timer resolution, preferable
  // micro-second resolution.
  static int GetUserTime(uint32_t* secs,  uint32_t* usecs);

  // Returns current time as the number of milliseconds since
  // 00:00:00 UTC, January 1, 1970.
  static double TimeCurrentMillis();

  // Returns a string identifying the current time zone. The
  // timestamp is used for determining if DST is in effect.
  static const char* LocalTimezone(double time);

  // Returns the local time offset in milliseconds east of UTC without
  // taking daylight savings time into account.
  static double LocalTimeOffset();

  // Returns the daylight savings offset for the given time.
  static double DaylightSavingsOffset(double time);

  // Returns last OS error.
  static int GetLastError();

  static FILE* FOpen(const char* path, const char* mode);
  static bool Remove(const char* path);

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

  // Sleep for a number of milliseconds.
  static void Sleep(const int milliseconds);

  // Abort the current process.
  static void Abort();

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
    static MemoryMappedFile* open(const char* name);
    static MemoryMappedFile* create(const char* name, int size, void* initial);
    virtual ~MemoryMappedFile() { }
    virtual void* memory() = 0;
    virtual int size() = 0;
  };

  // Safe formatting print. Ensures that str is always null-terminated.
  // Returns the number of chars written, or -1 if output was truncated.
  static int SNPrintF(Vector<char> str, const char* format, ...);
  static int VSNPrintF(Vector<char> str,
                       const char* format,
                       va_list args);

  static char* StrChr(char* str, int c);
  static void StrNCpy(Vector<char> dest, const char* src, size_t n);

  // Support for the profiler.  Can do nothing, in which case ticks
  // occuring in shared libraries will not be properly accounted for.
  static void LogSharedLibraryAddresses(Isolate* isolate);

  // Support for the profiler.  Notifies the external profiling
  // process that a code moving garbage collection starts.  Can do
  // nothing, in which case the code objects must not move (e.g., by
  // using --never-compact) if accurate profiling is desired.
  static void SignalCodeMovingGC();

  // The return value indicates the CPU features we are sure of because of the
  // OS.  For example MacOSX doesn't run on any x86 CPUs that don't have SSE2
  // instructions.
  // This is a little messy because the interpretation is subject to the cross
  // of the CPU and the OS.  The bits in the answer correspond to the bit
  // positions indicated by the members of the CpuFeature enum from globals.h
  static uint64_t CpuFeaturesImpliedByPlatform();

  // The total amount of physical memory available on the current system.
  static uint64_t TotalPhysicalMemory();

  // Maximum size of the virtual memory.  0 means there is no artificial
  // limit.
  static intptr_t MaxVirtualMemory();

  // Returns the double constant NAN
  static double nan_value();

  // Support runtime detection of whether the hard float option of the
  // EABI is used.
  static bool ArmUsingHardFloat();

  // Returns the activation frame alignment constraint or zero if
  // the platform doesn't care. Guaranteed to be a power of two.
  static int ActivationFrameAlignment();

#if defined(V8_TARGET_ARCH_IA32)
  // Limit below which the extra overhead of the MemCopy function is likely
  // to outweigh the benefits of faster copying.
  static const int kMinComplexMemCopy = 64;

  // Copy memory area. No restrictions.
  static void MemMove(void* dest, const void* src, size_t size);
  typedef void (*MemMoveFunction)(void* dest, const void* src, size_t size);

  // Keep the distinction of "move" vs. "copy" for the benefit of other
  // architectures.
  static void MemCopy(void* dest, const void* src, size_t size) {
    MemMove(dest, src, size);
  }
#elif defined(V8_HOST_ARCH_ARM)
  typedef void (*MemCopyUint8Function)(uint8_t* dest,
                                       const uint8_t* src,
                                       size_t size);
  static MemCopyUint8Function memcopy_uint8_function;
  static void MemCopyUint8Wrapper(uint8_t* dest,
                                  const uint8_t* src,
                                  size_t chars) {
    memcpy(dest, src, chars);
  }
  // For values < 16, the assembler function is slower than the inlined C code.
  static const int kMinComplexMemCopy = 16;
  static void MemCopy(void* dest, const void* src, size_t size) {
    (*memcopy_uint8_function)(reinterpret_cast<uint8_t*>(dest),
                              reinterpret_cast<const uint8_t*>(src),
                              size);
  }
  static void MemMove(void* dest, const void* src, size_t size) {
    memmove(dest, src, size);
  }

  typedef void (*MemCopyUint16Uint8Function)(uint16_t* dest,
                                             const uint8_t* src,
                                             size_t size);
  static MemCopyUint16Uint8Function memcopy_uint16_uint8_function;
  static void MemCopyUint16Uint8Wrapper(uint16_t* dest,
                                        const uint8_t* src,
                                        size_t chars);
  // For values < 12, the assembler function is slower than the inlined C code.
  static const int kMinComplexConvertMemCopy = 12;
  static void MemCopyUint16Uint8(uint16_t* dest,
                                 const uint8_t* src,
                                 size_t size) {
    (*memcopy_uint16_uint8_function)(dest, src, size);
  }
#else
  // Copy memory area to disjoint memory area.
  static void MemCopy(void* dest, const void* src, size_t size) {
    memcpy(dest, src, size);
  }
  static void MemMove(void* dest, const void* src, size_t size) {
    memmove(dest, src, size);
  }
  static const int kMinComplexMemCopy = 16 * kPointerSize;
#endif  // V8_TARGET_ARCH_IA32

  static int GetCurrentProcessId();

 private:
  static const int msPerSecond = 1000;

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
    ASSERT(IsReserved());
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
    ASSERT(IsReserved());
    // Notice: Order is important here. The VirtualMemory object might live
    // inside the allocated region.
    void* address = address_;
    size_t size = size_;
    Reset();
    bool result = ReleaseRegion(address, size);
    USE(result);
    ASSERT(result);
  }

  // Assign control of the reserved region to a different VirtualMemory object.
  // The old object is no longer functional (IsReserved() returns false).
  void TakeControl(VirtualMemory* from) {
    ASSERT(!IsReserved());
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
  // LOCAL_STORAGE_KEY_MIN_VALUE and LOCAL_STORAGE_KEY_MAX_VALUE are specified
  // to ensure that enumeration type has correct value range (see Issue 830 for
  // more details).
  enum LocalStorageKey {
    LOCAL_STORAGE_KEY_MIN_VALUE = kMinInt,
    LOCAL_STORAGE_KEY_MAX_VALUE = kMaxInt
  };

  class Options {
   public:
    Options() : name_("v8:<unknown>"), stack_size_(0) {}
    Options(const char* name, int stack_size = 0)
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
    ASSERT(result == GetThreadLocal(key));
    return result;
  }
#else
  static inline void* GetExistingThreadLocal(LocalStorageKey key) {
    return GetThreadLocal(key);
  }
#endif

  // A hint to the scheduler to let another thread run.
  static void YieldCPU();


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

} }  // namespace v8::internal

#endif  // V8_PLATFORM_H_
