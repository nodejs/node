// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/sampler.h"

#if V8_OS_POSIX && !V8_OS_CYGWIN

#define USE_SIGNALS

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#if !V8_OS_QNX && !V8_OS_NACL && !V8_OS_AIX
#include <sys/syscall.h>  // NOLINT
#endif

#if V8_OS_MACOSX
#include <mach/mach.h>
// OpenBSD doesn't have <ucontext.h>. ucontext_t lives in <signal.h>
// and is a typedef for struct sigcontext. There is no uc_mcontext.
#elif(!V8_OS_ANDROID || defined(__BIONIC_HAVE_UCONTEXT_T)) && \
    !V8_OS_OPENBSD && !V8_OS_NACL
#include <ucontext.h>
#endif

#include <unistd.h>

// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#if V8_OS_ANDROID && !defined(__BIONIC_HAVE_UCONTEXT_T) && \
    (defined(__arm__) || defined(__aarch64__)) && \
    !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#include <asm/sigcontext.h>  // NOLINT
#endif

#elif V8_OS_WIN || V8_OS_CYGWIN

#include "src/base/win32-headers.h"

#endif

#include "src/base/atomic-utils.h"
#include "src/base/platform/platform.h"
#include "src/profiler/cpu-profiler-inl.h"
#include "src/profiler/tick-sample.h"
#include "src/simulator.h"
#include "src/v8threads.h"


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
namespace internal {

namespace {

class PlatformDataCommon : public Malloced {
 public:
  PlatformDataCommon() : profiled_thread_id_(ThreadId::Current()) {}
  ThreadId profiled_thread_id() { return profiled_thread_id_; }

 protected:
  ~PlatformDataCommon() {}

 private:
  ThreadId profiled_thread_id_;
};


typedef List<Sampler*> SamplerList;

#if defined(USE_SIGNALS)
class AtomicGuard {
 public:
  explicit AtomicGuard(base::AtomicValue<int>* atomic, bool is_block = true)
      : atomic_(atomic),
        is_success_(false) {
    do {
      // Use Acquire_Load to gain mutual exclusion.
      USE(atomic_->Value());
      is_success_ = atomic_->TrySetValue(0, 1);
    } while (is_block && !is_success_);
  }

  bool is_success() { return is_success_; }

  ~AtomicGuard() {
    if (is_success_) {
      atomic_->SetValue(0);
    }
    atomic_ = NULL;
  }

 private:
  base::AtomicValue<int>* atomic_;
  bool is_success_;
};


// Returns key for hash map.
void* ThreadKey(pthread_t thread_id) {
  return reinterpret_cast<void*>(thread_id);
}


// Returns hash value for hash map.
uint32_t ThreadHash(pthread_t thread_id) {
#if V8_OS_MACOSX
  return static_cast<uint32_t>(reinterpret_cast<intptr_t>(thread_id));
#else
  return static_cast<uint32_t>(thread_id);
#endif
}
#endif  // USE_SIGNALS

}  // namespace

#if defined(USE_SIGNALS)

class Sampler::PlatformData : public PlatformDataCommon {
 public:
  PlatformData() : vm_tid_(pthread_self()) {}
  pthread_t vm_tid() const { return vm_tid_; }

 private:
  pthread_t vm_tid_;
};

#elif V8_OS_WIN || V8_OS_CYGWIN

// ----------------------------------------------------------------------------
// Win32 profiler support. On Cygwin we use the same sampler implementation as
// on Win32.

class Sampler::PlatformData : public PlatformDataCommon {
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
    if (profiled_thread_ != NULL) {
      CloseHandle(profiled_thread_);
      profiled_thread_ = NULL;
    }
  }

  HANDLE profiled_thread() { return profiled_thread_; }

 private:
  HANDLE profiled_thread_;
};
#endif


#if defined(USE_SIGNALS)

class SignalHandler : public AllStatic {
 public:
  static void SetUp() { if (!mutex_) mutex_ = new base::Mutex(); }
  static void TearDown() { delete mutex_; mutex_ = NULL; }

  static void IncreaseSamplerCount() {
    base::LockGuard<base::Mutex> lock_guard(mutex_);
    if (++client_count_ == 1) Install();
  }

