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

// Platform specific code for MacOS goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>

#include <AvailabilityMacros.h>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/vm_statistics.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#undef MAP_TYPE

#include "v8.h"

#include "platform-posix.h"
#include "platform.h"
#include "simulator.h"
#include "vm-state-inl.h"

// Manually define these here as weak imports, rather than including execinfo.h.
// This lets us launch on 10.4 which does not have these calls.
extern "C" {
  extern int backtrace(void**, int) __attribute__((weak_import));
  extern char** backtrace_symbols(void* const*, int)
      __attribute__((weak_import));
  extern void backtrace_symbols_fd(void* const*, int, int)
      __attribute__((weak_import));
}


namespace v8 {
namespace internal {

// 0 is never a valid thread id on MacOSX since a pthread_t is
// a pointer.
static const pthread_t kNoThread = (pthread_t) 0;


double ceiling(double x) {
  // Correct Mac OS X Leopard 'ceil' behavior.
  if (-1.0 < x && x < 0.0) {
    return -0.0;
  } else {
    return ceil(x);
  }
}


static Mutex* limit_mutex = NULL;


void OS::PostSetUp() {
  POSIXPostSetUp();
}


// We keep the lowest and highest addresses mapped as a quick way of
// determining that pointers are outside the heap (used mostly in assertions
// and verification).  The estimate is conservative, i.e., not all addresses in
// 'allocated' space are actually allocated to our heap.  The range is
// [lowest, highest), inclusive on the low and and exclusive on the high end.
static void* lowest_ever_allocated = reinterpret_cast<void*>(-1);
static void* highest_ever_allocated = reinterpret_cast<void*>(0);


static void UpdateAllocatedSpaceLimits(void* address, int size) {
  ASSERT(limit_mutex != NULL);
  ScopedLock lock(limit_mutex);

  lowest_ever_allocated = Min(lowest_ever_allocated, address);
  highest_ever_allocated =
      Max(highest_ever_allocated,
          reinterpret_cast<void*>(reinterpret_cast<char*>(address) + size));
}


bool OS::IsOutsideAllocatedSpace(void* address) {
  return address < lowest_ever_allocated || address >= highest_ever_allocated;
}


size_t OS::AllocateAlignment() {
  return getpagesize();
}


// Constants used for mmap.
// kMmapFd is used to pass vm_alloc flags to tag the region with the user
// defined tag 255 This helps identify V8-allocated regions in memory analysis
// tools like vmmap(1).
static const int kMmapFd = VM_MAKE_TAG(255);
static const off_t kMmapFdOffset = 0;


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, getpagesize());
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(OS::GetRandomMmapAddr(),
                     msize,
                     prot,
                     MAP_PRIVATE | MAP_ANON,
                     kMmapFd,
                     kMmapFdOffset);
  if (mbase == MAP_FAILED) {
    LOG(Isolate::Current(), StringEvent("OS::Allocate", "mmap failed"));
    return NULL;
  }
  *allocated = msize;
  UpdateAllocatedSpaceLimits(mbase, msize);
  return mbase;
}


void OS::Free(void* address, const size_t size) {
  // TODO(1240712): munmap has a return value which is ignored here.
  int result = munmap(address, size);
  USE(result);
  ASSERT(result == 0);
}


void OS::Sleep(int milliseconds) {
  usleep(1000 * milliseconds);
}


int OS::NumberOfCores() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}


void OS::Abort() {
  // Redirect to std abort to signal abnormal program termination
  abort();
}


void OS::DebugBreak() {
  asm("int $3");
}


void OS::DumpBacktrace() {
  // Currently unsupported.
}


class PosixMemoryMappedFile : public OS::MemoryMappedFile {
 public:
  PosixMemoryMappedFile(FILE* file, void* memory, int size)
    : file_(file), memory_(memory), size_(size) { }
  virtual ~PosixMemoryMappedFile();
  virtual void* memory() { return memory_; }
  virtual int size() { return size_; }
 private:
  FILE* file_;
  void* memory_;
  int size_;
};


OS::MemoryMappedFile* OS::MemoryMappedFile::open(const char* name) {
  FILE* file = fopen(name, "r+");
  if (file == NULL) return NULL;

  fseek(file, 0, SEEK_END);
  int size = ftell(file);

  void* memory =
      mmap(OS::GetRandomMmapAddr(),
           size,
           PROT_READ | PROT_WRITE,
           MAP_SHARED,
           fileno(file),
           0);
  return new PosixMemoryMappedFile(file, memory, size);
}


