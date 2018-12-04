// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libsampler/sampler.h"

#if V8_OS_POSIX && !V8_OS_CYGWIN && !V8_OS_FUCHSIA

#define USE_SIGNALS

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#if !V8_OS_QNX && !V8_OS_AIX
#include <sys/syscall.h>  // NOLINT
#endif

#if V8_OS_MACOSX
#include <mach/mach.h>
// OpenBSD doesn't have <ucontext.h>. ucontext_t lives in <signal.h>
// and is a typedef for struct sigcontext. There is no uc_mcontext.
#elif !V8_OS_OPENBSD
#include <ucontext.h>
#endif

#include <unistd.h>

#elif V8_OS_WIN || V8_OS_CYGWIN

#include "src/base/win32-headers.h"

#elif V8_OS_FUCHSIA

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>
#include <zircon/types.h>

// TODO(wez): Remove this once the Fuchsia SDK has rolled.
#if defined(ZX_THREAD_STATE_REGSET0)
#define ZX_THREAD_STATE_GENERAL_REGS ZX_THREAD_STATE_REGSET0
zx_status_t zx_thread_read_state(zx_handle_t h, uint32_t k, void* b, size_t l) {
  uint32_t dummy_out_len = 0;
  return zx_thread_read_state(h, k, b, static_cast<uint32_t>(l),
                              &dummy_out_len);
}
#if defined(__x86_64__)
typedef zx_x86_64_general_regs_t zx_thread_state_general_regs_t;
#else
typedef zx_arm64_general_regs_t zx_thread_state_general_regs_t;
#endif
#endif  // !defined(ZX_THREAD_STATE_GENERAL_REGS)

#endif

#include <algorithm>
#include <vector>
#include <map>

#include "src/base/atomic-utils.h"
#include "src/base/hashmap.h"
#include "src/base/platform/platform.h"

#if V8_OS_ANDROID && !defined(__BIONIC_HAVE_UCONTEXT_T)

// Not all versions of Android's C library provide ucontext_t.
// Detect this and provide custom but compatible definitions. Note that these
// follow the GLibc naming convention to access register values from
// mcontext_t.
//
// See http://code.google.com/p/android/issues/detail?id=34784

#if defined(__arm__)

typedef struct sigcontext mcontext_t;

typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used by V8, don't define them here.
} ucontext_t;

#elif defined(__aarch64__)

typedef struct sigcontext mcontext_t;

typedef struct ucontext {
  uint64_t uc_flags;
  struct ucontext *uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used by V8, don't define them here.
} ucontext_t;

#elif defined(__mips__)
// MIPS version of sigcontext, for Android bionic.
typedef struct {
  uint32_t regmask;
  uint32_t status;
  uint64_t pc;
  uint64_t gregs[32];
  uint64_t fpregs[32];
  uint32_t acx;
  uint32_t fpc_csr;
  uint32_t fpc_eir;
  uint32_t used_math;
  uint32_t dsp;
  uint64_t mdhi;
  uint64_t mdlo;
  uint32_t hi1;
  uint32_t lo1;
  uint32_t hi2;
  uint32_t lo2;
  uint32_t hi3;
  uint32_t lo3;
} mcontext_t;

typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used by V8, don't define them here.
} ucontext_t;

#elif defined(__i386__)
// x86 version for Android.
typedef struct {
  uint32_t gregs[19];
  void* fpregs;
  uint32_t oldmask;
  uint32_t cr2;
} mcontext_t;

typedef uint32_t kernel_sigset_t[2];  // x86 kernel uses 64-bit signal masks
typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used by V8, don't define them here.
} ucontext_t;
enum { REG_EBP = 6, REG_ESP = 7, REG_EIP = 14 };

#elif defined(__x86_64__)
// x64 version for Android.
typedef struct {
  uint64_t gregs[23];
  void* fpregs;
  uint64_t __reserved1[8];
} mcontext_t;

