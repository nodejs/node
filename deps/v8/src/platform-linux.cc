// Copyright 2011 the V8 project authors. All rights reserved.
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

// Platform specific code for Linux goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>

// Ubuntu Dapper requires memory pages to be marked as
// executable. Otherwise, OS raises an exception when executing code
// in that page.
#include <sys/types.h>  // mmap & munmap
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <fcntl.h>      // open
#include <unistd.h>     // sysconf
#ifdef __GLIBC__
#include <execinfo.h>   // backtrace, backtrace_symbols
#endif  // def __GLIBC__
#include <strings.h>    // index
#include <errno.h>
#include <stdarg.h>

#undef MAP_TYPE

#include "v8.h"

#include "platform.h"
#include "v8threads.h"
#include "vm-state-inl.h"


namespace v8 {
namespace internal {

// 0 is never a valid thread id on Linux since tids and pids share a
// name space and pid 0 is reserved (see man 2 kill).
static const pthread_t kNoThread = (pthread_t) 0;


double ceiling(double x) {
  return ceil(x);
}


static Mutex* limit_mutex = NULL;


static void* GetRandomMmapAddr() {
  Isolate* isolate = Isolate::UncheckedCurrent();
  // Note that the current isolate isn't set up in a call path via
  // CpuFeatures::Probe. We don't care about randomization in this case because
  // the code page is immediately freed.
  if (isolate != NULL) {
#ifdef V8_TARGET_ARCH_X64
    uint64_t rnd1 = V8::RandomPrivate(isolate);
    uint64_t rnd2 = V8::RandomPrivate(isolate);
    uint64_t raw_addr = (rnd1 << 32) ^ rnd2;
    raw_addr &= V8_UINT64_C(0x3ffffffff000);
#else
    uint32_t raw_addr = V8::RandomPrivate(isolate);
    // The range 0x20000000 - 0x60000000 is relatively unpopulated across a
    // variety of ASLR modes (PAE kernel, NX compat mode, etc).
    raw_addr &= 0x3ffff000;
    raw_addr += 0x20000000;
#endif
    return reinterpret_cast<void*>(raw_addr);
  }
  return NULL;
}


void OS::Setup() {
  // Seed the random number generator. We preserve microsecond resolution.
  uint64_t seed = Ticks() ^ (getpid() << 16);
  srandom(static_cast<unsigned int>(seed));
  limit_mutex = CreateMutex();

#ifdef __arm__
  // When running on ARM hardware check that the EABI used by V8 and
  // by the C code is the same.
  bool hard_float = OS::ArmUsingHardFloat();
  if (hard_float) {
#if !USE_EABI_HARDFLOAT
    PrintF("ERROR: Binary compiled with -mfloat-abi=hard but without "
           "-DUSE_EABI_HARDFLOAT\n");
    exit(1);
#endif
  } else {
#if USE_EABI_HARDFLOAT
    PrintF("ERROR: Binary not compiled with -mfloat-abi=hard but with "
           "-DUSE_EABI_HARDFLOAT\n");
    exit(1);
#endif
  }
#endif
}


uint64_t OS::CpuFeaturesImpliedByPlatform() {
  return 0;  // Linux runs on anything.
}


#ifdef __arm__
static bool CPUInfoContainsString(const char * search_string) {
  const char* file_name = "/proc/cpuinfo";
  // This is written as a straight shot one pass parser
  // and not using STL string and ifstream because,
  // on Linux, it's reading from a (non-mmap-able)
  // character special device.
  FILE* f = NULL;
  const char* what = search_string;

  if (NULL == (f = fopen(file_name, "r")))
    return false;

  int k;
  while (EOF != (k = fgetc(f))) {
    if (k == *what) {
      ++what;
      while ((*what != '\0') && (*what == fgetc(f))) {
        ++what;
      }
      if (*what == '\0') {
        fclose(f);
        return true;
      } else {
        what = search_string;
      }
    }
  }
  fclose(f);

  // Did not find string in the proc file.
  return false;
}


bool OS::ArmCpuHasFeature(CpuFeature feature) {
  const char* search_string = NULL;
  // Simple detection of VFP at runtime for Linux.
  // It is based on /proc/cpuinfo, which reveals hardware configuration
  // to user-space applications.  According to ARM (mid 2009), no similar
  // facility is universally available on the ARM architectures,
  // so it's up to individual OSes to provide such.
  switch (feature) {
    case VFP3:
      search_string = "vfpv3";
      break;
    case ARMv7:
      search_string = "ARMv7";
      break;
    default:
      UNREACHABLE();
  }

  if (CPUInfoContainsString(search_string)) {
    return true;
  }

  if (feature == VFP3) {
    // Some old kernels will report vfp not vfpv3. Here we make a last attempt
    // to detect vfpv3 by checking for vfp *and* neon, since neon is only
    // available on architectures with vfpv3.
    // Checking neon on its own is not enough as it is possible to have neon
    // without vfp.
    if (CPUInfoContainsString("vfp") && CPUInfoContainsString("neon")) {
      return true;
    }
  }

  return false;
}


// Simple helper function to detect whether the C code is compiled with
// option -mfloat-abi=hard. The register d0 is loaded with 1.0 and the register
// pair r0, r1 is loaded with 0.0. If -mfloat-abi=hard is pased to GCC then
// calling this will return 1.0 and otherwise 0.0.
static void ArmUsingHardFloatHelper() {
  asm("mov r0, #0");
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
  // Load 0x3ff00000 into r1 using instructions available in both ARM
  // and Thumb mode.
  asm("mov r1, #3");
  asm("mov r2, #255");
  asm("lsl r1, r1, #8");
  asm("orr r1, r1, r2");
  asm("lsl r1, r1, #20");
  // For vmov d0, r0, r1 use ARM mode.
#ifdef __thumb__
  asm volatile(
    "@   Enter ARM Mode  \n\t"
    "    adr r3, 1f      \n\t"
    "    bx  r3          \n\t"
    "    .ALIGN 4        \n\t"
    "    .ARM            \n"
    "1:  vmov d0, r0, r1 \n\t"
    "@   Enter THUMB Mode\n\t"
    "    adr r3, 2f+1    \n\t"
    "    bx  r3          \n\t"
    "    .THUMB          \n"
    "2:                  \n\t");
#else
  asm("vmov d0, r0, r1");
#endif  // __thumb__
#endif  // defined(__VFP_FP__) && !defined(__SOFTFP__)
  asm("mov r1, #0");
}


bool OS::ArmUsingHardFloat() {
  // Cast helper function from returning void to returning double.
  typedef double (*F)();
  F f = FUNCTION_CAST<F>(FUNCTION_ADDR(ArmUsingHardFloatHelper));
  return f() == 1.0;
}
#endif  // def __arm__


#ifdef __mips__
bool OS::MipsCpuHasFeature(CpuFeature feature) {
  const char* search_string = NULL;
  const char* file_name = "/proc/cpuinfo";
  // Simple detection of FPU at runtime for Linux.
  // It is based on /proc/cpuinfo, which reveals hardware configuration
  // to user-space applications.  According to MIPS (early 2010), no similar
  // facility is universally available on the MIPS architectures,
  // so it's up to individual OSes to provide such.
  //
  // This is written as a straight shot one pass parser
  // and not using STL string and ifstream because,
  // on Linux, it's reading from a (non-mmap-able)
  // character special device.

  switch (feature) {
    case FPU:
      search_string = "FPU";
      break;
    default:
      UNREACHABLE();
  }

  FILE* f = NULL;
  const char* what = search_string;

  if (NULL == (f = fopen(file_name, "r")))
    return false;

  int k;
  while (EOF != (k = fgetc(f))) {
    if (k == *what) {
      ++what;
      while ((*what != '\0') && (*what == fgetc(f))) {
        ++what;
      }
      if (*what == '\0') {
        fclose(f);
        return true;
      } else {
        what = search_string;
      }
    }
  }
  fclose(f);

  // Did not find string in the proc file.
  return false;
}
#endif  // def __mips__


int OS::ActivationFrameAlignment() {
#ifdef V8_TARGET_ARCH_ARM
  // On EABI ARM targets this is required for fp correctness in the
  // runtime system.
  return 8;
#elif V8_TARGET_ARCH_MIPS
  return 8;
#endif
  // With gcc 4.4 the tree vectorization optimizer can generate code
  // that requires 16 byte alignment such as movdqa on x86.
  return 16;
}


void OS::ReleaseStore(volatile AtomicWord* ptr, AtomicWord value) {
#if (defined(V8_TARGET_ARCH_ARM) && defined(__arm__)) || \
    (defined(V8_TARGET_ARCH_MIPS) && defined(__mips__))
  // Only use on ARM or MIPS hardware.
  MemoryBarrier();
#else
  __asm__ __volatile__("" : : : "memory");
  // An x86 store acts as a release barrier.
#endif
  *ptr = value;
}


const char* OS::LocalTimezone(double time) {
  if (isnan(time)) return "";
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


// We keep the lowest and highest addresses mapped as a quick way of
// determining that pointers are outside the heap (used mostly in assertions
// and verification).  The estimate is conservative, ie, not all addresses in
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
  return sysconf(_SC_PAGESIZE);
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, sysconf(_SC_PAGESIZE));
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* addr = GetRandomMmapAddr();
  void* mbase = mmap(addr, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mbase == MAP_FAILED) {
    LOG(i::Isolate::Current(),
        StringEvent("OS::Allocate", "mmap failed"));
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
  unsigned int ms = static_cast<unsigned int>(milliseconds);
  usleep(1000 * ms);
}


void OS::Abort() {
  // Redirect to std abort to signal abnormal program termination.
  abort();
}


void OS::DebugBreak() {
// TODO(lrn): Introduce processor define for runtime system (!= V8_ARCH_x,
//  which is the architecture of generated code).
#if (defined(__arm__) || defined(__thumb__))
# if defined(CAN_USE_ARMV5_INSTRUCTIONS)
  asm("bkpt 0");
# endif
#elif defined(__mips__)
  asm("break");
#else
  asm("int $3");
#endif
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
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
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
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


PosixMemoryMappedFile::~PosixMemoryMappedFile() {
  if (memory_) munmap(memory_, size_);
  fclose(file_);
}


void OS::LogSharedLibraryAddresses() {
  // This function assumes that the layout of the file is as follows:
  // hex_start_addr-hex_end_addr rwxp <unused data> [binary_file_name]
  // If we encounter an unexpected situation we abort scanning further entries.
  FILE* fp = fopen("/proc/self/maps", "r");
  if (fp == NULL) return;

  // Allocate enough room to be able to store a full file name.
  const int kLibNameLen = FILENAME_MAX + 1;
  char* lib_name = reinterpret_cast<char*>(malloc(kLibNameLen));

  i::Isolate* isolate = ISOLATE;
  // This loop will terminate once the scanning hits an EOF.
  while (true) {
    uintptr_t start, end;
    char attr_r, attr_w, attr_x, attr_p;
    // Parse the addresses and permission bits at the beginning of the line.
    if (fscanf(fp, "%" V8PRIxPTR "-%" V8PRIxPTR, &start, &end) != 2) break;
    if (fscanf(fp, " %c%c%c%c", &attr_r, &attr_w, &attr_x, &attr_p) != 4) break;

    int c;
    if (attr_r == 'r' && attr_w != 'w' && attr_x == 'x') {
      // Found a read-only executable entry. Skip characters until we reach
      // the beginning of the filename or the end of the line.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n') && (c != '/'));
      if (c == EOF) break;  // EOF: Was unexpected, just exit.

      // Process the filename if found.
      if (c == '/') {
        ungetc(c, fp);  // Push the '/' back into the stream to be read below.

        // Read to the end of the line. Exit if the read fails.
        if (fgets(lib_name, kLibNameLen, fp) == NULL) break;

        // Drop the newline character read by fgets. We do not need to check
        // for a zero-length string because we know that we at least read the
        // '/' character.
        lib_name[strlen(lib_name) - 1] = '\0';
      } else {
        // No library name found, just record the raw address range.
        snprintf(lib_name, kLibNameLen,
                 "%08" V8PRIxPTR "-%08" V8PRIxPTR, start, end);
      }
      LOG(isolate, SharedLibraryEvent(lib_name, start, end));
    } else {
      // Entry not describing executable data. Skip to end of line to setup
      // reading the next entry.
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n'));
      if (c == EOF) break;
    }
  }
  free(lib_name);
  fclose(fp);
}


static const char kGCFakeMmap[] = "/tmp/__v8_gc__";


void OS::SignalCodeMovingGC() {
  // Support for ll_prof.py.
  //
  // The Linux profiler built into the kernel logs all mmap's with
  // PROT_EXEC so that analysis tools can properly attribute ticks. We
  // do a mmap with a name known by ll_prof.py and immediately munmap
  // it. This injects a GC marker into the stream of events generated
  // by the kernel and allows us to synchronize V8 code log and the
  // kernel log.
  int size = sysconf(_SC_PAGESIZE);
  FILE* f = fopen(kGCFakeMmap, "w+");
  void* addr = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_PRIVATE,
                    fileno(f), 0);
  ASSERT(addr != MAP_FAILED);
  munmap(addr, size);
  fclose(f);
}


int OS::StackWalk(Vector<OS::StackFrame> frames) {
  // backtrace is a glibc extension.
#ifdef __GLIBC__
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
    SNPrintF(MutableCStrVector(frames[i].text, kStackWalkMaxTextLen),
             "%s",
             symbols[i]);
    // Make sure line termination is in place.
    frames[i].text[kStackWalkMaxTextLen - 1] = '\0';
  }

