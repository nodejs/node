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

#ifdef __sun
# ifndef signbit
int signbit(double x);
# endif
#endif

// GCC specific stuff
#ifdef __GNUC__

// Needed for va_list on at least MinGW and Android.
#include <stdarg.h>

#define __GNUC_VERSION__ (__GNUC__ * 10000 + __GNUC_MINOR__ * 100)

#endif  // __GNUC__


// Windows specific stuff.
#ifdef WIN32

// Microsoft Visual C++ specific stuff.
#ifdef _MSC_VER

#include "win32-math.h"

int strncasecmp(const char* s1, const char* s2, int n);

#endif  // _MSC_VER

// Random is missing on both Visual Studio and MinGW.
int random();

#endif  // WIN32

#include "atomicops.h"
#include "lazy-instance.h"
#include "platform-tls.h"
#include "utils.h"
#include "v8globals.h"

namespace v8 {
namespace internal {

// Use AtomicWord for a machine-sized pointer. It is assumed that
// reads and writes of naturally aligned values of this type are atomic.
typedef intptr_t AtomicWord;

class Semaphore;
class Mutex;

double ceiling(double x);
double modulo(double x, double y);

// Custom implementation of sin, cos, tan and log.
double fast_sin(double input);
double fast_cos(double input);
double fast_tan(double input);
double fast_log(double input);
double fast_sqrt(double input);

// Forward declarations.
class Socket;

// ----------------------------------------------------------------------------
// OS
//
// This class has static methods for the different platform specific
// functions. Add methods here to cope with differences between the
// supported platforms.

class OS {
 public:
  // Initializes the platform OS support. Called once at VM startup.
  static void SetUp();

  // Initializes the platform OS support that depend on CPU features. This is
  // called after CPU initialization.
  static void PostSetUp();

  // Clean up platform-OS-related things. Called once at VM shutdown.
  static void TearDown();

  // Returns the accumulated user time for thread. This routine
  // can be used for profiling. The implementation should
  // strive for high-precision timer resolution, preferable
  // micro-second resolution.
  static int GetUserTime(uint32_t* secs,  uint32_t* usecs);

  // Get a tick counter normalized to one tick per microsecond.
  // Used for calculating time intervals.
  static int64_t Ticks();

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

  // Returns an indication of whether a pointer is in a space that
  // has been allocated by Allocate().  This method may conservatively
  // always return false, but giving more accurate information may
  // improve the robustness of the stack dump code in the presence of
  // heap corruption.
  static bool IsOutsideAllocatedSpace(void* pointer);

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

  static int StackWalk(Vector<StackFrame> frames);

  // Factory method for creating platform dependent Mutex.
  // Please use delete to reclaim the storage for the returned Mutex.
  static Mutex* CreateMutex();

  // Factory method for creating platform dependent Semaphore.
  // Please use delete to reclaim the storage for the returned Semaphore.
  static Semaphore* CreateSemaphore(int count);

  // Factory method for creating platform dependent Socket.
  // Please use delete to reclaim the storage for the returned Socket.
  static Socket* CreateSocket();

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
  static void LogSharedLibraryAddresses();

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

  // Maximum size of the virtual memory.  0 means there is no artificial
  // limit.
  static intptr_t MaxVirtualMemory();

  // Returns the double constant NAN
  static double nan_value();

  // Support runtime detection of VFP3 on ARM CPUs.
  static bool ArmCpuHasFeature(CpuFeature feature);

  // Support runtime detection of whether the hard float option of the
  // EABI is used.
  static bool ArmUsingHardFloat();

  // Support runtime detection of FPU on MIPS CPUs.
  static bool MipsCpuHasFeature(CpuFeature feature);

  // Returns the activation frame alignment constraint or zero if
  // the platform doesn't care. Guaranteed to be a power of two.
  static int ActivationFrameAlignment();

  static void ReleaseStore(volatile AtomicWord* ptr, AtomicWord value);

#if defined(V8_TARGET_ARCH_IA32)
  // Copy memory area to disjoint memory area.
  static void MemCopy(void* dest, const void* src, size_t size);
  // Limit below which the extra overhead of the MemCopy function is likely
  // to outweigh the benefits of faster copying.
  static const int kMinComplexMemCopy = 64;
  typedef void (*MemCopyFunction)(void* dest, const void* src, size_t size);

#else  // V8_TARGET_ARCH_IA32
  static void MemCopy(void* dest, const void* src, size_t size) {
    memcpy(dest, src, size);
  }
  static const int kMinComplexMemCopy = 256;
#endif  // V8_TARGET_ARCH_IA32

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

  // Start new thread by calling the Run() method in the new thread.
  void Start();

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

 private:
  void set_name(const char* name);

  PlatformData* data_;

  char name_[kMaxThreadNameLength];
  int stack_size_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};


// ----------------------------------------------------------------------------
// Mutex
//
// Mutexes are used for serializing access to non-reentrant sections of code.
// The implementations of mutex should allow for nested/recursive locking.

class Mutex {
 public:
  virtual ~Mutex() {}

  // Locks the given mutex. If the mutex is currently unlocked, it becomes
  // locked and owned by the calling thread, and immediately. If the mutex
  // is already locked by another thread, suspends the calling thread until
  // the mutex is unlocked.
  virtual int Lock() = 0;

  // Unlocks the given mutex. The mutex is assumed to be locked and owned by
  // the calling thread on entrance.
  virtual int Unlock() = 0;

  // Tries to lock the given mutex. Returns whether the mutex was
  // successfully locked.
  virtual bool TryLock() = 0;
};

struct CreateMutexTrait {
  static Mutex* Create() {
    return OS::CreateMutex();
  }
};

// POD Mutex initialized lazily (i.e. the first time Pointer() is called).
// Usage:
//   static LazyMutex my_mutex = LAZY_MUTEX_INITIALIZER;
//
//   void my_function() {
//     ScopedLock my_lock(my_mutex.Pointer());
//     // Do something.
//   }
//
typedef LazyDynamicInstance<
    Mutex, CreateMutexTrait, ThreadSafeInitOnceTrait>::type LazyMutex;

#define LAZY_MUTEX_INITIALIZER LAZY_DYNAMIC_INSTANCE_INITIALIZER

// ----------------------------------------------------------------------------
// ScopedLock
//
// Stack-allocated ScopedLocks provide block-scoped locking and
// unlocking of a mutex.
class ScopedLock {
 public:
  explicit ScopedLock(Mutex* mutex): mutex_(mutex) {
    ASSERT(mutex_ != NULL);
    mutex_->Lock();
  }
  ~ScopedLock() {
    mutex_->Unlock();
  }

 private:
  Mutex* mutex_;
  DISALLOW_COPY_AND_ASSIGN(ScopedLock);
};


// ----------------------------------------------------------------------------
// Semaphore
//
// A semaphore object is a synchronization object that maintains a count. The
// count is decremented each time a thread completes a wait for the semaphore
// object and incremented each time a thread signals the semaphore. When the
// count reaches zero,  threads waiting for the semaphore blocks until the
// count becomes non-zero.

class Semaphore {
 public:
  virtual ~Semaphore() {}

  // Suspends the calling thread until the semaphore counter is non zero
  // and then decrements the semaphore counter.
  virtual void Wait() = 0;

  // Suspends the calling thread until the counter is non zero or the timeout
  // time has passed. If timeout happens the return value is false and the
  // counter is unchanged. Otherwise the semaphore counter is decremented and
  // true is returned. The timeout value is specified in microseconds.
  virtual bool Wait(int timeout) = 0;

  // Increments the semaphore counter.
  virtual void Signal() = 0;
};

template <int InitialValue>
struct CreateSemaphoreTrait {
  static Semaphore* Create() {
    return OS::CreateSemaphore(InitialValue);
  }
};

// POD Semaphore initialized lazily (i.e. the first time Pointer() is called).
// Usage:
//   // The following semaphore starts at 0.
//   static LazySemaphore<0>::type my_semaphore = LAZY_SEMAPHORE_INITIALIZER;
//
//   void my_function() {
//     // Do something with my_semaphore.Pointer().
//   }
//
template <int InitialValue>
struct LazySemaphore {
  typedef typename LazyDynamicInstance<
      Semaphore, CreateSemaphoreTrait<InitialValue>,
      ThreadSafeInitOnceTrait>::type type;
};

#define LAZY_SEMAPHORE_INITIALIZER LAZY_DYNAMIC_INSTANCE_INITIALIZER


// ----------------------------------------------------------------------------
// Socket
//

class Socket {
 public:
  virtual ~Socket() {}

