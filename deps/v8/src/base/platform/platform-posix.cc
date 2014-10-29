// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for POSIX goes here. This is not a platform on its
// own, but contains the parts which are the same across the POSIX platforms
// Linux, MacOS, FreeBSD, OpenBSD, NetBSD and QNX.

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>  // for pthread_set_name_np
#endif
#include <sched.h>  // for sched_yield
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#if defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>  // NOLINT, for sysctl
#endif

#undef MAP_TYPE

#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
#define LOG_TAG "v8"
#include <android/log.h>  // NOLINT
#endif

#include <cmath>
#include <cstdlib>

#include "src/base/lazy-instance.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/utils/random-number-generator.h"

#ifdef V8_FAST_TLS_SUPPORTED
#include "src/base/atomicops.h"
#endif

#if V8_OS_MACOSX
#include <dlfcn.h>
#endif

#if V8_OS_LINUX
#include <sys/prctl.h>  // NOLINT, for prctl
#endif

#if !V8_OS_NACL
#include <sys/syscall.h>
#endif

namespace v8 {
namespace base {

namespace {

// 0 is never a valid thread id.
const pthread_t kNoThread = (pthread_t) 0;

bool g_hard_abort = false;

const char* g_gc_fake_mmap = NULL;

}  // namespace


int OS::ActivationFrameAlignment() {
#if V8_TARGET_ARCH_ARM
  // On EABI ARM targets this is required for fp correctness in the
  // runtime system.
  return 8;
#elif V8_TARGET_ARCH_MIPS
  return 8;
#else
  // Otherwise we just assume 16 byte alignment, i.e.:
  // - With gcc 4.4 the tree vectorization optimizer can generate code
  //   that requires 16 byte alignment such as movdqa on x86.
  // - Mac OS X and Solaris (64-bit) activation frames must be 16 byte-aligned;
  //   see "Mac OS X ABI Function Call Guide"
  return 16;
#endif
}


intptr_t OS::CommitPageSize() {
  static intptr_t page_size = getpagesize();
  return page_size;
}


void OS::Free(void* address, const size_t size) {
  // TODO(1240712): munmap has a return value which is ignored here.
  int result = munmap(address, size);
  USE(result);
  DCHECK(result == 0);
}


// Get rid of writable permission on code allocations.
void OS::ProtectCode(void* address, const size_t size) {
#if V8_OS_CYGWIN
  DWORD old_protect;
  VirtualProtect(address, size, PAGE_EXECUTE_READ, &old_protect);
#elif V8_OS_NACL
  // The Native Client port of V8 uses an interpreter, so
  // code pages don't need PROT_EXEC.
  mprotect(address, size, PROT_READ);
#else
  mprotect(address, size, PROT_READ | PROT_EXEC);
#endif
}


// Create guard pages.
void OS::Guard(void* address, const size_t size) {
#if V8_OS_CYGWIN
  DWORD oldprotect;
  VirtualProtect(address, size, PAGE_NOACCESS, &oldprotect);
#else
  mprotect(address, size, PROT_NONE);
#endif
}


static LazyInstance<RandomNumberGenerator>::type
    platform_random_number_generator = LAZY_INSTANCE_INITIALIZER;


void OS::Initialize(int64_t random_seed, bool hard_abort,
                    const char* const gc_fake_mmap) {
  if (random_seed) {
    platform_random_number_generator.Pointer()->SetSeed(random_seed);
  }
  g_hard_abort = hard_abort;
  g_gc_fake_mmap = gc_fake_mmap;
}


const char* OS::GetGCFakeMMapFile() {
  return g_gc_fake_mmap;
}


void* OS::GetRandomMmapAddr() {
#if V8_OS_NACL
  // TODO(bradchen): restore randomization once Native Client gets
  // smarter about using mmap address hints.
  // See http://code.google.com/p/nativeclient/issues/3341
  return NULL;
#endif
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
  // Dynamic tools do not support custom mmap addresses.
  return NULL;
#endif
  uintptr_t raw_addr;
  platform_random_number_generator.Pointer()->NextBytes(&raw_addr,
                                                        sizeof(raw_addr));
#if V8_TARGET_ARCH_X64
  // Currently available CPUs have 48 bits of virtual addressing.  Truncate
  // the hint address to 46 bits to give the kernel a fighting chance of
  // fulfilling our placement request.
  raw_addr &= V8_UINT64_C(0x3ffffffff000);
#else
  raw_addr &= 0x3ffff000;

# ifdef __sun
  // For our Solaris/illumos mmap hint, we pick a random address in the bottom
  // half of the top half of the address space (that is, the third quarter).
  // Because we do not MAP_FIXED, this will be treated only as a hint -- the
  // system will not fail to mmap() because something else happens to already
  // be mapped at our random address. We deliberately set the hint high enough
  // to get well above the system's break (that is, the heap); Solaris and
  // illumos will try the hint and if that fails allocate as if there were
  // no hint at all. The high hint prevents the break from getting hemmed in
  // at low values, ceding half of the address space to the system heap.
  raw_addr += 0x80000000;
# else
  // The range 0x20000000 - 0x60000000 is relatively unpopulated across a
  // variety of ASLR modes (PAE kernel, NX compat mode, etc) and on macos
  // 10.6 and 10.7.
  raw_addr += 0x20000000;
# endif
#endif
  return reinterpret_cast<void*>(raw_addr);
}


size_t OS::AllocateAlignment() {
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}


void OS::Sleep(int milliseconds) {
  useconds_t ms = static_cast<useconds_t>(milliseconds);
  usleep(1000 * ms);
}


void OS::Abort() {
  if (g_hard_abort) {
    V8_IMMEDIATE_CRASH();
  }
  // Redirect to std abort to signal abnormal program termination.
  abort();
}


void OS::DebugBreak() {
#if V8_HOST_ARCH_ARM
  asm("bkpt 0");
#elif V8_HOST_ARCH_ARM64
  asm("brk 0");
#elif V8_HOST_ARCH_MIPS
  asm("break");
#elif V8_HOST_ARCH_MIPS64
  asm("break");
#elif V8_HOST_ARCH_IA32
#if V8_OS_NACL
  asm("hlt");
#else
  asm("int $3");
#endif  // V8_OS_NACL
#elif V8_HOST_ARCH_X64
  asm("int $3");
#else
#error Unsupported host architecture.
#endif
}


// ----------------------------------------------------------------------------
// Math functions

double OS::nan_value() {
  // NAN from math.h is defined in C99 and not in POSIX.
  return NAN;
}


int OS::GetCurrentProcessId() {
  return static_cast<int>(getpid());
}


int OS::GetCurrentThreadId() {
#if defined(ANDROID)
  return static_cast<int>(syscall(__NR_gettid));
#elif defined(SYS_gettid)
  return static_cast<int>(syscall(SYS_gettid));
#else
  // PNaCL doesn't have a way to get an integral thread ID, but it doesn't
  // really matter, because we only need it in PerfJitLogger::LogRecordedBuffer.
  return 0;
#endif
}


// ----------------------------------------------------------------------------
// POSIX date/time support.
//

int OS::GetUserTime(uint32_t* secs,  uint32_t* usecs) {
#if V8_OS_NACL
  // Optionally used in Logger::ResourceEvent.
  return -1;
#else
  struct rusage usage;

  if (getrusage(RUSAGE_SELF, &usage) < 0) return -1;
  *secs = usage.ru_utime.tv_sec;
  *usecs = usage.ru_utime.tv_usec;
  return 0;
#endif
}


double OS::TimeCurrentMillis() {
  return Time::Now().ToJsTime();
}


class TimezoneCache {};


TimezoneCache* OS::CreateTimezoneCache() {
  return NULL;
}


void OS::DisposeTimezoneCache(TimezoneCache* cache) {
  DCHECK(cache == NULL);
}


void OS::ClearTimezoneCache(TimezoneCache* cache) {
  DCHECK(cache == NULL);
}


double OS::DaylightSavingsOffset(double time, TimezoneCache*) {
  if (std::isnan(time)) return nan_value();
  time_t tv = static_cast<time_t>(std::floor(time/msPerSecond));
  struct tm* t = localtime(&tv);
  if (NULL == t) return nan_value();
  return t->tm_isdst > 0 ? 3600 * msPerSecond : 0;
}


int OS::GetLastError() {
  return errno;
}


// ----------------------------------------------------------------------------
// POSIX stdio support.
//

FILE* OS::FOpen(const char* path, const char* mode) {
  FILE* file = fopen(path, mode);
  if (file == NULL) return NULL;
  struct stat file_stat;
  if (fstat(fileno(file), &file_stat) != 0) return NULL;
  bool is_regular_file = ((file_stat.st_mode & S_IFREG) != 0);
  if (is_regular_file) return file;
  fclose(file);
  return NULL;
}


bool OS::Remove(const char* path) {
  return (remove(path) == 0);
}


FILE* OS::OpenTemporaryFile() {
  return tmpfile();
}


const char* const OS::LogFileOpenMode = "w";


void OS::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrint(format, args);
  va_end(args);
}


void OS::VPrint(const char* format, va_list args) {
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, args);
#else
  vprintf(format, args);
#endif
}