  static void DecreaseSamplerCount() {
    base::LockGuard<base::Mutex> lock_guard(mutex_);
    if (--client_count_ == 0) Restore();
  }

  static bool Installed() {
    return signal_handler_installed_;
  }

#if !V8_OS_NACL
  static void CollectSample(void* context, Sampler* sampler);
#endif

 private:
  static void Install() {
#if !V8_OS_NACL
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
#endif
  }

  static void Restore() {
#if !V8_OS_NACL
    if (signal_handler_installed_) {
      sigaction(SIGPROF, &old_signal_handler_, 0);
      signal_handler_installed_ = false;
    }
#endif
  }

#if !V8_OS_NACL
  static void HandleProfilerSignal(int signal, siginfo_t* info, void* context);
#endif
  // Protects the process wide state below.
  static base::Mutex* mutex_;
  static int client_count_;
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
};


base::Mutex* SignalHandler::mutex_ = NULL;
int SignalHandler::client_count_ = 0;
struct sigaction SignalHandler::old_signal_handler_;
bool SignalHandler::signal_handler_installed_ = false;


// As Native Client does not support signal handling, profiling is disabled.
#if !V8_OS_NACL
void SignalHandler::CollectSample(void* context, Sampler* sampler) {
  if (sampler == NULL || (!sampler->IsProfiling() &&
                          !sampler->IsRegistered())) {
    return;
  }
  Isolate* isolate = sampler->isolate();

  // We require a fully initialized and entered isolate.
  if (isolate == NULL || !isolate->IsInUse()) return;

  if (v8::Locker::IsActive() &&
      !isolate->thread_manager()->IsLockedByCurrentThread()) {
    return;
  }

  v8::RegisterState state;

#if defined(USE_SIMULATOR)
  if (!SimulatorHelper::FillRegisters(isolate, &state)) return;
#else
  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
#if !(V8_OS_OPENBSD || (V8_OS_LINUX && (V8_HOST_ARCH_PPC || V8_HOST_ARCH_S390)))
  mcontext_t& mcontext = ucontext->uc_mcontext;
#endif
#if V8_OS_LINUX
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(mcontext.gregs[REG_EIP]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[REG_ESP]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[REG_EBP]);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(mcontext.gregs[REG_RIP]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[REG_RSP]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[REG_RBP]);
#elif V8_HOST_ARCH_ARM
#if V8_LIBC_GLIBC && !V8_GLIBC_PREREQ(2, 4)
  // Old GLibc ARM versions used a gregs[] array to access the register
  // values from mcontext_t.
  state.pc = reinterpret_cast<Address>(mcontext.gregs[R15]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[R13]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[R11]);
#else
  state.pc = reinterpret_cast<Address>(mcontext.arm_pc);
  state.sp = reinterpret_cast<Address>(mcontext.arm_sp);
  state.fp = reinterpret_cast<Address>(mcontext.arm_fp);
#endif  // V8_LIBC_GLIBC && !V8_GLIBC_PREREQ(2, 4)
#elif V8_HOST_ARCH_ARM64
  state.pc = reinterpret_cast<Address>(mcontext.pc);
  state.sp = reinterpret_cast<Address>(mcontext.sp);
  // FP is an alias for x29.
  state.fp = reinterpret_cast<Address>(mcontext.regs[29]);
#elif V8_HOST_ARCH_MIPS
  state.pc = reinterpret_cast<Address>(mcontext.pc);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[29]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[30]);
#elif V8_HOST_ARCH_MIPS64
  state.pc = reinterpret_cast<Address>(mcontext.pc);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[29]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[30]);
#elif V8_HOST_ARCH_PPC
  state.pc = reinterpret_cast<Address>(ucontext->uc_mcontext.regs->nip);
  state.sp = reinterpret_cast<Address>(ucontext->uc_mcontext.regs->gpr[PT_R1]);
  state.fp = reinterpret_cast<Address>(ucontext->uc_mcontext.regs->gpr[PT_R31]);