  free(symbols);

  return frames_count;
#else  // ndef __GLIBC__
  return 0;
#endif  // ndef __GLIBC__
}


// Constants used for mmap.
static const int kMmapFd = -1;
static const int kMmapFdOffset = 0;


VirtualMemory::VirtualMemory(size_t size) {
  address_ = mmap(GetRandomMmapAddr(), size, PROT_NONE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                  kMmapFd, kMmapFdOffset);
  size_ = size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    if (0 == munmap(address(), size())) address_ = MAP_FAILED;
  }
}


bool VirtualMemory::IsReserved() {
  return address_ != MAP_FAILED;
}


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  if (MAP_FAILED == mmap(address, size, prot,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                         kMmapFd, kMmapFdOffset)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(address, size);
  return true;
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  return mmap(address, size, PROT_NONE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
              kMmapFd, kMmapFdOffset) != MAP_FAILED;
}


class Thread::PlatformData : public Malloced {
 public:
  PlatformData() : thread_(kNoThread) {}

  pthread_t thread_;  // Thread handle for pthread.
};

Thread::Thread(const Options& options)
    : data_(new PlatformData()),
      stack_size_(options.stack_size) {
  set_name(options.name);
}


Thread::Thread(const char* name)
    : data_(new PlatformData()),
      stack_size_(0) {
  set_name(name);
}