void OS::FPrint(FILE* out, const char* format, ...) {
  va_list args;
  va_start(args, format);
  VFPrint(out, format, args);
  va_end(args);
}


void OS::VFPrint(FILE* out, const char* format, va_list args) {
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_INFO, LOG_TAG, format, args);
#else
  vfprintf(out, format, args);
#endif
}


void OS::PrintError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VPrintError(format, args);
  va_end(args);
}


void OS::VPrintError(const char* format, va_list args) {
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, format, args);
#else
  vfprintf(stderr, format, args);
#endif
}


int OS::SNPrintF(char* str, int length, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, length, format, args);
  va_end(args);
  return result;
}


int OS::VSNPrintF(char* str,
                  int length,
                  const char* format,
                  va_list args) {
  int n = vsnprintf(str, length, format, args);
  if (n < 0 || n >= length) {
    // If the length is zero, the assignment fails.
    if (length > 0)
      str[length - 1] = '\0';
    return -1;
  } else {
    return n;
  }
}


// ----------------------------------------------------------------------------
// POSIX string support.
//

char* OS::StrChr(char* str, int c) {
  return strchr(str, c);
}


void OS::StrNCpy(char* dest, int length, const char* src, size_t n) {
  strncpy(dest, src, n);
}


// ----------------------------------------------------------------------------
// POSIX thread support.
//