#elif V8_HOST_ARCH_S390
#if V8_TARGET_ARCH_32_BIT
  // 31-bit target will have bit 0 (MSB) of the PSW set to denote addressing
  // mode.  This bit needs to be masked out to resolve actual address.
  state.pc =
      reinterpret_cast<Address>(ucontext->uc_mcontext.psw.addr & 0x7FFFFFFF);
#else
  state.pc = reinterpret_cast<Address>(ucontext->uc_mcontext.psw.addr);
#endif  // V8_TARGET_ARCH_32_BIT
  state.sp = reinterpret_cast<Address>(ucontext->uc_mcontext.gregs[15]);
  state.fp = reinterpret_cast<Address>(ucontext->uc_mcontext.gregs[11]);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_MACOSX
#if V8_HOST_ARCH_X64
#if __DARWIN_UNIX03
  state.pc = reinterpret_cast<Address>(mcontext->__ss.__rip);
  state.sp = reinterpret_cast<Address>(mcontext->__ss.__rsp);
  state.fp = reinterpret_cast<Address>(mcontext->__ss.__rbp);
#else  // !__DARWIN_UNIX03
  state.pc = reinterpret_cast<Address>(mcontext->ss.rip);
  state.sp = reinterpret_cast<Address>(mcontext->ss.rsp);
  state.fp = reinterpret_cast<Address>(mcontext->ss.rbp);
#endif  // __DARWIN_UNIX03
#elif V8_HOST_ARCH_IA32
#if __DARWIN_UNIX03
  state.pc = reinterpret_cast<Address>(mcontext->__ss.__eip);
  state.sp = reinterpret_cast<Address>(mcontext->__ss.__esp);
  state.fp = reinterpret_cast<Address>(mcontext->__ss.__ebp);
#else  // !__DARWIN_UNIX03
  state.pc = reinterpret_cast<Address>(mcontext->ss.eip);
  state.sp = reinterpret_cast<Address>(mcontext->ss.esp);
  state.fp = reinterpret_cast<Address>(mcontext->ss.ebp);
#endif  // __DARWIN_UNIX03
#endif  // V8_HOST_ARCH_IA32
#elif V8_OS_FREEBSD
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(mcontext.mc_eip);
  state.sp = reinterpret_cast<Address>(mcontext.mc_esp);
  state.fp = reinterpret_cast<Address>(mcontext.mc_ebp);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(mcontext.mc_rip);
  state.sp = reinterpret_cast<Address>(mcontext.mc_rsp);
  state.fp = reinterpret_cast<Address>(mcontext.mc_rbp);
#elif V8_HOST_ARCH_ARM
  state.pc = reinterpret_cast<Address>(mcontext.mc_r15);
  state.sp = reinterpret_cast<Address>(mcontext.mc_r13);
  state.fp = reinterpret_cast<Address>(mcontext.mc_r11);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_NETBSD
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(mcontext.__gregs[_REG_EIP]);
  state.sp = reinterpret_cast<Address>(mcontext.__gregs[_REG_ESP]);
  state.fp = reinterpret_cast<Address>(mcontext.__gregs[_REG_EBP]);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(mcontext.__gregs[_REG_RIP]);
  state.sp = reinterpret_cast<Address>(mcontext.__gregs[_REG_RSP]);
  state.fp = reinterpret_cast<Address>(mcontext.__gregs[_REG_RBP]);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_OPENBSD
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(ucontext->sc_eip);
  state.sp = reinterpret_cast<Address>(ucontext->sc_esp);
  state.fp = reinterpret_cast<Address>(ucontext->sc_ebp);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(ucontext->sc_rip);
  state.sp = reinterpret_cast<Address>(ucontext->sc_rsp);
  state.fp = reinterpret_cast<Address>(ucontext->sc_rbp);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_SOLARIS
  state.pc = reinterpret_cast<Address>(mcontext.gregs[REG_PC]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[REG_SP]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[REG_FP]);
#elif V8_OS_QNX
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(mcontext.cpu.eip);
  state.sp = reinterpret_cast<Address>(mcontext.cpu.esp);
  state.fp = reinterpret_cast<Address>(mcontext.cpu.ebp);