typedef struct ucontext {
  uint64_t uc_flags;
  struct ucontext *uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used by V8, don't define them here.
} ucontext_t;
enum { REG_RBP = 10, REG_RSP = 15, REG_RIP = 16 };
#endif

#endif  // V8_OS_ANDROID && !defined(__BIONIC_HAVE_UCONTEXT_T)


namespace v8 {
namespace sampler {

namespace {

#if defined(USE_SIGNALS)
typedef std::vector<Sampler*> SamplerList;
typedef SamplerList::iterator SamplerListIterator;
typedef base::AtomicValue<bool> AtomicMutex;

class AtomicGuard {
 public:
  explicit AtomicGuard(AtomicMutex* atomic, bool is_blocking = true)
      : atomic_(atomic), is_success_(false) {
    do {
      // Use Acquire_Load to gain mutual exclusion.
      USE(atomic_->Value());
      is_success_ = atomic_->TrySetValue(false, true);
    } while (is_blocking && !is_success_);
  }

  bool is_success() const { return is_success_; }

  ~AtomicGuard() {
    if (!is_success_) return;
    atomic_->SetValue(false);
  }

 private:
  AtomicMutex* const atomic_;
  bool is_success_;
};

// Returns key for hash map.
void* ThreadKey(pthread_t thread_id) {
  return reinterpret_cast<void*>(thread_id);
}

// Returns hash value for hash map.
uint32_t ThreadHash(pthread_t thread_id) {
#if V8_OS_BSD
  return static_cast<uint32_t>(reinterpret_cast<intptr_t>(thread_id));
#else
  return static_cast<uint32_t>(thread_id);
#endif
}

#endif  // USE_SIGNALS

}  // namespace

#if defined(USE_SIGNALS)

class Sampler::PlatformData {
 public:
  PlatformData() : vm_tid_(pthread_self()) {}
  pthread_t vm_tid() const { return vm_tid_; }

 private:
  pthread_t vm_tid_;
};

class SamplerManager {
 public:
  SamplerManager() : sampler_map_() {}

  void AddSampler(Sampler* sampler) {
    AtomicGuard atomic_guard(&samplers_access_counter_);
    DCHECK(sampler->IsActive() || !sampler->IsRegistered());
    // Add sampler into map if needed.
    pthread_t thread_id = sampler->platform_data()->vm_tid();
    base::HashMap::Entry* entry =
            sampler_map_.LookupOrInsert(ThreadKey(thread_id),
                                        ThreadHash(thread_id));
    DCHECK_NOT_NULL(entry);
    if (entry->value == nullptr) {
      SamplerList* samplers = new SamplerList();
      samplers->push_back(sampler);
      entry->value = samplers;
    } else {
      SamplerList* samplers = reinterpret_cast<SamplerList*>(entry->value);
      bool exists = false;
      for (SamplerListIterator iter = samplers->begin();
           iter != samplers->end(); ++iter) {
        if (*iter == sampler) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        samplers->push_back(sampler);
      }
    }
  }

  void RemoveSampler(Sampler* sampler) {
    AtomicGuard atomic_guard(&samplers_access_counter_);
    DCHECK(sampler->IsActive() || sampler->IsRegistered());
    // Remove sampler from map.
    pthread_t thread_id = sampler->platform_data()->vm_tid();
    void* thread_key = ThreadKey(thread_id);
    uint32_t thread_hash = ThreadHash(thread_id);
    base::HashMap::Entry* entry = sampler_map_.Lookup(thread_key, thread_hash);
    DCHECK_NOT_NULL(entry);
    SamplerList* samplers = reinterpret_cast<SamplerList*>(entry->value);
    for (SamplerListIterator iter = samplers->begin(); iter != samplers->end();
         ++iter) {
      if (*iter == sampler) {
        samplers->erase(iter);
        break;
      }
    }
    if (samplers->empty()) {
      sampler_map_.Remove(thread_key, thread_hash);
      delete samplers;
    }
  }

#if defined(USE_SIGNALS)
  void DoSample(const v8::RegisterState& state) {
    AtomicGuard atomic_guard(&SamplerManager::samplers_access_counter_, false);
    if (!atomic_guard.is_success()) return;
    pthread_t thread_id = pthread_self();
    base::HashMap::Entry* entry =
        sampler_map_.Lookup(ThreadKey(thread_id), ThreadHash(thread_id));
    if (!entry) return;
    SamplerList& samplers = *static_cast<SamplerList*>(entry->value);

    for (size_t i = 0; i < samplers.size(); ++i) {
      Sampler* sampler = samplers[i];
      Isolate* isolate = sampler->isolate();
      // We require a fully initialized and entered isolate.
      if (isolate == nullptr || !isolate->IsInUse()) continue;
      if (v8::Locker::IsActive() && !Locker::IsLocked(isolate)) continue;
      sampler->SampleStack(state);
    }
  }
#endif