Thread::~Thread() {
  delete data_;
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // This is also initialized by the first argument to pthread_create() but we
  // don't know which thread will run first (the original thread or the new
  // one) so we initialize it here too.
#ifdef PR_SET_NAME
  prctl(PR_SET_NAME,
        reinterpret_cast<unsigned long>(thread->name()),  // NOLINT
        0, 0, 0);
#endif
  thread->data()->thread_ = pthread_self();
  ASSERT(thread->data()->thread_ != kNoThread);
  thread->Run();
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


Thread::LocalStorageKey Thread::CreateThreadLocalKey() {
  pthread_key_t key;
  int result = pthread_key_create(&key, NULL);
  USE(result);
  ASSERT(result == 0);
  return static_cast<LocalStorageKey>(key);
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


class LinuxMutex : public Mutex {
 public:
  LinuxMutex() {
    pthread_mutexattr_t attrs;
    int result = pthread_mutexattr_init(&attrs);
    ASSERT(result == 0);
    result = pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE);
    ASSERT(result == 0);
    result = pthread_mutex_init(&mutex_, &attrs);
    ASSERT(result == 0);
    USE(result);
  }

  virtual ~LinuxMutex() { pthread_mutex_destroy(&mutex_); }

  virtual int Lock() {
    int result = pthread_mutex_lock(&mutex_);
    return result;
  }

  virtual int Unlock() {
    int result = pthread_mutex_unlock(&mutex_);
    return result;
  }

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
  pthread_mutex_t mutex_;   // Pthread mutex for POSIX platforms.
};


Mutex* OS::CreateMutex() {
  return new LinuxMutex();
}


class LinuxSemaphore : public Semaphore {
 public:
  explicit LinuxSemaphore(int count) {  sem_init(&sem_, 0, count); }
  virtual ~LinuxSemaphore() { sem_destroy(&sem_); }

  virtual void Wait();
  virtual bool Wait(int timeout);
  virtual void Signal() { sem_post(&sem_); }
 private:
  sem_t sem_;
};


void LinuxSemaphore::Wait() {
  while (true) {
    int result = sem_wait(&sem_);
    if (result == 0) return;  // Successfully got semaphore.
    CHECK(result == -1 && errno == EINTR);  // Signal caused spurious wakeup.
  }
}


#ifndef TIMEVAL_TO_TIMESPEC
#define TIMEVAL_TO_TIMESPEC(tv, ts) do {                            \
    (ts)->tv_sec = (tv)->tv_sec;                                    \
    (ts)->tv_nsec = (tv)->tv_usec * 1000;                           \
} while (false)
#endif