#elif V8_HOST_ARCH_ARM
  state.pc = reinterpret_cast<Address>(mcontext.cpu.gpr[ARM_REG_PC]);
  state.sp = reinterpret_cast<Address>(mcontext.cpu.gpr[ARM_REG_SP]);
  state.fp = reinterpret_cast<Address>(mcontext.cpu.gpr[ARM_REG_FP]);
#endif  // V8_HOST_ARCH_*
#elif V8_OS_AIX
  state.pc = reinterpret_cast<Address>(mcontext.jmp_context.iar);
  state.sp = reinterpret_cast<Address>(mcontext.jmp_context.gpr[1]);
  state.fp = reinterpret_cast<Address>(mcontext.jmp_context.gpr[31]);
#endif  // V8_OS_AIX
#endif  // USE_SIMULATOR
  sampler->SampleStack(state);
}
#endif  // V8_OS_NACL

#endif  // USE_SIGNALS


class SamplerThread : public base::Thread {
 public:
  static const int kSamplerThreadStackSize = 64 * KB;

  explicit SamplerThread(int interval)
      : Thread(base::Thread::Options("SamplerThread", kSamplerThreadStackSize)),
        interval_(interval) {}

  static void SetUp() { if (!mutex_) mutex_ = new base::Mutex(); }
  static void TearDown() { delete mutex_; mutex_ = NULL; }

  static void AddActiveSampler(Sampler* sampler) {
    bool need_to_start = false;
    base::LockGuard<base::Mutex> lock_guard(mutex_);
    if (instance_ == NULL) {
      // Start a thread that will send SIGPROF signal to VM threads,
      // when CPU profiling will be enabled.
      instance_ = new SamplerThread(sampler->interval());
      need_to_start = true;
    }

    DCHECK(sampler->IsActive());
    DCHECK(instance_->interval_ == sampler->interval());

#if defined(USE_SIGNALS)
    AddSampler(sampler);
#else
    DCHECK(!instance_->active_samplers_.Contains(sampler));
    instance_->active_samplers_.Add(sampler);
#endif  // USE_SIGNALS

    if (need_to_start) instance_->StartSynchronously();
  }

  static void RemoveSampler(Sampler* sampler) {
    SamplerThread* instance_to_remove = NULL;
    {
      base::LockGuard<base::Mutex> lock_guard(mutex_);

      DCHECK(sampler->IsActive() || sampler->IsRegistered());
#if defined(USE_SIGNALS)
      {
        AtomicGuard atomic_guard(&sampler_list_access_counter_);
        // Remove sampler from map.
        pthread_t thread_id = sampler->platform_data()->vm_tid();
        void* thread_key = ThreadKey(thread_id);
        uint32_t thread_hash = ThreadHash(thread_id);
        HashMap::Entry* entry =
            thread_id_to_samplers_.Get().Lookup(thread_key, thread_hash);
        DCHECK(entry != NULL);
        SamplerList* samplers = reinterpret_cast<SamplerList*>(entry->value);
        samplers->RemoveElement(sampler);
        if (samplers->is_empty()) {
          thread_id_to_samplers_.Pointer()->Remove(thread_key, thread_hash);
          delete samplers;
        }
        if (thread_id_to_samplers_.Get().occupancy() == 0) {
          instance_to_remove = instance_;
          instance_ = NULL;
        }
      }
#else
      bool removed = instance_->active_samplers_.RemoveElement(sampler);
      DCHECK(removed);
      USE(removed);

      // We cannot delete the instance immediately as we need to Join() the
      // thread but we are holding mutex_ and the thread may try to acquire it.
      if (instance_->active_samplers_.is_empty()) {
        instance_to_remove = instance_;
        instance_ = NULL;
      }
#endif  // USE_SIGNALS
    }

    if (!instance_to_remove) return;
    instance_to_remove->Join();
    delete instance_to_remove;
  }

  // Unlike AddActiveSampler, this method only adds a sampler,
  // but won't start the sampler thread.
  static void RegisterSampler(Sampler* sampler) {
    base::LockGuard<base::Mutex> lock_guard(mutex_);
#if defined(USE_SIGNALS)
    AddSampler(sampler);
#endif  // USE_SIGNALS
  }

