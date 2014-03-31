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

// Platform-specific code for POSIX goes here. This is not a platform on its
// own, but contains the parts which are the same across the POSIX platforms
// Linux, MacOS, FreeBSD, OpenBSD, NetBSD and QNX.

#include <dlfcn.h>
#include <pthread.h>
#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>  // for pthread_set_name_np
#endif
#include <sched.h>  // for sched_yield
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/prctl.h>  // for prctl
#endif
#if defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>  // for sysctl
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#undef MAP_TYPE

#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
#define LOG_TAG "v8"
#include <android/log.h>
#endif

#include "v8.h"

#include "codegen.h"
#include "isolate-inl.h"
#include "platform.h"

namespace v8 {
namespace internal {

// 0 is never a valid thread id.
static const pthread_t kNoThread = (pthread_t) 0;


uint64_t OS::CpuFeaturesImpliedByPlatform() {
#if V8_OS_MACOSX
  // Mac OS X requires all these to install so we can assume they are present.
  // These constants are defined by the CPUid instructions.
  const uint64_t one = 1;
  return (one << SSE2) | (one << CMOV);
#else
  return 0;  // Nothing special about the other systems.
#endif
}


// Maximum size of the virtual memory.  0 means there is no artificial
// limit.

intptr_t OS::MaxVirtualMemory() {
  struct rlimit limit;
  int result = getrlimit(RLIMIT_DATA, &limit);
  if (result != 0) return 0;
  return limit.rlim_cur;
}


uint64_t OS::TotalPhysicalMemory() {
#if V8_OS_MACOSX
  int mib[2];
  mib[0] = CTL_HW;
  mib[1] = HW_MEMSIZE;
  int64_t size = 0;
  size_t len = sizeof(size);
  if (sysctl(mib, 2, &size, &len, NULL, 0) != 0) {
    UNREACHABLE();
    return 0;
  }
  return static_cast<uint64_t>(size);
#elif V8_OS_FREEBSD
  int pages, page_size;
  size_t size = sizeof(pages);
  sysctlbyname("vm.stats.vm.v_page_count", &pages, &size, NULL, 0);
  sysctlbyname("vm.stats.vm.v_page_size", &page_size, &size, NULL, 0);
  if (pages == -1 || page_size == -1) {
    UNREACHABLE();
    return 0;
  }
  return static_cast<uint64_t>(pages) * page_size;
#elif V8_OS_CYGWIN
  MEMORYSTATUS memory_info;
  memory_info.dwLength = sizeof(memory_info);
  if (!GlobalMemoryStatus(&memory_info)) {
    UNREACHABLE();
    return 0;
  }
  return static_cast<uint64_t>(memory_info.dwTotalPhys);
#elif V8_OS_QNX
  struct stat stat_buf;
  if (stat("/proc", &stat_buf) != 0) {
    UNREACHABLE();
    return 0;
  }
  return static_cast<uint64_t>(stat_buf.st_size);
#else
  intptr_t pages = sysconf(_SC_PHYS_PAGES);
  intptr_t page_size = sysconf(_SC_PAGESIZE);
  if (pages == -1 || page_size == -1) {
    UNREACHABLE();
    return 0;
  }
  return static_cast<uint64_t>(pages) * page_size;
#endif
}


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
  ASSERT(result == 0);
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


void* OS::GetRandomMmapAddr() {
#if V8_OS_NACL
  // TODO(bradchen): restore randomization once Native Client gets
  // smarter about using mmap address hints.
  // See http://code.google.com/p/nativeclient/issues/3341
  return NULL;
#endif
  Isolate* isolate = Isolate::UncheckedCurrent();
  // Note that the current isolate isn't set up in a call path via
  // CpuFeatures::Probe. We don't care about randomization in this case because
  // the code page is immediately freed.
  if (isolate != NULL) {
    uintptr_t raw_addr;
    isolate->random_number_generator()->NextBytes(&raw_addr, sizeof(raw_addr));
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
  return NULL;
}


size_t OS::AllocateAlignment() {
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}


void OS::Sleep(int milliseconds) {
  useconds_t ms = static_cast<useconds_t>(milliseconds);
  usleep(1000 * ms);
}


void OS::Abort() {
  if (FLAG_hard_abort) {
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
#elif V8_HOST_ARCH_IA32
#if defined(__native_client__)
  asm("hlt");
#else
  asm("int $3");
#endif  // __native_client__
#elif V8_HOST_ARCH_X64
  asm("int $3");
#else
#error Unsupported host architecture.
#endif
}


// ----------------------------------------------------------------------------
// Math functions

double modulo(double x, double y) {
  return std::fmod(x, y);
}


#define UNARY_MATH_FUNCTION(name, generator)             \
static UnaryMathFunction fast_##name##_function = NULL;  \
void init_fast_##name##_function() {                     \
  fast_##name##_function = generator;                    \
}                                                        \
double fast_##name(double x) {                           \
  return (*fast_##name##_function)(x);                   \
}

UNARY_MATH_FUNCTION(exp, CreateExpFunction())
UNARY_MATH_FUNCTION(sqrt, CreateSqrtFunction())

#undef UNARY_MATH_FUNCTION


void lazily_initialize_fast_exp() {
  if (fast_exp_function == NULL) {
    init_fast_exp_function();
  }
}


double OS::nan_value() {
  // NAN from math.h is defined in C99 and not in POSIX.
  return NAN;
}


int OS::GetCurrentProcessId() {
  return static_cast<int>(getpid());
}


// ----------------------------------------------------------------------------
// POSIX date/time support.
//

int OS::GetUserTime(uint32_t* secs,  uint32_t* usecs) {
  struct rusage usage;

  if (getrusage(RUSAGE_SELF, &usage) < 0) return -1;
  *secs = usage.ru_utime.tv_sec;
  *usecs = usage.ru_utime.tv_usec;
  return 0;
}


double OS::TimeCurrentMillis() {
  return Time::Now().ToJsTime();
}


class TimezoneCache {};


TimezoneCache* OS::CreateTimezoneCache() {
  return NULL;
}


void OS::DisposeTimezoneCache(TimezoneCache* cache) {
  ASSERT(cache == NULL);
}


void OS::ClearTimezoneCache(TimezoneCache* cache) {
  ASSERT(cache == NULL);
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


int OS::SNPrintF(Vector<char> str, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = VSNPrintF(str, format, args);
  va_end(args);
  return result;
}


int OS::VSNPrintF(Vector<char> str,
                  const char* format,
                  va_list args) {
  int n = vsnprintf(str.start(), str.length(), format, args);
  if (n < 0 || n >= str.length()) {
    // If the length is zero, the assignment fails.
    if (str.length() > 0)
      str[str.length() - 1] = '\0';
    return -1;
  } else {
    return n;
  }
}


#if V8_TARGET_ARCH_IA32
static void MemMoveWrapper(void* dest, const void* src, size_t size) {
  memmove(dest, src, size);
}


// Initialize to library version so we can call this at any time during startup.
static OS::MemMoveFunction memmove_function = &MemMoveWrapper;

// Defined in codegen-ia32.cc.
OS::MemMoveFunction CreateMemMoveFunction();

// Copy memory area. No restrictions.
void OS::MemMove(void* dest, const void* src, size_t size) {
  if (size == 0) return;
  // Note: here we rely on dependent reads being ordered. This is true
  // on all architectures we currently support.
  (*memmove_function)(dest, src, size);
}

#elif defined(V8_HOST_ARCH_ARM)
void OS::MemCopyUint16Uint8Wrapper(uint16_t* dest,
                                   const uint8_t* src,
                                   size_t chars) {
  uint16_t *limit = dest + chars;
  while (dest < limit) {
    *dest++ = static_cast<uint16_t>(*src++);
  }
}


OS::MemCopyUint8Function OS::memcopy_uint8_function = &OS::MemCopyUint8Wrapper;
OS::MemCopyUint16Uint8Function OS::memcopy_uint16_uint8_function =
    &OS::MemCopyUint16Uint8Wrapper;
// Defined in codegen-arm.cc.
OS::MemCopyUint8Function CreateMemCopyUint8Function(
    OS::MemCopyUint8Function stub);
OS::MemCopyUint16Uint8Function CreateMemCopyUint16Uint8Function(
    OS::MemCopyUint16Uint8Function stub);

#elif defined(V8_HOST_ARCH_MIPS)
OS::MemCopyUint8Function OS::memcopy_uint8_function = &OS::MemCopyUint8Wrapper;
// Defined in codegen-mips.cc.
OS::MemCopyUint8Function CreateMemCopyUint8Function(
    OS::MemCopyUint8Function stub);
#endif


void OS::PostSetUp() {
#if V8_TARGET_ARCH_IA32
  OS::MemMoveFunction generated_memmove = CreateMemMoveFunction();
  if (generated_memmove != NULL) {
    memmove_function = generated_memmove;
  }
#elif defined(V8_HOST_ARCH_ARM)
  OS::memcopy_uint8_function =
      CreateMemCopyUint8Function(&OS::MemCopyUint8Wrapper);
  OS::memcopy_uint16_uint8_function =
      CreateMemCopyUint16Uint8Function(&OS::MemCopyUint16Uint8Wrapper);
#elif defined(V8_HOST_ARCH_MIPS)
  OS::memcopy_uint8_function =
      CreateMemCopyUint8Function(&OS::MemCopyUint8Wrapper);
#endif
  // fast_exp is initialized lazily.
  init_fast_sqrt_function();
}


// ----------------------------------------------------------------------------
// POSIX string support.
//

char* OS::StrChr(char* str, int c) {
  return strchr(str, c);
}


void OS::StrNCpy(Vector<char> dest, const char* src, size_t n) {
  strncpy(dest.start(), src, n);
}


// ----------------------------------------------------------------------------
// POSIX thread support.
//

class Thread::PlatformData : public Malloced {
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
  if (stack_size_ > 0 && stack_size_ < PTHREAD_STACK_MIN) {
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
  ASSERT(thread->data()->thread_ != kNoThread);
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
  ASSERT_EQ(0, result);
  // Native client uses default stack size.
#if !V8_OS_NACL
  if (stack_size_ > 0) {
    result = pthread_attr_setstacksize(&attr, static_cast<size_t>(stack_size_));
    ASSERT_EQ(0, result);
  }
#endif
  {
    LockGuard<Mutex> lock_guard(&data_->thread_creation_mutex_);
    result = pthread_create(&data_->thread_, &attr, ThreadEntry, this);
  }
  ASSERT_EQ(0, result);
  result = pthread_attr_destroy(&attr);
  ASSERT_EQ(0, result);
  ASSERT(data_->thread_ != kNoThread);
  USE(result);
}


void Thread::Join() {
  pthread_join(data_->thread_, NULL);
}


void Thread::YieldCPU() {
  int result = sched_yield();
  ASSERT_EQ(0, result);
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
  ASSERT_EQ(0, result);
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
  ASSERT_EQ(0, result);
  USE(result);
}


void* Thread::GetThreadLocal(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  return pthread_getspecific(pthread_key);
}


void Thread::SetThreadLocal(LocalStorageKey key, void* value) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  int result = pthread_setspecific(pthread_key, value);
  ASSERT_EQ(0, result);
  USE(result);
}


} }  // namespace v8::internal