  static SamplerManager* instance() { return instance_.Pointer(); }

 private:
  base::HashMap sampler_map_;
  static AtomicMutex samplers_access_counter_;
  static base::LazyInstance<SamplerManager>::type instance_;
};

AtomicMutex SamplerManager::samplers_access_counter_;
base::LazyInstance<SamplerManager>::type SamplerManager::instance_ =
    LAZY_INSTANCE_INITIALIZER;

#elif V8_OS_WIN || V8_OS_CYGWIN

// ----------------------------------------------------------------------------
// Win32 profiler support. On Cygwin we use the same sampler implementation as
// on Win32.

class Sampler::PlatformData {
 public:
  // Get a handle to the calling thread. This is the thread that we are
  // going to profile. We need to make a copy of the handle because we are
  // going to use it in the sampler thread. Using GetThreadHandle() will
  // not work in this case. We're using OpenThread because DuplicateHandle
  // for some reason doesn't work in Chrome's sandbox.
  PlatformData()
      : profiled_thread_(OpenThread(THREAD_GET_CONTEXT |
                                    THREAD_SUSPEND_RESUME |
                                    THREAD_QUERY_INFORMATION,
                                    false,
                                    GetCurrentThreadId())) {}

  ~PlatformData() {
    if (profiled_thread_ != nullptr) {
      CloseHandle(profiled_thread_);
      profiled_thread_ = nullptr;
    }
  }

  HANDLE profiled_thread() { return profiled_thread_; }

 private:
  HANDLE profiled_thread_;
};

#elif V8_OS_FUCHSIA

class Sampler::PlatformData {
 public:
  PlatformData() {
    zx_handle_duplicate(zx_thread_self(), ZX_RIGHT_SAME_RIGHTS,
                        &profiled_thread_);
  }
  ~PlatformData() {
    if (profiled_thread_ != ZX_HANDLE_INVALID) {
      zx_handle_close(profiled_thread_);
      profiled_thread_ = ZX_HANDLE_INVALID;
    }
  }

  zx_handle_t profiled_thread() { return profiled_thread_; }

 private:
  zx_handle_t profiled_thread_ = ZX_HANDLE_INVALID;
};

#endif  // USE_SIGNALS


#if defined(USE_SIGNALS)
class SignalHandler {
 public:
  static void SetUp() { if (!mutex_) mutex_ = new base::Mutex(); }
  static void TearDown() {
    delete mutex_;
    mutex_ = nullptr;
  }

  static void IncreaseSamplerCount() {
    base::LockGuard<base::Mutex> lock_guard(mutex_);
    if (++client_count_ == 1) Install();
  }

  static void DecreaseSamplerCount() {
    base::LockGuard<base::Mutex> lock_guard(mutex_);
    if (--client_count_ == 0) Restore();
  }

  static bool Installed() {
    base::LockGuard<base::Mutex> lock_guard(mutex_);
    return signal_handler_installed_;
  }