  // Implement Thread::Run().
  virtual void Run() {
    while (true) {
      {
        base::LockGuard<base::Mutex> lock_guard(mutex_);
#if defined(USE_SIGNALS)
        if (thread_id_to_samplers_.Get().occupancy() == 0) break;
        if (SignalHandler::Installed()) {
          for (HashMap::Entry *p = thread_id_to_samplers_.Get().Start();
               p != NULL; p = thread_id_to_samplers_.Get().Next(p)) {
#if V8_OS_AIX && V8_TARGET_ARCH_PPC64
            // on AIX64, cannot cast (void *) to pthread_t which is
            // of type unsigned int (4bytes)
            pthread_t thread_id = reinterpret_cast<intptr_t>(p->key);
#else
            pthread_t thread_id = reinterpret_cast<pthread_t>(p->key);
#endif
            pthread_kill(thread_id, SIGPROF);
          }
        }
#else
        if (active_samplers_.is_empty()) break;
        // When CPU profiling is enabled both JavaScript and C++ code is
        // profiled. We must not suspend.
        for (int i = 0; i < active_samplers_.length(); ++i) {
          Sampler* sampler = active_samplers_.at(i);
          if (!sampler->IsProfiling()) continue;
          sampler->DoSample();
        }
#endif  // USE_SIGNALS
      }
      base::OS::Sleep(base::TimeDelta::FromMilliseconds(interval_));
    }
  }

 private:
  // Protects the process wide state below.
  static base::Mutex* mutex_;
  static SamplerThread* instance_;

  const int interval_;

#if defined(USE_SIGNALS)
  struct HashMapCreateTrait {
    static void Construct(HashMap* allocated_ptr) {
      new (allocated_ptr) HashMap(HashMap::PointersMatch);
    }
  };
  friend class SignalHandler;
  static base::LazyInstance<HashMap, HashMapCreateTrait>::type
      thread_id_to_samplers_;
  static base::AtomicValue<int> sampler_list_access_counter_;
  static void AddSampler(Sampler* sampler) {
    AtomicGuard atomic_guard(&sampler_list_access_counter_);
    // Add sampler into map if needed.
    pthread_t thread_id = sampler->platform_data()->vm_tid();
    HashMap::Entry *entry =
        thread_id_to_samplers_.Pointer()->LookupOrInsert(ThreadKey(thread_id),
                                                         ThreadHash(thread_id));
    if (entry->value == NULL) {
      SamplerList* samplers = new SamplerList();
      samplers->Add(sampler);
      entry->value = samplers;
    } else {
      SamplerList* samplers = reinterpret_cast<SamplerList*>(entry->value);
      if (!samplers->Contains(sampler)) {
        samplers->Add(sampler);
      }
    }
  }
#else
  SamplerList active_samplers_;
#endif  // USE_SIGNALS

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};


base::Mutex* SamplerThread::mutex_ = NULL;
SamplerThread* SamplerThread::instance_ = NULL;
#if defined(USE_SIGNALS)
base::LazyInstance<HashMap, SamplerThread::HashMapCreateTrait>::type
    SamplerThread::thread_id_to_samplers_ = LAZY_INSTANCE_INITIALIZER;
base::AtomicValue<int> SamplerThread::sampler_list_access_counter_(0);

// As Native Client does not support signal handling, profiling is disabled.
#if !V8_OS_NACL
void SignalHandler::HandleProfilerSignal(int signal, siginfo_t* info,
                                         void* context) {
  USE(info);
  if (signal != SIGPROF) return;
  AtomicGuard atomic_guard(&SamplerThread::sampler_list_access_counter_, false);
  if (!atomic_guard.is_success()) return;
  pthread_t thread_id = pthread_self();
  HashMap::Entry* entry =
      SamplerThread::thread_id_to_samplers_.Pointer()->Lookup(
          ThreadKey(thread_id), ThreadHash(thread_id));
  if (entry == NULL)
    return;
  SamplerList* samplers = reinterpret_cast<SamplerList*>(entry->value);
  for (int i = 0; i < samplers->length(); ++i) {
    Sampler* sampler = samplers->at(i);
    CollectSample(context, sampler);
  }
}
#endif  // !V8_OS_NACL
#endif  // USE_SIGNALs


