// Copyright 2006-2009 the V8 project authors. All rights reserved.
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


#undef MAP_TYPE

#include "v8.h"

#include "platform.h"


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


void OS::Setup() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly will cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srandom(static_cast<unsigned int>(seed));
}


uint64_t OS::CpuFeaturesImpliedByPlatform() {
  return 0;  // Solaris runs on a lot of things.
}


int OS::ActivationFrameAlignment() {
  return STACK_ALIGN;
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
  return static_cast<size_t>(getpagesize());
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, getpagesize());
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(NULL, msize, prot, MAP_PRIVATE | MAP_ANON, -1, 0);

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
 private:
  FILE* file_;
  void* memory_;
  int size_;
};


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


VirtualMemory::VirtualMemory(size_t size) {
  address_ = mmap(NULL, size, PROT_NONE,
                  MAP_PRIVATE | MAP_ANON | MAP_NORESERVE,
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


bool VirtualMemory::Commit(void* address, size_t size, bool executable) {
  int prot = PROT_READ | PROT_WRITE | (executable ? PROT_EXEC : 0);
  if (MAP_FAILED == mmap(address, size, prot,
                         MAP_PRIVATE | MAP_ANON | MAP_FIXED,
                         kMmapFd, kMmapFdOffset)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(address, size);
  return true;
}


bool VirtualMemory::Uncommit(void* address, size_t size) {
  return mmap(address, size, PROT_NONE,
              MAP_PRIVATE | MAP_ANON | MAP_NORESERVE | MAP_FIXED,
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
}


Thread::~Thread() {
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // This is also initialized by the first argument to pthread_create() but we
  // don't know which thread will run first (the original thread or the new
  // one) so we initialize it here too.
  thread->thread_handle_data()->thread_ = pthread_self();
  ASSERT(thread->IsValid());
  thread->Run();
  return NULL;
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


#ifdef ENABLE_LOGGING_AND_PROFILING

static Sampler* active_sampler_ = NULL;

static void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
  USE(info);
  if (signal != SIGPROF) return;
  if (active_sampler_ == NULL) return;

  TickSample sample;
  sample.pc = 0;
  sample.sp = 0;
  sample.fp = 0;

  // We always sample the VM state.
  sample.state = VMState::current_state();

  active_sampler_->Tick(&sample);
}


class Sampler::PlatformData : public Malloced {
 public:
  PlatformData() {
    signal_handler_installed_ = false;
  }

  bool signal_handler_installed_;
  struct sigaction old_signal_handler_;
  struct itimerval old_timer_value_;
};


Sampler::Sampler(int interval, bool profiling)
    : interval_(interval), profiling_(profiling), active_(false) {
  data_ = new PlatformData();
}


Sampler::~Sampler() {
  delete data_;
}


void Sampler::Start() {
  // There can only be one active sampler at the time on POSIX
  // platforms.
  if (active_sampler_ != NULL) return;

  // Request profiling signals.
  struct sigaction sa;
  sa.sa_sigaction = ProfilerSignalHandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGPROF, &sa, &data_->old_signal_handler_) != 0) return;
  data_->signal_handler_installed_ = true;

  // Set the itimer to generate a tick for each interval.
  itimerval itimer;
  itimer.it_interval.tv_sec = interval_ / 1000;
  itimer.it_interval.tv_usec = (interval_ % 1000) * 1000;
  itimer.it_value.tv_sec = itimer.it_interval.tv_sec;
  itimer.it_value.tv_usec = itimer.it_interval.tv_usec;
  setitimer(ITIMER_PROF, &itimer, &data_->old_timer_value_);

  // Set this sampler as the active sampler.
  active_sampler_ = this;
  active_ = true;
}


void Sampler::Stop() {
  // Restore old signal handler
  if (data_->signal_handler_installed_) {
    setitimer(ITIMER_PROF, &data_->old_timer_value_, NULL);
    sigaction(SIGPROF, &data_->old_signal_handler_, 0);
    data_->signal_handler_installed_ = false;
  }

  // This sampler is no longer the active sampler.
  active_sampler_ = NULL;
  active_ = false;
}

#endif  // ENABLE_LOGGING_AND_PROFILING

} }  // namespace v8::internal
