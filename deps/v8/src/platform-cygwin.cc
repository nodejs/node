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

// Platform specific code for Cygwin goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdarg.h>
#include <strings.h>    // index
#include <sys/time.h>
#include <sys/mman.h>   // mmap & munmap
#include <unistd.h>     // sysconf

#undef MAP_TYPE

#include "v8.h"

#include "platform-posix.h"
#include "platform.h"
#include "simulator.h"
#include "v8threads.h"
#include "vm-state-inl.h"
#include "win32-headers.h"

namespace v8 {
namespace internal {

// 0 is never a valid thread id
static const pthread_t kNoThread = (pthread_t) 0;


double ceiling(double x) {
  return ceil(x);
}


static Mutex* limit_mutex = NULL;


void OS::PostSetUp() {
  POSIXPostSetUp();
}

uint64_t OS::CpuFeaturesImpliedByPlatform() {
  return 0;  // Nothing special about Cygwin.
}


int OS::ActivationFrameAlignment() {
  // With gcc 4.4 the tree vectorization optimizer can generate code
  // that requires 16 byte alignment such as movdqa on x86.
  return 16;
}


void OS::ReleaseStore(volatile AtomicWord* ptr, AtomicWord value) {
  __asm__ __volatile__("" : : : "memory");
  // An x86 store acts as a release barrier.
  *ptr = value;
}

const char* OS::LocalTimezone(double time) {
  if (std::isnan(time)) return "";
  time_t tv = static_cast<time_t>(floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return "";
  return tzname[0];  // The location of the timezone string on Cygwin.
}


double OS::LocalTimeOffset() {
  // On Cygwin, struct tm does not contain a tm_gmtoff field.
  time_t utc = time(NULL);
  ASSERT(utc != -1);
  struct tm* loc = localtime(&utc);
  ASSERT(loc != NULL);
  // time - localtime includes any daylight savings offset, so subtract it.
  return static_cast<double>((mktime(loc) - utc) * msPerSecond -
                             (loc->tm_isdst > 0 ? 3600 * msPerSecond : 0));
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
  return sysconf(_SC_PAGESIZE);
}


void* OS::Allocate(const size_t requested,
                   size_t* allocated,
                   bool is_executable) {
  const size_t msize = RoundUp(requested, sysconf(_SC_PAGESIZE));
  int prot = PROT_READ | PROT_WRITE | (is_executable ? PROT_EXEC : 0);
  void* mbase = mmap(NULL, msize, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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


void OS::ProtectCode(void* address, const size_t size) {
  DWORD old_protect;
  VirtualProtect(address, size, PAGE_EXECUTE_READ, &old_protect);
}


void OS::Guard(void* address, const size_t size) {
  DWORD oldprotect;
  VirtualProtect(address, size, PAGE_READONLY | PAGE_GUARD, &oldprotect);
}


void OS::Sleep(int milliseconds) {
  unsigned int ms = static_cast<unsigned int>(milliseconds);
  usleep(1000 * ms);
}


int OS::NumberOfCores() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}


void OS::Abort() {
  // Redirect to std abort to signal abnormal program termination.
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
      // Entry not describing executable data. Skip to end of line to set up
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


void OS::SignalCodeMovingGC() {
  // Nothing to do on Cygwin.
}


int OS::StackWalk(Vector<OS::StackFrame> frames) {
  // Not supported on Cygwin.
  return 0;
}


// The VirtualMemory implementation is taken from platform-win32.cc.
// The mmap-based virtual memory implementation as it is used on most posix
// platforms does not work well because Cygwin does not support MAP_FIXED.
// This causes VirtualMemory::Commit to not always commit the memory region
// specified.

static void* GetRandomAddr() {
  Isolate* isolate = Isolate::UncheckedCurrent();
  // Note that the current isolate isn't set up in a call path via
  // CpuFeatures::Probe. We don't care about randomization in this case because
  // the code page is immediately freed.
  if (isolate != NULL) {
    // The address range used to randomize RWX allocations in OS::Allocate
    // Try not to map pages into the default range that windows loads DLLs
    // Use a multiple of 64k to prevent committing unused memory.
    // Note: This does not guarantee RWX regions will be within the
    // range kAllocationRandomAddressMin to kAllocationRandomAddressMax
#ifdef V8_HOST_ARCH_64_BIT
    static const intptr_t kAllocationRandomAddressMin = 0x0000000080000000;
    static const intptr_t kAllocationRandomAddressMax = 0x000003FFFFFF0000;
#else
    static const intptr_t kAllocationRandomAddressMin = 0x04000000;
    static const intptr_t kAllocationRandomAddressMax = 0x3FFF0000;
#endif
    uintptr_t address = (V8::RandomPrivate(isolate) << kPageSizeBits)
        | kAllocationRandomAddressMin;
    address &= kAllocationRandomAddressMax;
    return reinterpret_cast<void *>(address);
  }
  return NULL;
}


static void* RandomizedVirtualAlloc(size_t size, int action, int protection) {
  LPVOID base = NULL;

  if (protection == PAGE_EXECUTE_READWRITE || protection == PAGE_NOACCESS) {
    // For exectutable pages try and randomize the allocation address
    for (size_t attempts = 0; base == NULL && attempts < 3; ++attempts) {
      base = VirtualAlloc(GetRandomAddr(), size, action, protection);
    }
  }

  // After three attempts give up and let the OS find an address to use.
  if (base == NULL) base = VirtualAlloc(NULL, size, action, protection);

  return base;
}


VirtualMemory::VirtualMemory() : address_(NULL), size_(0) { }


VirtualMemory::VirtualMemory(size_t size)
    : address_(ReserveRegion(size)), size_(size) { }


VirtualMemory::VirtualMemory(size_t size, size_t alignment)
    : address_(NULL), size_(0) {
  ASSERT(IsAligned(alignment, static_cast<intptr_t>(OS::AllocateAlignment())));
  size_t request_size = RoundUp(size + alignment,
                                static_cast<intptr_t>(OS::AllocateAlignment()));
  void* address = ReserveRegion(request_size);
  if (address == NULL) return;
  Address base = RoundUp(static_cast<Address>(address), alignment);
  // Try reducing the size by freeing and then reallocating a specific area.
  bool result = ReleaseRegion(address, request_size);
  USE(result);
  ASSERT(result);
  address = VirtualAlloc(base, size, MEM_RESERVE, PAGE_NOACCESS);
  if (address != NULL) {
    request_size = size;
    ASSERT(base == static_cast<Address>(address));
  } else {
    // Resizing failed, just go with a bigger area.
    address = ReserveRegion(request_size);
    if (address == NULL) return;
  }
  address_ = address;
  size_ = request_size;
}


VirtualMemory::~VirtualMemory() {
  if (IsReserved()) {
    bool result = ReleaseRegion(address_, size_);
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
  ASSERT(IsReserved());
  return UncommitRegion(address, size);
}


void* VirtualMemory::ReserveRegion(size_t size) {
  return RandomizedVirtualAlloc(size, MEM_RESERVE, PAGE_NOACCESS);
}


bool VirtualMemory::CommitRegion(void* base, size_t size, bool is_executable) {
  int prot = is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
  if (NULL == VirtualAlloc(base, size, MEM_COMMIT, prot)) {
    return false;
  }

  UpdateAllocatedSpaceLimits(base, static_cast<int>(size));
  return true;
}


bool VirtualMemory::Guard(void* address) {
  if (NULL == VirtualAlloc(address,
                           OS::CommitPageSize(),
                           MEM_COMMIT,
                           PAGE_READONLY | PAGE_GUARD)) {
    return false;
  }
  return true;
}


bool VirtualMemory::UncommitRegion(void* base, size_t size) {
  return VirtualFree(base, size, MEM_DECOMMIT) != 0;
}


bool VirtualMemory::ReleaseRegion(void* base, size_t size) {
  return VirtualFree(base, 0, MEM_RELEASE) != 0;
}


bool VirtualMemory::HasLazyCommits() {
  // TODO(alph): implement for the platform.
  return false;
}


class Thread::PlatformData : public Malloced {
 public:
  PlatformData() : thread_(kNoThread) {}
  pthread_t thread_;  // Thread handle for pthread.
};


Thread::Thread(const Options& options)
    : data_(new PlatformData()),
      stack_size_(options.stack_size()),
      start_semaphore_(NULL) {
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


static inline Thread::LocalStorageKey PthreadKeyToLocalKey(
    pthread_key_t pthread_key) {
  // We need to cast pthread_key_t to Thread::LocalStorageKey in two steps
  // because pthread_key_t is a pointer type on Cygwin. This will probably not
  // work on 64-bit platforms, but Cygwin doesn't support 64-bit anyway.
  STATIC_ASSERT(sizeof(Thread::LocalStorageKey) == sizeof(pthread_key_t));
  intptr_t ptr_key = reinterpret_cast<intptr_t>(pthread_key);
  return static_cast<Thread::LocalStorageKey>(ptr_key);
}


static inline pthread_key_t LocalKeyToPthreadKey(
    Thread::LocalStorageKey local_key) {
  STATIC_ASSERT(sizeof(Thread::LocalStorageKey) == sizeof(pthread_key_t));
  intptr_t ptr_key = static_cast<intptr_t>(local_key);
  return reinterpret_cast<pthread_key_t>(ptr_key);
}


Thread::LocalStorageKey Thread::CreateThreadLocalKey() {
  pthread_key_t key;
  int result = pthread_key_create(&key, NULL);
  USE(result);
  ASSERT(result == 0);
  return PthreadKeyToLocalKey(key);
}


void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  int result = pthread_key_delete(pthread_key);
  USE(result);
  ASSERT(result == 0);
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  return pthread_getspecific(pthread_key);
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  pthread_setspecific(pthread_key, value);
}


void Thread::YieldCPU() {
  sched_yield();
}


class CygwinMutex : public Mutex {
 public:
  CygwinMutex() {
    pthread_mutexattr_t attrs;
    memset(&attrs, 0, sizeof(attrs));

    int result = pthread_mutexattr_init(&attrs);
    ASSERT(result == 0);
    result = pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE);
    ASSERT(result == 0);
    result = pthread_mutex_init(&mutex_, &attrs);
    ASSERT(result == 0);
  }

  virtual ~CygwinMutex() { pthread_mutex_destroy(&mutex_); }

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
  return new CygwinMutex();
}


class CygwinSemaphore : public Semaphore {
 public:
  explicit CygwinSemaphore(int count) {  sem_init(&sem_, 0, count); }
  virtual ~CygwinSemaphore() { sem_destroy(&sem_); }

  virtual void Wait();
  virtual bool Wait(int timeout);
  virtual void Signal() { sem_post(&sem_); }
 private:
  sem_t sem_;
};


void CygwinSemaphore::Wait() {
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


bool CygwinSemaphore::Wait(int timeout) {
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
  return new CygwinSemaphore(count);
}


void OS::SetUp() {
  // Seed the random number generator.
  // Convert the current time to a 64-bit integer first, before converting it
  // to an unsigned. Going directly can cause an overflow and the seed to be
  // set to all ones. The seed will be identical for different instances that
  // call this setup code within the same millisecond.
  uint64_t seed = static_cast<uint64_t>(TimeCurrentMillis());
  srandom(static_cast<unsigned int>(seed));
  limit_mutex = CreateMutex();
}


void OS::TearDown() {
  delete limit_mutex;
}


} }  // namespace v8::internal
