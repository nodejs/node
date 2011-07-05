// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include "top.h"
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


void OS::Setup() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly can cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srandom(static_cast<unsigned int>(seed));
}


uint64_t OS::CpuFeaturesImpliedByPlatform() {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
  // Here gcc is telling us that we are on an ARM and gcc is assuming that we
  // have VFP3 instructions.  If gcc can assume it then so can we.
  return 1u << VFP3;
#elif CAN_USE_ARMV7_INSTRUCTIONS
  return 1u << ARMv7;
#else
  return 0;  // Linux runs on anything.
#endif
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
#endif  // def __arm__


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
#if defined(V8_TARGET_ARCH_ARM) && defined(__arm__)
  // Only use on ARM hardware.
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
  // TODO(805): Port randomization of allocated executable memory to Linux.
  const size_t msize = RoundUp(requested, sysconf(_SC_PAGESIZE));
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(NULL, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mbase == MAP_FAILED) {
    LOG(StringEvent("OS::Allocate", "mmap failed"));
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


#ifdef ENABLE_HEAP_PROTECTION

void OS::Protect(void* address, size_t size) {
  // TODO(1240712): mprotect has a return value which is ignored here.
  mprotect(address, size, PROT_READ);
}


void OS::Unprotect(void* address, size_t size, bool is_executable) {
  // TODO(1240712): mprotect has a return value which is ignored here.
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  mprotect(address, size, prot);
}

#endif


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
#ifdef ENABLE_LOGGING_AND_PROFILING
  // This function assumes that the layout of the file is as follows:
  // hex_start_addr-hex_end_addr rwxp <unused data> [binary_file_name]
  // If we encounter an unexpected situation we abort scanning further entries.
  FILE* fp = fopen("/proc/self/maps", "r");
  if (fp == NULL) return;

  // Allocate enough room to be able to store a full file name.
  const int kLibNameLen = FILENAME_MAX + 1;
  char* lib_name = reinterpret_cast<char*>(malloc(kLibNameLen));

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
      LOG(SharedLibraryEvent(lib_name, start, end));
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
#endif
}


static const char kGCFakeMmap[] = "/tmp/__v8_gc__";


void OS::SignalCodeMovingGC() {
#ifdef ENABLE_LOGGING_AND_PROFILING
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
#endif
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
  address_ = mmap(NULL, size, PROT_NONE,
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


class ThreadHandle::PlatformData : public Malloced {
 public:
  explicit PlatformData(ThreadHandle::Kind kind) {
    Initialize(kind);
  }

  void Initialize(ThreadHandle::Kind kind) {
    switch (kind) {
      case ThreadHandle::SELF: thread_ = pthread_self(); break;
      case ThreadHandle::INVALID: thread_ = kNoThread; break;
    }
  }

  pthread_t thread_;  // Thread handle for pthread.
};


ThreadHandle::ThreadHandle(Kind kind) {
  data_ = new PlatformData(kind);
}


void ThreadHandle::Initialize(ThreadHandle::Kind kind) {
  data_->Initialize(kind);
}


ThreadHandle::~ThreadHandle() {
  delete data_;
}


bool ThreadHandle::IsSelf() const {
  return pthread_equal(data_->thread_, pthread_self());
}


bool ThreadHandle::IsValid() const {
  return data_->thread_ != kNoThread;
}


Thread::Thread() : ThreadHandle(ThreadHandle::INVALID) {
  set_name("v8:<unknown>");
}


Thread::Thread(const char* name) : ThreadHandle(ThreadHandle::INVALID) {
  set_name(name);
}


Thread::~Thread() {
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // This is also initialized by the first argument to pthread_create() but we
  // don't know which thread will run first (the original thread or the new
  // one) so we initialize it here too.
  prctl(PR_SET_NAME, thread->name(), 0, 0, 0);
  thread->thread_handle_data()->thread_ = pthread_self();
  ASSERT(thread->IsValid());
  thread->Run();
  return NULL;
}


void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}


void Thread::Start() {
  pthread_create(&thread_handle_data()->thread_, NULL, ThreadEntry, this);
  ASSERT(IsValid());
}


void Thread::Join() {
  pthread_join(thread_handle_data()->thread_, NULL);
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


#ifdef ENABLE_LOGGING_AND_PROFILING

static Sampler* active_sampler_ = NULL;
static int vm_tid_ = 0;


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
  return syscall(SYS_gettid);
}


static void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
#ifndef V8_HOST_ARCH_MIPS
  USE(info);
  if (signal != SIGPROF) return;
  if (active_sampler_ == NULL || !active_sampler_->IsActive()) return;
  if (vm_tid_ != GetThreadID()) return;

  TickSample sample_obj;
  TickSample* sample = CpuProfiler::TickSampleEvent();
  if (sample == NULL) sample = &sample_obj;

  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  mcontext_t& mcontext = ucontext->uc_mcontext;
  sample->state = Top::current_vm_state();
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
  // Implement this on MIPS.
  UNIMPLEMENTED();
#endif
  active_sampler_->SampleStack(sample);
  active_sampler_->Tick(sample);
#endif
}


class Sampler::PlatformData : public Malloced {
 public:
  enum SleepInterval {
    FULL_INTERVAL,
    HALF_INTERVAL
  };

  explicit PlatformData(Sampler* sampler)
      : sampler_(sampler),
        signal_handler_installed_(false),
        vm_tgid_(getpid()),
        signal_sender_launched_(false) {
  }

  void SignalSender() {
    while (sampler_->IsActive()) {
      if (rate_limiter_.SuspendIfNecessary()) continue;
      if (sampler_->IsProfiling() && RuntimeProfiler::IsEnabled()) {
        SendProfilingSignal();
        Sleep(HALF_INTERVAL);
        RuntimeProfiler::NotifyTick();
        Sleep(HALF_INTERVAL);
      } else {
        if (sampler_->IsProfiling()) SendProfilingSignal();
        if (RuntimeProfiler::IsEnabled()) RuntimeProfiler::NotifyTick();
        Sleep(FULL_INTERVAL);
      }
    }
  }

  void SendProfilingSignal() {
    // Glibc doesn't provide a wrapper for tgkill(2).
    syscall(SYS_tgkill, vm_tgid_, vm_tid_, SIGPROF);
  }

  void Sleep(SleepInterval full_or_half) {
    // Convert ms to us and subtract 100 us to compensate delays
    // occuring during signal delivery.
    useconds_t interval = sampler_->interval_ * 1000 - 100;
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

  Sampler* sampler_;
  bool signal_handler_installed_;
  struct sigaction old_signal_handler_;
  int vm_tgid_;
  bool signal_sender_launched_;
  pthread_t signal_sender_thread_;
  RuntimeProfilerRateLimiter rate_limiter_;
};


static void* SenderEntry(void* arg) {
  Sampler::PlatformData* data =
      reinterpret_cast<Sampler::PlatformData*>(arg);
  data->SignalSender();
  return 0;
}


Sampler::Sampler(int interval)
    : interval_(interval),
      profiling_(false),
      active_(false),
      samples_taken_(0) {
  data_ = new PlatformData(this);
}


Sampler::~Sampler() {
  ASSERT(!data_->signal_sender_launched_);
  delete data_;
}


void Sampler::Start() {
  // There can only be one active sampler at the time on POSIX
  // platforms.
  ASSERT(!IsActive());
  vm_tid_ = GetThreadID();

  // Request profiling signals.
  struct sigaction sa;
  sa.sa_sigaction = ProfilerSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGPROF, &sa, &data_->old_signal_handler_) != 0) return;
  data_->signal_handler_installed_ = true;

  // Start a thread that sends SIGPROF signal to VM thread.
  // Sending the signal ourselves instead of relying on itimer provides
  // much better accuracy.
  SetActive(true);
  if (pthread_create(
          &data_->signal_sender_thread_, NULL, SenderEntry, data_) == 0) {
    data_->signal_sender_launched_ = true;
  }

  // Set this sampler as the active sampler.
  active_sampler_ = this;
}


void Sampler::Stop() {
  SetActive(false);

  // Wait for signal sender termination (it will exit after setting
  // active_ to false).
  if (data_->signal_sender_launched_) {
    Top::WakeUpRuntimeProfilerThreadBeforeShutdown();
    pthread_join(data_->signal_sender_thread_, NULL);
    data_->signal_sender_launched_ = false;
  }

  // Restore old signal handler
  if (data_->signal_handler_installed_) {
    sigaction(SIGPROF, &data_->old_signal_handler_, 0);
    data_->signal_handler_installed_ = false;
  }

  // This sampler is no longer the active sampler.
  active_sampler_ = NULL;
}


#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal
