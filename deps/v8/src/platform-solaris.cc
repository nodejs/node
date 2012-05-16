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

// Platform specific code for Solaris 10 goes here. For the POSIX comaptible
// parts the implementation is in platform-posix.cc.

#ifdef __sparc
# error "V8 does not support the SPARC CPU architecture."
#endif

#include <sys/stack.h>  // for stack alignment
#include <unistd.h>  // getpagesize(), usleep()
#include <sys/mman.h>  // mmap()
#include <ucontext.h>  // walkstack(), getcontext()
#include <dlfcn.h>     // dladdr
#include <pthread.h>
#include <sched.h>  // for sched_yield
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>  // gettimeofday(), timeradd()
#include <errno.h>
#include <ieeefp.h>  // finite()
#include <signal.h>  // sigemptyset(), etc
#include <sys/regset.h>


#undef MAP_TYPE

#include "v8.h"

#include "platform-posix.h"
#include "platform.h"
#include "v8threads.h"
#include "vm-state-inl.h"


// It seems there is a bug in some Solaris distributions (experienced in
// SunOS 5.10 Generic_141445-09) which make it difficult or impossible to
// access signbit() despite the availability of other C99 math functions.
#ifndef signbit
// Test sign - usually defined in math.h
int signbit(double x) {
  // We need to take care of the special case of both positive and negative
  // versions of zero.
  if (x == 0) {
    return fpclass(x) & FP_NZERO;
  } else {
    // This won't detect negative NaN but that should be okay since we don't
    // assume that behavior.
    return x < 0;
  }
}
#endif  // signbit

namespace v8 {
namespace internal {


// 0 is never a valid thread id on Solaris since the main thread is 1 and
// subsequent have their ids incremented from there
static const pthread_t kNoThread = (pthread_t) 0;


double ceiling(double x) {
  return ceil(x);
}


static Mutex* limit_mutex = NULL;


void OS::PostSetUp() {
  POSIXPostSetUp();
}


uint64_t OS::CpuFeaturesImpliedByPlatform() {
  return 0;  // Solaris runs on a lot of things.
}


int OS::ActivationFrameAlignment() {
  // GCC generates code that requires 16 byte alignment such as movdqa.
  return Max(STACK_ALIGN, 16);
}


void OS::ReleaseStore(volatile AtomicWord* ptr, AtomicWord value) {
  __asm__ __volatile__("" : : : "memory");
  *ptr = value;
}


const char* OS::LocalTimezone(double time) {
  if (isnan(time)) return "";
  time_t tv = static_cast<time_t>(floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  return tzname[0];  // The location of the timezone string on Solaris.
}


double OS::LocalTimeOffset() {
  // On Solaris, struct tm does not contain a tm_gmtoff field.
  time_t utc = time(NULL);
  ASSERT(utc != -1);
  struct tm* loc = localtime(&utc);
  ASSERT(loc != NULL);
  return static_cast<double>((mktime(loc) - utc) * msPerSecond);
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
  return static_cast<size_t>(getpagesize());
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, getpagesize());
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(NULL, msize, prot, MAP_PRIVATE | MAP_ANON, -1, 0);

  if (mbase == MAP_FAILED) {
    LOG(ISOLATE, StringEvent("OS::Allocate", "mmap failed"));
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
  useconds_t ms = static_cast<useconds_t>(milliseconds);
  usleep(1000 * ms);
}


void OS::Abort() {
  // Redirect to std abort to signal abnormal program termination.
  abort();
}


void OS::DebugBreak() {
  asm("int $3");
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
}


void OS::SignalCodeMovingGC() {
}


struct StackWalker {
  Vector<OS::StackFrame>& frames;
  int index;
};


static int StackWalkCallback(uintptr_t pc, int signo, void* data) {
  struct StackWalker* walker = static_cast<struct StackWalker*>(data);
  Dl_info info;

  int i = walker->index;

  walker->frames[i].address = reinterpret_cast<void*>(pc);

  // Make sure line termination is in place.
  walker->frames[i].text[OS::kStackWalkMaxTextLen - 1] = '\0';

  Vector<char> text = MutableCStrVector(walker->frames[i].text,
                                        OS::kStackWalkMaxTextLen);

  if (dladdr(reinterpret_cast<void*>(pc), &info) == 0) {
    OS::SNPrintF(text, "[0x%p]", pc);
  } else if ((info.dli_fname != NULL && info.dli_sname != NULL)) {
    // We have symbol info.
    OS::SNPrintF(text, "%s'%s+0x%x", info.dli_fname, info.dli_sname, pc);
  } else {
    // No local symbol info.
    OS::SNPrintF(text,
                 "%s'0x%p [0x%p]",
                 info.dli_fname,
                 pc - reinterpret_cast<uintptr_t>(info.dli_fbase),
                 pc);
  }
  walker->index++;
  return 0;
}


int OS::StackWalk(Vector<OS::StackFrame> frames) {
  ucontext_t ctx;
  struct StackWalker walker = { frames, 0 };

  if (getcontext(&ctx) < 0) return kStackWalkError;

  if (!walkcontext(&ctx, StackWalkCallback, &walker)) {
    return kStackWalkError;
  }

  return walker.index;
}


// Constants used for mmap.
static const int kMmapFd = -1;
static const int kMmapFdOffset = 0;


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }

VirtualMemory::VirtualMemory(size_t size) {
  address_ = ReserveRegion(size);
  size_ = size;
}


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  ASSERT(IsAligned(alignment, static_cast<intptr_t>(OS::AllocateAlignment())));
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* reservation = mmap(OS::GetRandomMmapAddr(),
                           request_size,
                           PROT_NONE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
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
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
                      kMmapFd,
                      kMmapFdOffset);