  // Server initialization.
  virtual bool Bind(const int port) = 0;
  virtual bool Listen(int backlog) const = 0;
  virtual Socket* Accept() const = 0;

  // Client initialization.
  virtual bool Connect(const char* host, const char* port) = 0;

  // Shutdown socket for both read and write. This causes blocking Send and
  // Receive calls to exit. After Shutdown the Socket object cannot be used for
  // any communication.
  virtual bool Shutdown() = 0;

  // Data Transimission
  // Return 0 on failure.
  virtual int Send(const char* data, int len) const = 0;
  virtual int Receive(char* data, int len) const = 0;

  // Set the value of the SO_REUSEADDR socket option.
  virtual bool SetReuseAddress(bool reuse_address) = 0;

  virtual bool IsValid() const = 0;

  static bool SetUp();
  static int LastError();
  static uint16_t HToN(uint16_t value);
  static uint16_t NToH(uint16_t value);
  static uint32_t HToN(uint32_t value);
  static uint32_t NToH(uint32_t value);
};


// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

// TickSample captures the information collected for each sample.
class TickSample {
 public:
  TickSample()
      : state(OTHER),
        pc(NULL),
        sp(NULL),
        fp(NULL),
        tos(NULL),
        frames_count(0),
        has_external_callback(false) {}
  StateTag state;  // The state of the VM.
  Address pc;      // Instruction pointer.
  Address sp;      // Stack pointer.
  Address fp;      // Frame pointer.
  union {
    Address tos;   // Top stack value (*sp).
    Address external_callback;
  };
  static const int kMaxFramesCount = 64;
  Address stack[kMaxFramesCount];  // Call stack.
  int frames_count : 8;  // Number of captured frames.
  bool has_external_callback : 1;
};

class Sampler {
 public:
  // Initialize sampler.
  Sampler(Isolate* isolate, int interval);
  virtual ~Sampler();

  int interval() const { return interval_; }

  // Performs stack sampling.
  void SampleStack(TickSample* sample) {
    DoSampleStack(sample);
    IncSamplesTaken();
  }

  // This method is called for each sampling period with the current
  // program counter.
  virtual void Tick(TickSample* sample) = 0;

  // Start and stop sampler.
  void Start();
  void Stop();

  // Is the sampler used for profiling?
  bool IsProfiling() const { return NoBarrier_Load(&profiling_) > 0; }
  void IncreaseProfilingDepth() { NoBarrier_AtomicIncrement(&profiling_, 1); }
  void DecreaseProfilingDepth() { NoBarrier_AtomicIncrement(&profiling_, -1); }

  // Whether the sampler is running (that is, consumes resources).
  bool IsActive() const { return NoBarrier_Load(&active_); }

  Isolate* isolate() { return isolate_; }

  // Used in tests to make sure that stack sampling is performed.
  int samples_taken() const { return samples_taken_; }
  void ResetSamplesTaken() { samples_taken_ = 0; }

  class PlatformData;
  PlatformData* data() { return data_; }

  PlatformData* platform_data() { return data_; }

 protected:
  virtual void DoSampleStack(TickSample* sample) = 0;

 private:
  void SetActive(bool value) { NoBarrier_Store(&active_, value); }
  void IncSamplesTaken() { if (++samples_taken_ < 0) samples_taken_ = 0; }

  Isolate* isolate_;
  const int interval_;
  Atomic32 profiling_;
  Atomic32 active_;
  PlatformData* data_;  // Platform specific data.
  int samples_taken_;  // Counts stack samples taken.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Sampler);
};


} }  // namespace v8::internal

#endif  // V8_PLATFORM_H_