OS::MemoryMappedFile* OS::MemoryMappedFile::create(const char* name, int size,
    void* initial) {
  FILE* file = fopen(name, "w+");
  if (file == NULL) return NULL;
  int result = fwrite(initial, size, 1, file);
  if (result < 1) {
    fclose(file);
    return NULL;
  }
  void* memory =
      mmap(OS::GetRandomMmapAddr(),
          size,
          PROT_READ | PROT_WRITE,
          MAP_SHARED,
          fileno(file),
          0);
  return new PosixMemoryMappedFile(file, memory, size);
}


PosixMemoryMappedFile::~PosixMemoryMappedFile() {
  if (memory_) OS::Free(memory_, size_);
  fclose(file_);
}


void OS::LogSharedLibraryAddresses() {
  unsigned int images_count = _dyld_image_count();
  for (unsigned int i = 0; i < images_count; ++i) {
    const mach_header* header = _dyld_get_image_header(i);
    if (header == NULL) continue;
#if V8_HOST_ARCH_X64
    uint64_t size;
    char* code_ptr = getsectdatafromheader_64(
        reinterpret_cast<const mach_header_64*>(header),
        SEG_TEXT,
        SECT_TEXT,
        &size);
#else
    unsigned int size;
    char* code_ptr = getsectdatafromheader(header, SEG_TEXT, SECT_TEXT, &size);
#endif
    if (code_ptr == NULL) continue;
    const uintptr_t slide = _dyld_get_image_vmaddr_slide(i);
    const uintptr_t start = reinterpret_cast<uintptr_t>(code_ptr) + slide;
    LOG(Isolate::Current(),
        SharedLibraryEvent(_dyld_get_image_name(i), start, start + size));
  }
}


void OS::SignalCodeMovingGC() {
}


uint64_t OS::CpuFeaturesImpliedByPlatform() {
  // MacOSX requires all these to install so we can assume they are present.
  // These constants are defined by the CPUid instructions.
  const uint64_t one = 1;
  return (one << SSE2) | (one << CMOV) | (one << RDTSC) | (one << CPUID);
}


int OS::ActivationFrameAlignment() {
  // OS X activation frames must be 16 byte-aligned; see "Mac OS X ABI
  // Function Call Guide".
  return 16;
}


void OS::ReleaseStore(volatile AtomicWord* ptr, AtomicWord value) {
  OSMemoryBarrier();
  *ptr = value;
}


const char* OS::LocalTimezone(double time) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  return t->tm_zone;
}


double OS::LocalTimeOffset() {
  time_t tv = time(NULL);
  struct tm* t = localtime(&tv);
  // tm_gmtoff includes any daylight savings offset, so subtract it.
  return static_cast<double>(t->tm_gmtoff * msPerSecond -
                             (t->tm_isdst > 0 ? 3600 * msPerSecond : 0));
}


int OS::StackWalk(Vector<StackFrame> frames) {
  // If weak link to execinfo lib has failed, ie because we are on 10.4, abort.
  if (backtrace == NULL)
    return 0;

  int frames_size = frames.length();
  ScopedVector<void*> addresses(frames_size);

  int frames_count = backtrace(addresses.start(), frames_size);

  char** symbols = backtrace_symbols(addresses.start(), frames_count);
  if (symbols == NULL) {
    return kStackWalkError;
  }

  for (int i = 0; i < frames_count; i++) {
    frames[i].address = addresses[i];
    // Format a text representation of the frame based on the information
    // available.
    SNPrintF(MutableCStrVector(frames[i].text,
                               kStackWalkMaxTextLen),
             "%s",
             symbols[i]);
    // Make sure line termination is in place.
    frames[i].text[kStackWalkMaxTextLen - 1] = '\0';
  }

  free(symbols);

  return frames_count;
}


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  ASSERT(IsAligned(alignment, static_cast<intptr_t>(OS::AllocateAlignment())));
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* reservation = mmap(OS::GetRandomMmapAddr(),
                           request_size,
                           PROT_NONE,
                           MAP_PRIVATE | MAP_ANON | MAP_NORESERVE,
                           kMmapFd,
                           kMmapFdOffset);
  if (reservation == MAP_FAILED) return;

  Address base = static_cast<Address>(reservation);
  Address aligned_base = RoundUp(base, alignment);
  ASSERT_LE(base, aligned_base);

  // Unmap extra memory reserved before and after the desired block.
  if (aligned_base != base) {
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    OS::Free(base, prefix_size);
    request_size -= prefix_size;
  }

  size_t aligned_size = RoundUp(size, OS::AllocateAlignment());
  ASSERT_LE(aligned_size, request_size);

  if (aligned_size != request_size) {
    size_t suffix_size = request_size - aligned_size;
    OS::Free(aligned_base + aligned_size, suffix_size);
    request_size -= suffix_size;
  }

  ASSERT(aligned_size == request_size);

  address_ = static_cast<void*>(aligned_base);
  size_ = aligned_size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = ReleaseRegion(address(), size());
    ASSERT(result);
    USE(result);
  }
}


