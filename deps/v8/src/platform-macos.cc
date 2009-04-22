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

// Platform specific code for MacOS goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <ucontext.h>
#include <unistd.h>
#include <sys/mman.h>
#include <mach/mach_init.h>

#include <AvailabilityMacros.h>

#ifdef MAC_OS_X_VERSION_10_5
# include <execinfo.h>  // backtrace, backtrace_symbols
#endif

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>

#include <errno.h>

#undef MAP_TYPE

#include "v8.h"

#include "platform.h"

namespace v8 { namespace internal {

// 0 is never a valid thread id on MacOSX since a ptread_t is
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


void OS::Setup() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly will cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srandom(static_cast<unsigned int>(seed));
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
  return getpagesize();
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
  munmap(address, size);
}


#ifdef ENABLE_HEAP_PROTECTION

void OS::Protect(void* address, size_t size) {
  UNIMPLEMENTED();
}


void OS::Unprotect(void* address, size_t size, bool is_executable) {
  UNIMPLEMENTED();
}

#endif


void OS::Sleep(int milliseconds) {
  usleep(1000 * milliseconds);
}


void OS::Abort() {
  // Redirect to std abort to signal abnormal program termination
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
  fwrite(initial, size, 1, file);
  void* memory =
      mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0);
  return new PosixMemoryMappedFile(file, memory, size);
}


PosixMemoryMappedFile::~PosixMemoryMappedFile() {
  if (memory_) munmap(memory_, size_);
  fclose(file_);
}


void OS::LogSharedLibraryAddresses() {
  // TODO(1233579): Implement.
}


double OS::nan_value() {
  return NAN;
}


int OS::ActivationFrameAlignment() {
  // OS X activation frames must be 16 byte-aligned; see "Mac OS X ABI
  // Function Call Guide".
  return 16;
}


int OS::StackWalk(StackFrame* frames, int frames_size) {
#ifndef MAC_OS_X_VERSION_10_5
  return 0;
#else
  void** addresses = NewArray<void*>(frames_size);
  int frames_count = backtrace(addresses, frames_size);

  char** symbols;
  symbols = backtrace_symbols(addresses, frames_count);
  if (symbols == NULL) {
    DeleteArray(addresses);
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

  DeleteArray(addresses);
  free(symbols);

  return frames_count;
#endif
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


bool VirtualMemory::Commit(void* address, size_t size, bool is_executable) {
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
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
              MAP_PRIVATE | MAP_ANON | MAP_NORESERVE,
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


class MacOSMutex : public Mutex {
 public:

  MacOSMutex() {
    // For some reason the compiler doesn't allow you to write
    // "this->mutex_ = PTHREAD_..." directly on mac.
    pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&m, &attr);
    mutex_ = m;
  }

  ~MacOSMutex() { pthread_mutex_destroy(&mutex_); }

  int Lock() { return pthread_mutex_lock(&mutex_); }

  int Unlock() { return pthread_mutex_unlock(&mutex_); }

 private:
  pthread_mutex_t mutex_;
};


Mutex* OS::CreateMutex() {
  return new MacOSMutex();
}


class MacOSSemaphore : public Semaphore {
 public:
  explicit MacOSSemaphore(int count) {
    semaphore_create(mach_task_self(), &semaphore_, SYNC_POLICY_FIFO, count);
  }

  ~MacOSSemaphore() {
    semaphore_destroy(mach_task_self(), semaphore_);
  }

  // The MacOS mach semaphore documentation claims it does not have spurious
  // wakeups, the way pthreads semaphores do.  So the code from the linux
  // platform is not needed here.
  void Wait() { semaphore_wait(semaphore_); }

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


#ifdef ENABLE_LOGGING_AND_PROFILING

static Sampler* active_sampler_ = NULL;

static void ProfilerSignalHandler(int signal, siginfo_t* info, void* context) {
  USE(info);
  if (signal != SIGPROF) return;
  if (active_sampler_ == NULL) return;

  TickSample sample;

  // If profiling, we extract the current pc and sp.
  if (active_sampler_->IsProfiling()) {
    // Extracting the sample from the context is extremely machine dependent.
    ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
    mcontext_t& mcontext = ucontext->uc_mcontext;
#if __DARWIN_UNIX03
    sample.pc = mcontext->__ss.__eip;
    sample.sp = mcontext->__ss.__esp;
    sample.fp = mcontext->__ss.__ebp;
#else  // !__DARWIN_UNIX03
    sample.pc = mcontext->ss.eip;
    sample.sp = mcontext->ss.esp;
    sample.fp = mcontext->ss.ebp;
#endif  // __DARWIN_UNIX03
  }

  // We always sample the VM state.
  sample.state = Logger::state();

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