bool LinuxSemaphore::Wait(int timeout) {
  const long kOneSecondMicros = 1000000;  // NOLINT

  // Split timeout into second and nanosecond parts.
  struct timeval delta;
  delta.tv_usec = timeout % kOneSecondMicros;
  delta.tv_sec = timeout / kOneSecondMicros;

  struct timeval current_time;
  // Get the current time.
  if (gettimeofday(&current_time, NULL) == -1) {
    return false;
  }

  // Calculate time for end of timeout.
  struct timeval end_time;
  timeradd(&current_time, &delta, &end_time);

  struct timespec ts;
  TIMEVAL_TO_TIMESPEC(&end_time, &ts);
  // Wait for semaphore signalled or timeout.
  while (true) {
    int result = sem_timedwait(&sem_, &ts);
    if (result == 0) return true;  // Successfully got semaphore.
    if (result > 0) {
      // For glibc prior to 2.3.4 sem_timedwait returns the error instead of -1.
      errno = result;
      result = -1;
    }
    if (result == -1 && errno == ETIMEDOUT) return false;  // Timeout.
    CHECK(result == -1 && errno == EINTR);  // Signal caused spurious wakeup.
  }
}


Semaphore* OS::CreateSemaphore(int count) {
  return new LinuxSemaphore(count);
}