bool VirtualMemory::IsReserved() {
  return address_ != NULL;
}


void VirtualMemory::Reset() {
  address_ = NULL;
  size_ = 0;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  return CommitRegion(address, size, is_executable);
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  return UncommitRegion(address, size);
}


bool VirtualMemory::Guard(void* address) {
  OS::Guard(address, OS::CommitPageSize());
  return true;
}


void* VirtualMemory::ReserveRegion(size_t size) {
  void* result = mmap(OS::GetRandomMmapAddr(),
                      size,
                      PROT_NONE,
                      MAP_PRIVATE | MAP_ANON | MAP_NORESERVE,
                      kMmapFd,
                      kMmapFdOffset);

  if (result == MAP_FAILED) return NULL;

  return result;
}


bool VirtualMemory::CommitRegion(void* address,
                                 size_t size,
                                 bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  if (MAP_FAILED == mmap(address,
                         size,
                         prot,
                         MAP_PRIVATE | MAP_ANON | MAP_FIXED,
                         kMmapFd,
                         kMmapFdOffset)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(address, size);
  return true;
}


bool VirtualMemory::UncommitRegion(void* address, size_t size) {
  return mmap(address,
              size,
              PROT_NONE,
              MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_FIXED,
              kMmapFd,
              kMmapFdOffset) != MAP_FAILED;
}


bool VirtualMemory::ReleaseRegion(void* address, size_t size) {
  return munmap(address, size) == 0;
}


bool VirtualMemory::HasLazyCommits() {
  return false;
}


class Thread::PlatformData : public Malloced {
 public:
  PlatformData() : thread_(kNoThread) {}
  pthread_t thread_;  // Thread handle for pthread.
};


Thread::Thread(const Options& options)
    : data_(new PlatformData),
      stack_size_(options.stack_size()),
      start_semaphore_(NULL) {
  set_name(options.name());
}


Thread::~Thread() {
  delete data_;
}


static void SetThreadName(const char* name) {
  // pthread_setname_np is only available in 10.6 or later, so test
  // for it at runtime.
  int (*dynamic_pthread_setname_np)(const char*);
  *reinterpret_cast<void**>(&dynamic_pthread_setname_np) =
    dlsym(RTLD_DEFAULT, "pthread_setname_np");
  if (!dynamic_pthread_setname_np)
    return;

  // Mac OS X does not expose the length limit of the name, so hardcode it.
  static const int kMaxNameLength = 63;
  USE(kMaxNameLength);
  ASSERT(Thread::kMaxThreadNameLength <= kMaxNameLength);
  dynamic_pthread_setname_np(name);
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // This is also initialized by the first argument to pthread_create() but we
  // don't know which thread will run first (the original thread or the new
  // one) so we initialize it here too.
  thread->data()->thread_ = pthread_self();
  SetThreadName(thread->name());
  ASSERT(thread->data()->thread_ != kNoThread);
  thread->NotifyStartedAndRun();
  return NULL;
}


void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}


void Thread::Start() {
  pthread_attr_t* attr_ptr = NULL;
  pthread_attr_t attr;
  if (stack_size_ > 0) {
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, static_cast<size_t>(stack_size_));
    attr_ptr = &attr;
  }
  pthread_create(&data_->thread_, attr_ptr, ThreadEntry, this);
  ASSERT(data_->thread_ != kNoThread);
}


void Thread::Join() {
  pthread_join(data_->thread_, NULL);
}


#ifdef V8_FAST_TLS_SUPPORTED

static Atomic32 tls_base_offset_initialized = 0;
intptr_t kMacTlsBaseOffset = 0;

// It's safe to do the initialization more that once, but it has to be
// done at least once.
static void InitializeTlsBaseOffset() {
  const size_t kBufferSize = 128;
  char buffer[kBufferSize];
  size_t buffer_size = kBufferSize;
  int ctl_name[] = { CTL_KERN , KERN_OSRELEASE };
  if (sysctl(ctl_name, 2, buffer, &buffer_size, NULL, 0) != 0) {
    V8_Fatal(__FILE__, __LINE__, "V8 failed to get kernel version");
  }
  // The buffer now contains a string of the form XX.YY.ZZ, where
  // XX is the major kernel version component.
  // Make sure the buffer is 0-terminated.
  buffer[kBufferSize - 1] = '\0';
  char* period_pos = strchr(buffer, '.');
  *period_pos = '\0';
  int kernel_version_major =
      static_cast<int>(strtol(buffer, NULL, 10));  // NOLINT
  // The constants below are taken from pthreads.s from the XNU kernel
  // sources archive at www.opensource.apple.com.
  if (kernel_version_major < 11) {
    // 8.x.x (Tiger), 9.x.x (Leopard), 10.x.x (Snow Leopard) have the
    // same offsets.
#if defined(V8_HOST_ARCH_IA32)
    kMacTlsBaseOffset = 0x48;
#else
    kMacTlsBaseOffset = 0x60;
#endif
  } else {
    // 11.x.x (Lion) changed the offset.
    kMacTlsBaseOffset = 0;
  }

  Release_Store(&tls_base_offset_initialized, 1);
}