  if (result == MAP_FAILED) return NULL;

  return result;
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  if (MAP_FAILED == mmap(base,
                         size,
                         prot,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                         kMmapFd,
                         kMmapFdOffset)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(base, size);
  return true;
}


bool VirtualMemory::UncommitRegion(void* base, size_t size) {
  return mmap(base,
              size,
              PROT_NONE,
              MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED,
              kMmapFd,
              kMmapFdOffset) != MAP_FAILED;
}


bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
  return munmap(base, size) == 0;
}


class Thread::PlatformData : public Malloced {
 public:
  PlatformData() : thread_(kNoThread) {  }

  pthread_t thread_;  // Thread handle for pthread.
};


Thread::Thread(const Options& options)
    : data_(new PlatformData()),
      stack_size_(options.stack_size()) {
  set_name(options.name());
}


Thread::~Thread() {
  delete data_;
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // This is also initialized by the first argument to pthread_create() but we
  // don't know which thread will run first (the original thread or the new
  // one) so we initialize it here too.
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
  pthread_attr_t attr;
  if (stack_size_ > 0) {
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, static_cast<size_t>(stack_size_));
  }
  pthread_create(&data_->thread_, NULL, ThreadEntry, this);
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


class SolarisMutex : public Mutex {
 public:
  SolarisMutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex_, &attr);
  }

  ~SolarisMutex() { pthread_mutex_destroy(&mutex_); }

  int Lock() { return pthread_mutex_lock(&mutex_); }

  int Unlock() { return pthread_mutex_unlock(&mutex_); }

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
  return new SolarisMutex();
}


class SolarisSemaphore : public Semaphore {
 public:
  explicit SolarisSemaphore(int count) {  sem_init(&sem_, 0, count); }
  virtual ~SolarisSemaphore() { sem_destroy(&sem_); }

  virtual void Wait();
  virtual bool Wait(int timeout);
  virtual void Signal() { sem_post(&sem_); }
 private:
  sem_t sem_;
};


void SolarisSemaphore::Wait() {
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


#ifndef timeradd
#define timeradd(a, b, result) \
  do { \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
    if ((result)->tv_usec >= 1000000) { \
      ++(result)->tv_sec; \
      (result)->tv_usec -= 1000000; \
    } \
  } while (0)
#endif


bool SolarisSemaphore::Wait(int timeout) {
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
    if (result == -1 && errno == ETIMEDOUT) return false;  // Timeout.
    CHECK(result == -1 && errno == EINTR);  // Signal caused spurious wakeup.
  }
}


Semaphore* OS::CreateSemaphore(int count) {
  return new SolarisSemaphore(count);
}


static pthread_t GetThreadID() {
  return pthread_self();
}

static void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
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

  sample->pc = reinterpret_cast<Address>(mcontext.gregs[REG_PC]);
  sample->sp = reinterpret_cast<Address>(mcontext.gregs[REG_SP]);
  sample->fp = reinterpret_cast<Address>(mcontext.gregs[REG_FP]);

  sampler->SampleStack(sample);
  sampler->Tick(sample);
}

class Sampler::PlatformData : public Malloced {
 public:
  PlatformData() : vm_tid_(GetThreadID()) {}

  pthread_t vm_tid() const { return vm_tid_; }

 private:
  pthread_t vm_tid_;
};


class SignalSender : public Thread {
 public:
  enum SleepInterval {
    HALF_INTERVAL,
    FULL_INTERVAL
  };

  static const int kSignalSenderStackSize = 64 * KB;

  explicit SignalSender(int interval)
      : Thread(Thread::Options("SignalSender", kSignalSenderStackSize)),
        interval_(interval) {}

  static void SetUp() { if (!mutex_) mutex_ = OS::CreateMutex(); }
  static void TearDown() { delete mutex_; }

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

  void SendProfilingSignal(pthread_t tid) {
    if (!signal_handler_installed_) return;
    pthread_kill(tid, SIGPROF);
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

  const int interval_;
  RuntimeProfilerRateLimiter rate_limiter_;

  // Protects the process wide state below.
  static Mutex* mutex_;
  static SignalSender* instance_;
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalSender);
};

Mutex* SignalSender::mutex_ = NULL;
SignalSender* SignalSender::instance_ = NULL;
struct sigaction SignalSender::old_signal_handler_;
bool SignalSender::signal_handler_installed_ = false;


void OS::SetUp() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly will cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srandom(static_cast<unsigned int>(seed));
  limit_mutex = CreateMutex();
  SignalSender::SetUp();
}


void OS::TearDown() {
  SignalSender::TearDown();
  delete limit_mutex;
}


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