 private:
  static void Install() {
    struct sigaction sa;
    sa.sa_sigaction = &HandleProfilerSignal;
    sigemptyset(&sa.sa_mask);
#if V8_OS_QNX
    sa.sa_flags = SA_SIGINFO;
#else
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
#endif
    signal_handler_installed_ =
        (sigaction(SIGPROF, &sa, &old_signal_handler_) == 0);
  }

  static void Restore() {
    if (signal_handler_installed_) {
      sigaction(SIGPROF, &old_signal_handler_, nullptr);
      signal_handler_installed_ = false;
    }
  }

  static void FillRegisterState(void* context, RegisterState* regs);
  static void HandleProfilerSignal(int signal, siginfo_t* info, void* context);

  // Protects the process wide state below.
  static base::Mutex* mutex_;
  static int client_count_;
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
};

base::Mutex* SignalHandler::mutex_ = nullptr;
int SignalHandler::client_count_ = 0;
struct sigaction SignalHandler::old_signal_handler_;
bool SignalHandler::signal_handler_installed_ = false;


void SignalHandler::HandleProfilerSignal(int signal, siginfo_t* info,
                                         void* context) {
  USE(info);
  if (signal != SIGPROF) return;
  v8::RegisterState state;
  FillRegisterState(context, &state);
  SamplerManager::instance()->DoSample(state);
}

void SignalHandler::FillRegisterState(void* context, RegisterState* state) {
  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
#if !(V8_OS_OPENBSD || (V8_OS_LINUX && (V8_HOST_ARCH_PPC || V8_HOST_ARCH_S390)))
  mcontext_t& mcontext = ucontext->uc_mcontext;
#endif
#if V8_OS_LINUX
#if V8_HOST_ARCH_IA32
  state->pc = reinterpret_cast<void*>(mcontext.gregs[REG_EIP]);
  state->sp = reinterpret_cast<void*>(mcontext.gregs[REG_ESP]);
  state->fp = reinterpret_cast<void*>(mcontext.gregs[REG_EBP]);
#elif V8_HOST_ARCH_X64
  state->pc = reinterpret_cast<void*>(mcontext.gregs[REG_RIP]);
  state->sp = reinterpret_cast<void*>(mcontext.gregs[REG_RSP]);
  state->fp = reinterpret_cast<void*>(mcontext.gregs[REG_RBP]);
#elif V8_HOST_ARCH_ARM
#if V8_LIBC_GLIBC && !V8_GLIBC_PREREQ(2, 4)
  // Old GLibc ARM versions used a gregs[] array to access the register
  // values from mcontext_t.
  state->pc = reinterpret_cast<void*>(mcontext.gregs[R15]);
  state->sp = reinterpret_cast<void*>(mcontext.gregs[R13]);
  state->fp = reinterpret_cast<void*>(mcontext.gregs[R11]);
#else
  state->pc = reinterpret_cast<void*>(mcontext.arm_pc);
  state->sp = reinterpret_cast<void*>(mcontext.arm_sp);
  state->fp = reinterpret_cast<void*>(mcontext.arm_fp);
#endif  // V8_LIBC_GLIBC && !V8_GLIBC_PREREQ(2, 4)
#elif V8_HOST_ARCH_ARM64
  state->pc = reinterpret_cast<void*>(mcontext.pc);
  state->sp = reinterpret_cast<void*>(mcontext.sp);
  // FP is an alias for x29.
  state->fp = reinterpret_cast<void*>(mcontext.regs[29]);
#elif V8_HOST_ARCH_MIPS
  state->pc = reinterpret_cast<void*>(mcontext.pc);
  state->sp = reinterpret_cast<void*>(mcontext.gregs[29]);
  state->fp = reinterpret_cast<void*>(mcontext.gregs[30]);
#elif V8_HOST_ARCH_MIPS64
  state->pc = reinterpret_cast<void*>(mcontext.pc);
  state->sp = reinterpret_cast<void*>(mcontext.gregs[29]);
  state->fp = reinterpret_cast<void*>(mcontext.gregs[30]);
#elif V8_HOST_ARCH_PPC
#if V8_LIBC_GLIBC
  state->pc = reinterpret_cast<void*>(ucontext->uc_mcontext.regs->nip);
  state->sp =
      reinterpret_cast<void*>(ucontext->uc_mcontext.regs->gpr[PT_R1]);
  state->fp =
      reinterpret_cast<void*>(ucontext->uc_mcontext.regs->gpr[PT_R31]);
#else
  // Some C libraries, notably Musl, define the regs member as a void pointer
  state->pc = reinterpret_cast<void*>(ucontext->uc_mcontext.gp_regs[32]);
  state->sp = reinterpret_cast<void*>(ucontext->uc_mcontext.gp_regs[1]);
  state->fp = reinterpret_cast<void*>(ucontext->uc_mcontext.gp_regs[31]);
#endif
#elif V8_HOST_ARCH_S390
#if V8_TARGET_ARCH_32_BIT
  // 31-bit target will have bit 0 (MSB) of the PSW set to denote addressing
  // mode.  This bit needs to be masked out to resolve actual address.
  state->pc =
      reinterpret_cast<void*>(ucontext->uc_mcontext.psw.addr & 0x7FFFFFFF);
#else
  state->pc = reinterpret_cast<void*>(ucontext->uc_mcontext.psw.addr);
#endif  // V8_TARGET_ARCH_32_BIT
  state->sp = reinterpret_cast<void*>(ucontext->uc_mcontext.gregs[15]);
  state->fp = reinterpret_cast<void*>(ucontext->uc_mcontext.gregs[11]);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_MACOSX
#if V8_HOST_ARCH_X64
#if __DARWIN_UNIX03
  state->pc = reinterpret_cast<void*>(mcontext->__ss.__rip);
  state->sp = reinterpret_cast<void*>(mcontext->__ss.__rsp);
  state->fp = reinterpret_cast<void*>(mcontext->__ss.__rbp);
#else  // !__DARWIN_UNIX03
  state->pc = reinterpret_cast<void*>(mcontext->ss.rip);
  state->sp = reinterpret_cast<void*>(mcontext->ss.rsp);
  state->fp = reinterpret_cast<void*>(mcontext->ss.rbp);
#endif  // __DARWIN_UNIX03
#elif V8_HOST_ARCH_IA32
#if __DARWIN_UNIX03
  state->pc = reinterpret_cast<void*>(mcontext->__ss.__eip);
  state->sp = reinterpret_cast<void*>(mcontext->__ss.__esp);
  state->fp = reinterpret_cast<void*>(mcontext->__ss.__ebp);
#else  // !__DARWIN_UNIX03
  state->pc = reinterpret_cast<void*>(mcontext->ss.eip);
  state->sp = reinterpret_cast<void*>(mcontext->ss.esp);
  state->fp = reinterpret_cast<void*>(mcontext->ss.ebp);
#endif  // __DARWIN_UNIX03
#endif  // V8_HOST_ARCH_IA32
#elif V8_OS_FREEBSD
#if V8_HOST_ARCH_IA32
  state->pc = reinterpret_cast<void*>(mcontext.mc_eip);
  state->sp = reinterpret_cast<void*>(mcontext.mc_esp);
  state->fp = reinterpret_cast<void*>(mcontext.mc_ebp);
#elif V8_HOST_ARCH_X64
  state->pc = reinterpret_cast<void*>(mcontext.mc_rip);
  state->sp = reinterpret_cast<void*>(mcontext.mc_rsp);
  state->fp = reinterpret_cast<void*>(mcontext.mc_rbp);
#elif V8_HOST_ARCH_ARM
  state->pc = reinterpret_cast<void*>(mcontext.mc_r15);
  state->sp = reinterpret_cast<void*>(mcontext.mc_r13);
  state->fp = reinterpret_cast<void*>(mcontext.mc_r11);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_NETBSD
#if V8_HOST_ARCH_IA32
  state->pc = reinterpret_cast<void*>(mcontext.__gregs[_REG_EIP]);
  state->sp = reinterpret_cast<void*>(mcontext.__gregs[_REG_ESP]);
  state->fp = reinterpret_cast<void*>(mcontext.__gregs[_REG_EBP]);
#elif V8_HOST_ARCH_X64
  state->pc = reinterpret_cast<void*>(mcontext.__gregs[_REG_RIP]);
  state->sp = reinterpret_cast<void*>(mcontext.__gregs[_REG_RSP]);
  state->fp = reinterpret_cast<void*>(mcontext.__gregs[_REG_RBP]);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_OPENBSD
#if V8_HOST_ARCH_IA32
  state->pc = reinterpret_cast<void*>(ucontext->sc_eip);
  state->sp = reinterpret_cast<void*>(ucontext->sc_esp);
  state->fp = reinterpret_cast<void*>(ucontext->sc_ebp);
#elif V8_HOST_ARCH_X64
  state->pc = reinterpret_cast<void*>(ucontext->sc_rip);
  state->sp = reinterpret_cast<void*>(ucontext->sc_rsp);
  state->fp = reinterpret_cast<void*>(ucontext->sc_rbp);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_SOLARIS
  state->pc = reinterpret_cast<void*>(mcontext.gregs[REG_PC]);
  state->sp = reinterpret_cast<void*>(mcontext.gregs[REG_SP]);
  state->fp = reinterpret_cast<void*>(mcontext.gregs[REG_FP]);
#elif V8_OS_QNX
#if V8_HOST_ARCH_IA32
  state->pc = reinterpret_cast<void*>(mcontext.cpu.eip);
  state->sp = reinterpret_cast<void*>(mcontext.cpu.esp);
  state->fp = reinterpret_cast<void*>(mcontext.cpu.ebp);
#elif V8_HOST_ARCH_ARM
  state->pc = reinterpret_cast<void*>(mcontext.cpu.gpr[ARM_REG_PC]);
  state->sp = reinterpret_cast<void*>(mcontext.cpu.gpr[ARM_REG_SP]);
  state->fp = reinterpret_cast<void*>(mcontext.cpu.gpr[ARM_REG_FP]);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_AIX
  state->pc = reinterpret_cast<void*>(mcontext.jmp_context.iar);
  state->sp = reinterpret_cast<void*>(mcontext.jmp_context.gpr[1]);
  state->fp = reinterpret_cast<void*>(mcontext.jmp_context.gpr[31]);
#endif  // V8_OS_AIX
}

#endif  // USE_SIGNALS


void Sampler::SetUp() {
#if defined(USE_SIGNALS)
  SignalHandler::SetUp();
#endif
}


void Sampler::TearDown() {
#if defined(USE_SIGNALS)
  SignalHandler::TearDown();
#endif
}

Sampler::Sampler(Isolate* isolate)
    : is_counting_samples_(false),
      js_sample_count_(0),
      external_sample_count_(0),
      isolate_(isolate),
      profiling_(false),
      has_processing_thread_(false),
      active_(false),
      registered_(false) {
  data_ = new PlatformData;
}

Sampler::~Sampler() {
  DCHECK(!IsActive());
#if defined(USE_SIGNALS)
  if (IsRegistered()) {
    SamplerManager::instance()->RemoveSampler(this);
  }
#endif
  delete data_;
}

void Sampler::Start() {
  DCHECK(!IsActive());
  SetActive(true);
#if defined(USE_SIGNALS)
  SamplerManager::instance()->AddSampler(this);
#endif
}


void Sampler::Stop() {
#if defined(USE_SIGNALS)
  SamplerManager::instance()->RemoveSampler(this);
#endif
  DCHECK(IsActive());
  SetActive(false);
  SetRegistered(false);
}


void Sampler::IncreaseProfilingDepth() {
  base::Relaxed_AtomicIncrement(&profiling_, 1);
#if defined(USE_SIGNALS)
  SignalHandler::IncreaseSamplerCount();
#endif
}


void Sampler::DecreaseProfilingDepth() {
#if defined(USE_SIGNALS)
  SignalHandler::DecreaseSamplerCount();
#endif
  base::Relaxed_AtomicIncrement(&profiling_, -1);
}


#if defined(USE_SIGNALS)

void Sampler::DoSample() {
  if (!SignalHandler::Installed()) return;
  if (!IsActive() && !IsRegistered()) {
    SamplerManager::instance()->AddSampler(this);
    SetRegistered(true);
  }
  pthread_kill(platform_data()->vm_tid(), SIGPROF);
}

#elif V8_OS_WIN || V8_OS_CYGWIN

void Sampler::DoSample() {
  HANDLE profiled_thread = platform_data()->profiled_thread();
  if (profiled_thread == nullptr) return;

  const DWORD kSuspendFailed = static_cast<DWORD>(-1);
  if (SuspendThread(profiled_thread) == kSuspendFailed) return;

  // Context used for sampling the register state of the profiled thread.
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_FULL;
  if (GetThreadContext(profiled_thread, &context) != 0) {
    v8::RegisterState state;
#if V8_HOST_ARCH_X64
    state.pc = reinterpret_cast<void*>(context.Rip);
    state.sp = reinterpret_cast<void*>(context.Rsp);
    state.fp = reinterpret_cast<void*>(context.Rbp);
#else
    state.pc = reinterpret_cast<void*>(context.Eip);
    state.sp = reinterpret_cast<void*>(context.Esp);
    state.fp = reinterpret_cast<void*>(context.Ebp);
#endif
    SampleStack(state);
  }
  ResumeThread(profiled_thread);
}

#elif V8_OS_FUCHSIA

void Sampler::DoSample() {
  zx_handle_t profiled_thread = platform_data()->profiled_thread();
  if (profiled_thread == ZX_HANDLE_INVALID) return;

  zx_handle_t suspend_token = ZX_HANDLE_INVALID;
  if (zx_task_suspend_token(profiled_thread, &suspend_token) != ZX_OK) return;

  // Wait for the target thread to become suspended, or to exit.
  // TODO(wez): There is currently no suspension count for threads, so there
  // is a risk that some other caller resumes the thread in-between our suspend
  // and wait calls, causing us to miss the SUSPENDED signal. We apply a 100ms
  // deadline to protect against hanging the sampler thread in this case.
  zx_signals_t signals = 0;
  zx_status_t suspended = zx_object_wait_one(
      profiled_thread, ZX_THREAD_SUSPENDED | ZX_THREAD_TERMINATED,
      zx_deadline_after(ZX_MSEC(100)), &signals);
  if (suspended != ZX_OK || (signals & ZX_THREAD_SUSPENDED) == 0) {
    zx_handle_close(suspend_token);
    return;
  }

  // Fetch a copy of its "general register" states.
  zx_thread_state_general_regs_t thread_state = {};
  if (zx_thread_read_state(profiled_thread, ZX_THREAD_STATE_GENERAL_REGS,
                           &thread_state, sizeof(thread_state)) == ZX_OK) {
    v8::RegisterState state;
#if V8_HOST_ARCH_X64
    state.pc = reinterpret_cast<void*>(thread_state.rip);
    state.sp = reinterpret_cast<void*>(thread_state.rsp);
    state.fp = reinterpret_cast<void*>(thread_state.rbp);
#elif V8_HOST_ARCH_ARM64
    state.pc = reinterpret_cast<void*>(thread_state.pc);
    state.sp = reinterpret_cast<void*>(thread_state.sp);
    state.fp = reinterpret_cast<void*>(thread_state.r[29]);
#endif
    SampleStack(state);
  }

  zx_handle_close(suspend_token);
}

// TODO(wez): Remove this once the Fuchsia SDK has rolled.
#if defined(ZX_THREAD_STATE_REGSET0)
#undef ZX_THREAD_STATE_GENERAL_REGS
#endif

#endif  // USE_SIGNALS

}  // namespace sampler
}  // namespace v8