static void CheckFastTls(Thread::LocalStorageKey key) {
  void* expected = reinterpret_cast<void*>(0x1234CAFE);
  Thread::SetThreadLocal(key, expected);
  void* actual = Thread::GetExistingThreadLocal(key);
  if (expected != actual) {
    V8_Fatal(__FILE__, __LINE__,
             "V8 failed to initialize fast TLS on current kernel");
  }
  Thread::SetThreadLocal(key, NULL);
}

#endif  // V8_FAST_TLS_SUPPORTED


Thread::LocalStorageKey Thread::CreateThreadLocalKey() {
#ifdef V8_FAST_TLS_SUPPORTED
  bool check_fast_tls = false;
  if (tls_base_offset_initialized == 0) {
    check_fast_tls = true;
    InitializeTlsBaseOffset();
  }
#endif
  pthread_key_t key;
  int result = pthread_key_create(&key, NULL);
  USE(result);
  ASSERT(result == 0);
  LocalStorageKey typed_key = static_cast<LocalStorageKey>(key);
#ifdef V8_FAST_TLS_SUPPORTED
  // If we just initialized fast TLS support, make sure it works.
  if (check_fast_tls) CheckFastTls(typed_key);
#endif
  return typed_key;
}


void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  pthread_key_t pthread_key = static_cast<pthread_key_t>(key);
  int result = pthread_key_delete(pthread_key);
  USE(result);
  ASSERT(result == 0);
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  pthread_key_t pthread_key = static_cast<pthread_key_t>(key);
  return pthread_getspecific(pthread_key);
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  pthread_key_t pthread_key = static_cast<pthread_key_t>(key);
  pthread_setspecific(pthread_key, value);
}


void Thread::YieldCPU() {
  sched_yield();
}


class MacOSMutex : public Mutex {
 public:
  MacOSMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &attr);
  }

  virtual ~MacOSMutex() { pthread_mutex_destroy(&mutex_); }

  virtual int Lock() { return pthread_mutex_lock(&mutex_); }
  virtual int Unlock() { return pthread_mutex_unlock(&mutex_); }

  virtual bool TryLock() {
    int result = pthread_mutex_trylock(&mutex_);
    // Return false if the lock is busy and locking failed.
    if (result == EBUSY) {
      return false;
    }
    ASSERT(result == 0);  // Verify no other errors.
    return true;
  }

 private:
  pthread_mutex_t mutex_;
};


Mutex* OS::CreateMutex() {
  return new MacOSMutex();
}


class MacOSSemaphore : public Semaphore {
 public:
  explicit MacOSSemaphore(int count) {
    int r;
    r = semaphore_create(mach_task_self(),
                         &semaphore_,
                         SYNC_POLICY_FIFO,
                         count);
    ASSERT(r == KERN_SUCCESS);
  }

  ~MacOSSemaphore() {
    int r;
    r = semaphore_destroy(mach_task_self(), semaphore_);
    ASSERT(r == KERN_SUCCESS);
  }

  void Wait() {
    int r;
    do {
      r = semaphore_wait(semaphore_);
      ASSERT(r == KERN_SUCCESS || r == KERN_ABORTED);
    } while (r == KERN_ABORTED);
  }

  bool Wait(int timeout);

  void Signal() { semaphore_signal(semaphore_); }

 private:
  semaphore_t semaphore_;
};


bool MacOSSemaphore::Wait(int timeout) {
  mach_timespec_t ts;
  ts.tv_sec = timeout / 1000000;
  ts.tv_nsec = (timeout % 1000000) * 1000;
  return semaphore_timedwait(semaphore_, ts) != KERN_OPERATION_TIMED_OUT;
}


Semaphore* OS::CreateSemaphore(int count) {
  return new MacOSSemaphore(count);
}


void OS::SetUp() {
  // Seed the random number generator. We preserve microsecond resolution.
  uint64_t seed = Ticks() ^ (getpid() << 16);
  srandom(static_cast<unsigned int>(seed));
  limit_mutex = CreateMutex();
}


void OS::TearDown() {
  delete limit_mutex;
}


} }  // namespace v8::internal