class Thread::PlatformData {
 public:
  PlatformData() : thread_(kNoThread) {}
  pthread_t thread_;  // Thread handle for pthread.
  // Synchronizes thread creation
  Mutex thread_creation_mutex_;
};

Thread::Thread(const Options& options)
    : data_(new PlatformData),
      stack_size_(options.stack_size()),
      start_semaphore_(NULL) {
  if (stack_size_ > 0 && static_cast<size_t>(stack_size_) < PTHREAD_STACK_MIN) {
    stack_size_ = PTHREAD_STACK_MIN;
  }
  set_name(options.name());
}


Thread::~Thread() {
  delete data_;
}


static void SetThreadName(const char* name) {
#if V8_OS_DRAGONFLYBSD || V8_OS_FREEBSD || V8_OS_OPENBSD
  pthread_set_name_np(pthread_self(), name);
#elif V8_OS_NETBSD
  STATIC_ASSERT(Thread::kMaxThreadNameLength <= PTHREAD_MAX_NAMELEN_NP);
  pthread_setname_np(pthread_self(), "%s", name);
#elif V8_OS_MACOSX
  // pthread_setname_np is only available in 10.6 or later, so test
  // for it at runtime.
  int (*dynamic_pthread_setname_np)(const char*);
  *reinterpret_cast<void**>(&dynamic_pthread_setname_np) =
    dlsym(RTLD_DEFAULT, "pthread_setname_np");
  if (dynamic_pthread_setname_np == NULL)
    return;

  // Mac OS X does not expose the length limit of the name, so hardcode it.
  static const int kMaxNameLength = 63;
  STATIC_ASSERT(Thread::kMaxThreadNameLength <= kMaxNameLength);
  dynamic_pthread_setname_np(name);
#elif defined(PR_SET_NAME)
  prctl(PR_SET_NAME,
        reinterpret_cast<unsigned long>(name),  // NOLINT
        0, 0, 0);
#endif
}