void Sampler::SetUp() {
#if defined(USE_SIGNALS)
  SignalHandler::SetUp();
#endif
  SamplerThread::SetUp();
}


void Sampler::TearDown() {
  SamplerThread::TearDown();
#if defined(USE_SIGNALS)
  SignalHandler::TearDown();
#endif
}

Sampler::Sampler(Isolate* isolate, int interval)
    : isolate_(isolate),
      interval_(interval),
      profiling_(false),
      has_processing_thread_(false),
      active_(false),
      registered_(false),
      is_counting_samples_(false),
      js_sample_count_(0),
      external_sample_count_(0) {
  data_ = new PlatformData;
}

Sampler::~Sampler() {
  DCHECK(!IsActive());
  if (IsRegistered()) {
    SamplerThread::RemoveSampler(this);
  }
  delete data_;
}

void Sampler::Start() {
  DCHECK(!IsActive());
  SetActive(true);
  SamplerThread::AddActiveSampler(this);
}


void Sampler::Stop() {
  DCHECK(IsActive());
  SamplerThread::RemoveSampler(this);
  SetActive(false);
  SetRegistered(false);
}


void Sampler::IncreaseProfilingDepth() {
  base::NoBarrier_AtomicIncrement(&profiling_, 1);
#if defined(USE_SIGNALS)
  SignalHandler::IncreaseSamplerCount();
#endif
}


void Sampler::DecreaseProfilingDepth() {
#if defined(USE_SIGNALS)
  SignalHandler::DecreaseSamplerCount();
#endif
  base::NoBarrier_AtomicIncrement(&profiling_, -1);
}


void Sampler::SampleStack(const v8::RegisterState& state) {
  TickSample* sample = isolate_->cpu_profiler()->StartTickSample();
  TickSample sample_obj;
  if (sample == NULL) sample = &sample_obj;
  sample->Init(isolate_, state, TickSample::kIncludeCEntryFrame, true);
  if (is_counting_samples_ && !sample->timestamp.IsNull()) {
    if (sample->state == JS) ++js_sample_count_;
    if (sample->state == EXTERNAL) ++external_sample_count_;
  }
  Tick(sample);
  if (sample != &sample_obj) {
    isolate_->cpu_profiler()->FinishTickSample();
  }
}


#if defined(USE_SIGNALS)

void Sampler::DoSample() {
  if (!SignalHandler::Installed()) return;
  if (!IsActive() && !IsRegistered()) {
    SamplerThread::RegisterSampler(this);
    SetRegistered(true);
  }
  pthread_kill(platform_data()->vm_tid(), SIGPROF);
}

#elif V8_OS_WIN || V8_OS_CYGWIN

void Sampler::DoSample() {
  HANDLE profiled_thread = platform_data()->profiled_thread();
  if (profiled_thread == NULL) return;

  const DWORD kSuspendFailed = static_cast<DWORD>(-1);
  if (SuspendThread(profiled_thread) == kSuspendFailed) return;

  // Context used for sampling the register state of the profiled thread.
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_FULL;
  if (GetThreadContext(profiled_thread, &context) != 0) {
    v8::RegisterState state;
#if defined(USE_SIMULATOR)
    if (!SimulatorHelper::FillRegisters(isolate(), &state)) {
      ResumeThread(profiled_thread);
      return;
    }
#else
#if V8_HOST_ARCH_X64
    state.pc = reinterpret_cast<Address>(context.Rip);
    state.sp = reinterpret_cast<Address>(context.Rsp);
    state.fp = reinterpret_cast<Address>(context.Rbp);
#else
    state.pc = reinterpret_cast<Address>(context.Eip);
    state.sp = reinterpret_cast<Address>(context.Esp);
    state.fp = reinterpret_cast<Address>(context.Ebp);
#endif
#endif  // USE_SIMULATOR
    SampleStack(state);
  }
  ResumeThread(profiled_thread);
}

#endif  // USE_SIGNALS


}  // namespace internal
}  // namespace v8