#if !defined(__GLIBC__) && (defined(__arm__) || defined(__thumb__))
// Android runs a fairly new Linux kernel, so signal info is there,
// but the C library doesn't have the structs defined.

struct sigcontext {
  uint32_t trap_no;
  uint32_t error_code;
  uint32_t oldmask;
  uint32_t gregs[16];
  uint32_t arm_cpsr;
  uint32_t fault_address;
};
typedef uint32_t __sigset_t;
typedef struct sigcontext mcontext_t;
typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  __sigset_t uc_sigmask;
} ucontext_t;
enum ArmRegisters {R15 = 15, R13 = 13, R11 = 11};

#endif


static int GetThreadID() {
  // Glibc doesn't provide a wrapper for gettid(2).
#if defined(ANDROID)
  return syscall(__NR_gettid);
#else
  return syscall(SYS_gettid);
#endif
}


static void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
#ifndef V8_HOST_ARCH_MIPS
  USE(info);
  if (signal != SIGPROF) return;
  Isolate* isolate = Isolate::UncheckedCurrent();
  if (isolate == NULL || !isolate->IsInitialized() || !isolate->IsInUse()) {
    // We require a fully initialized and entered isolate.
    return;
  }
  if (v8::Locker::IsActive() &&
      !isolate->thread_manager()->IsLockedByCurrentThread()) {
    return;
  }

  Sampler* sampler = isolate->logger()->sampler();
  if (sampler == NULL || !sampler->IsActive()) return;

  TickSample sample_obj;
  TickSample* sample = CpuProfiler::TickSampleEvent(isolate);
  if (sample == NULL) sample = &sample_obj;

  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  mcontext_t& mcontext = ucontext->uc_mcontext;
  sample->state = isolate->current_vm_state();