static void* ThreadEntry(void* arg) {
  Thread* thread = reinterpret_cast<Thread*>(arg);
  // We take the lock here to make sure that pthread_create finished first since
  // we don't know which thread will run first (the original thread or the new
  // one).
  { LockGuard<Mutex> lock_guard(&thread->data()->thread_creation_mutex_); }
  SetThreadName(thread->name());
  DCHECK(thread->data()->thread_ != kNoThread);
  thread->NotifyStartedAndRun();
  return NULL;
}


void Thread::set_name(const char* name) {
  strncpy(name_, name, sizeof(name_));
  name_[sizeof(name_) - 1] = '\0';
}


void Thread::Start() {
  int result;
  pthread_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  result = pthread_attr_init(&attr);
  DCHECK_EQ(0, result);
  // Native client uses default stack size.
#if !V8_OS_NACL
  if (stack_size_ > 0) {
    result = pthread_attr_setstacksize(&attr, static_cast<size_t>(stack_size_));
    DCHECK_EQ(0, result);
  }
#endif
  {
    LockGuard<Mutex> lock_guard(&data_->thread_creation_mutex_);
    result = pthread_create(&data_->thread_, &attr, ThreadEntry, this);
  }
  DCHECK_EQ(0, result);
  result = pthread_attr_destroy(&attr);
  DCHECK_EQ(0, result);
  DCHECK(data_->thread_ != kNoThread);
  USE(result);
}


void Thread::Join() {
  pthread_join(data_->thread_, NULL);
}


void Thread::YieldCPU() {
  int result = sched_yield();
  DCHECK_EQ(0, result);
  USE(result);
}


static Thread::LocalStorageKey PthreadKeyToLocalKey(pthread_key_t pthread_key) {
#if V8_OS_CYGWIN
  // We need to cast pthread_key_t to Thread::LocalStorageKey in two steps
  // because pthread_key_t is a pointer type on Cygwin. This will probably not
  // work on 64-bit platforms, but Cygwin doesn't support 64-bit anyway.
  STATIC_ASSERT(sizeof(Thread::LocalStorageKey) == sizeof(pthread_key_t));
  intptr_t ptr_key = reinterpret_cast<intptr_t>(pthread_key);
  return static_cast<Thread::LocalStorageKey>(ptr_key);
#else
  return static_cast<Thread::LocalStorageKey>(pthread_key);
#endif
}


static pthread_key_t LocalKeyToPthreadKey(Thread::LocalStorageKey local_key) {
#if V8_OS_CYGWIN
  STATIC_ASSERT(sizeof(Thread::LocalStorageKey) == sizeof(pthread_key_t));
  intptr_t ptr_key = static_cast<intptr_t>(local_key);
  return reinterpret_cast<pthread_key_t>(ptr_key);
#else
  return static_cast<pthread_key_t>(local_key);
#endif
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
#if V8_HOST_ARCH_IA32
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
  DCHECK_EQ(0, result);
  USE(result);
  LocalStorageKey local_key = PthreadKeyToLocalKey(key);
#ifdef V8_FAST_TLS_SUPPORTED
  // If we just initialized fast TLS support, make sure it works.
  if (check_fast_tls) CheckFastTls(local_key);
#endif
  return local_key;
}


void Thread::DeleteThreadLocalKey(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  int result = pthread_key_delete(pthread_key);
  DCHECK_EQ(0, result);
  USE(result);
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  return pthread_getspecific(pthread_key);
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  int result = pthread_setspecific(pthread_key, value);
  DCHECK_EQ(0, result);
  USE(result);
}


} }  // namespace v8::base