#if V8_HOST_ARCH_IA32
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_EIP]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_ESP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_EBP]);
#elif V8_HOST_ARCH_X64
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_RIP]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_RSP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_RBP]);
#elif V8_HOST_ARCH_ARM
// An undefined macro evaluates to 0, so this applies to Android's Bionic also.
#if (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 3))
  sample->pc = reinterpret_cast<Address>(mcontext.gregs[R15]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[R13]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[R11]);
#else
  sample->pc = reinterpret_cast<Address>(mcontext.arm_pc);
  sample->sp = reinterpret_cast<Address>(mcontext.arm_sp);
  sample->fp = reinterpret_cast<Address>(mcontext.arm_fp);
#endif
#elif V8_HOST_ARCH_MIPS
  sample.pc = reinterpret_cast<Address>(mcontext.pc);
  sample.sp = reinterpret_cast<Address>(mcontext.gregs[29]);
  sample.fp = reinterpret_cast<Address>(mcontext.gregs[30]);
#endif
  sampler->SampleStack(sample);
  sampler->Tick(sample);
#endif
}


class Sampler::PlatformData : public Malloced {
 public:
  PlatformData() : vm_tid_(GetThreadID()) {}

  int vm_tid() const { return vm_tid_; }

 private:
  const int vm_tid_;
};


class SignalSender : public Thread {
 public:
  enum SleepInterval {
    HALF_INTERVAL,
    FULL_INTERVAL
  };

  explicit SignalSender(int interval)
      : Thread("SignalSender"),
        vm_tgid_(getpid()),
        interval_(interval) {}

  static void InstallSignalHandler() {
    struct sigaction sa;
    sa.sa_sigaction = ProfilerSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    signal_handler_installed_ =
        (sigaction(SIGPROF, &sa, &old_signal_handler_) == 0);
  }

  static void RestoreSignalHandler() {
    if (signal_handler_installed_) {
      sigaction(SIGPROF, &old_signal_handler_, 0);
      signal_handler_installed_ = false;
    }
  }

  static void AddActiveSampler(Sampler* sampler) {
    ScopedLock lock(mutex_);
    SamplerRegistry::AddActiveSampler(sampler);
    if (instance_ == NULL) {
      // Start a thread that will send SIGPROF signal to VM threads,
      // when CPU profiling will be enabled.
      instance_ = new SignalSender(sampler->interval());
      instance_->Start();
    } else {
      ASSERT(instance_->interval_ == sampler->interval());
    }
  }

  static void RemoveActiveSampler(Sampler* sampler) {
    ScopedLock lock(mutex_);
    SamplerRegistry::RemoveActiveSampler(sampler);
    if (SamplerRegistry::GetState() == SamplerRegistry::HAS_NO_SAMPLERS) {
      RuntimeProfiler::StopRuntimeProfilerThreadBeforeShutdown(instance_);
      delete instance_;
      instance_ = NULL;
      RestoreSignalHandler();
    }
  }

  // Implement Thread::Run().
  virtual void Run() {
    SamplerRegistry::State state;
    while ((state = SamplerRegistry::GetState()) !=
           SamplerRegistry::HAS_NO_SAMPLERS) {
      bool cpu_profiling_enabled =
          (state == SamplerRegistry::HAS_CPU_PROFILING_SAMPLERS);
      bool runtime_profiler_enabled = RuntimeProfiler::IsEnabled();
      if (cpu_profiling_enabled && !signal_handler_installed_) {
        InstallSignalHandler();
      } else if (!cpu_profiling_enabled && signal_handler_installed_) {
        RestoreSignalHandler();
      }
      // When CPU profiling is enabled both JavaScript and C++ code is
      // profiled. We must not suspend.
      if (!cpu_profiling_enabled) {
        if (rate_limiter_.SuspendIfNecessary()) continue;
      }
      if (cpu_profiling_enabled && runtime_profiler_enabled) {
        if (!SamplerRegistry::IterateActiveSamplers(&DoCpuProfile, this)) {
          return;
        }
        Sleep(HALF_INTERVAL);
        if (!SamplerRegistry::IterateActiveSamplers(&DoRuntimeProfile, NULL)) {
          return;
        }
        Sleep(HALF_INTERVAL);
      } else {
        if (cpu_profiling_enabled) {
          if (!SamplerRegistry::IterateActiveSamplers(&DoCpuProfile,
                                                      this)) {
            return;
          }
        }
        if (runtime_profiler_enabled) {
          if (!SamplerRegistry::IterateActiveSamplers(&DoRuntimeProfile,
                                                      NULL)) {
            return;
          }
        }
        Sleep(FULL_INTERVAL);
      }
    }
  }

  static void DoCpuProfile(Sampler* sampler, void* raw_sender) {
    if (!sampler->IsProfiling()) return;
    SignalSender* sender = reinterpret_cast<SignalSender*>(raw_sender);
    sender->SendProfilingSignal(sampler->platform_data()->vm_tid());
  }

  static void DoRuntimeProfile(Sampler* sampler, void* ignored) {
    if (!sampler->isolate()->IsInitialized()) return;
    sampler->isolate()->runtime_profiler()->NotifyTick();
  }

  void SendProfilingSignal(int tid) {
    if (!signal_handler_installed_) return;
    // Glibc doesn't provide a wrapper for tgkill(2).
#if defined(ANDROID)
    syscall(__NR_tgkill, vm_tgid_, tid, SIGPROF);
#else
    syscall(SYS_tgkill, vm_tgid_, tid, SIGPROF);
#endif
  }

  void Sleep(SleepInterval full_or_half) {
    // Convert ms to us and subtract 100 us to compensate delays
    // occuring during signal delivery.
    useconds_t interval = interval_ * 1000 - 100;
    if (full_or_half == HALF_INTERVAL) interval /= 2;
    int result = usleep(interval);
#ifdef DEBUG
    if (result != 0 && errno != EINTR) {
      fprintf(stderr,
              "SignalSender usleep error; interval = %u, errno = %d\n",
              interval,
              errno);
      ASSERT(result == 0 || errno == EINTR);
    }
#endif
    USE(result);
  }

  const int vm_tgid_;
  const int interval_;
  RuntimeProfilerRateLimiter rate_limiter_;

  // Protects the process wide state below.
  static Mutex* mutex_;
  static SignalSender* instance_;
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;

  DISALLOW_COPY_AND_ASSIGN(SignalSender);
};


Mutex* SignalSender::mutex_ = OS::CreateMutex();
SignalSender* SignalSender::instance_ = NULL;
struct sigaction SignalSender::old_signal_handler_;
bool SignalSender::signal_handler_installed_ = false;


Sampler::Sampler(Isolate* isolate, int interval)
    : isolate_(isolate),
      interval_(interval),
      profiling_(false),
      active_(false),
      samples_taken_(0) {
  data_ = new PlatformData;
}


Sampler::~Sampler() {
  ASSERT(!IsActive());
  delete data_;
}


void Sampler::Start() {
  ASSERT(!IsActive());
  SetActive(true);
  SignalSender::AddActiveSampler(this);
}


void Sampler::Stop() {
  ASSERT(IsActive());
  SignalSender::RemoveActiveSampler(this);
  SetActive(false);
}


} }  // namespace v8::internal
