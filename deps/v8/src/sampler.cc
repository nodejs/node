// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sampler.h"

#if V8_OS_POSIX && !V8_OS_CYGWIN

#define USE_SIGNALS

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#if !V8_OS_QNX && !V8_OS_NACL
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

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/cpu-profiler-inl.h"
#include "src/flags.h"
#include "src/frames-inl.h"
#include "src/log.h"
#include "src/simulator.h"
#include "src/v8threads.h"
#include "src/vm-state-inl.h"


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


#if defined(USE_SIMULATOR)
class SimulatorHelper {
 public:
  inline bool Init(Isolate* isolate) {
    simulator_ = isolate->thread_local_top()->simulator_;
    // Check if there is active simulator.
    return simulator_ != NULL;
  }

  inline void FillRegisters(v8::RegisterState* state) {
#if V8_TARGET_ARCH_ARM
    state->pc = reinterpret_cast<Address>(simulator_->get_pc());
    state->sp = reinterpret_cast<Address>(simulator_->get_register(
        Simulator::sp));
    state->fp = reinterpret_cast<Address>(simulator_->get_register(
        Simulator::r11));
#elif V8_TARGET_ARCH_ARM64
    if (simulator_->sp() == 0 || simulator_->fp() == 0) {
      // It's possible that the simulator is interrupted while it is updating
      // the sp or fp register. ARM64 simulator does this in two steps:
      // first setting it to zero and then setting it to a new value.
      // Bailout if sp/fp doesn't contain the new value.
      return;
    }
    state->pc = reinterpret_cast<Address>(simulator_->pc());
    state->sp = reinterpret_cast<Address>(simulator_->sp());
    state->fp = reinterpret_cast<Address>(simulator_->fp());
#elif V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
    state->pc = reinterpret_cast<Address>(simulator_->get_pc());
    state->sp = reinterpret_cast<Address>(simulator_->get_register(
        Simulator::sp));
    state->fp = reinterpret_cast<Address>(simulator_->get_register(
        Simulator::fp));
#endif
  }

 private:
  Simulator* simulator_;
};
#endif  // USE_SIMULATOR


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
void SignalHandler::HandleProfilerSignal(int signal, siginfo_t* info,
                                         void* context) {
  USE(info);
  if (signal != SIGPROF) return;
  Isolate* isolate = Isolate::UnsafeCurrent();
  if (isolate == NULL || !isolate->IsInUse()) {
    // We require a fully initialized and entered isolate.
    return;
  }
  if (v8::Locker::IsActive() &&
      !isolate->thread_manager()->IsLockedByCurrentThread()) {
    return;
  }

  Sampler* sampler = isolate->logger()->sampler();
  if (sampler == NULL) return;

  v8::RegisterState state;

#if defined(USE_SIMULATOR)
  SimulatorHelper helper;
  if (!helper.Init(isolate)) return;
  helper.FillRegisters(&state);
  // It possible that the simulator is interrupted while it is updating
  // the sp or fp register. ARM64 simulator does this in two steps:
  // first setting it to zero and then setting it to the new value.
  // Bailout if sp/fp doesn't contain the new value.
  if (state.sp == 0 || state.fp == 0) return;
#else
  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
#if !V8_OS_OPENBSD
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
#endif  // V8_OS_QNX
#endif  // USE_SIMULATOR
  sampler->SampleStack(state);
}
#endif  // V8_OS_NACL

#endif


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
    DCHECK(!instance_->active_samplers_.Contains(sampler));
    DCHECK(instance_->interval_ == sampler->interval());
    instance_->active_samplers_.Add(sampler);

    if (need_to_start) instance_->StartSynchronously();
  }

  static void RemoveActiveSampler(Sampler* sampler) {
    SamplerThread* instance_to_remove = NULL;
    {
      base::LockGuard<base::Mutex> lock_guard(mutex_);

      DCHECK(sampler->IsActive());
      bool removed = instance_->active_samplers_.RemoveElement(sampler);
      DCHECK(removed);
      USE(removed);

      // We cannot delete the instance immediately as we need to Join() the
      // thread but we are holding mutex_ and the thread may try to acquire it.
      if (instance_->active_samplers_.is_empty()) {
        instance_to_remove = instance_;
        instance_ = NULL;
      }
    }

    if (!instance_to_remove) return;
    instance_to_remove->Join();
    delete instance_to_remove;
  }

  // Implement Thread::Run().
  virtual void Run() {
    while (true) {
      {
        base::LockGuard<base::Mutex> lock_guard(mutex_);
        if (active_samplers_.is_empty()) break;
        // When CPU profiling is enabled both JavaScript and C++ code is
        // profiled. We must not suspend.
        for (int i = 0; i < active_samplers_.length(); ++i) {
          Sampler* sampler = active_samplers_.at(i);
          if (!sampler->IsProfiling()) continue;
          sampler->DoSample();
        }
      }
      base::OS::Sleep(interval_);
    }
  }

 private:
  // Protects the process wide state below.
  static base::Mutex* mutex_;
  static SamplerThread* instance_;

  const int interval_;
  List<Sampler*> active_samplers_;

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};


base::Mutex* SamplerThread::mutex_ = NULL;
SamplerThread* SamplerThread::instance_ = NULL;


//
// StackTracer implementation
//
DISABLE_ASAN void TickSample::Init(Isolate* isolate,
                                   const v8::RegisterState& regs,
                                   RecordCEntryFrame record_c_entry_frame) {
  timestamp = base::TimeTicks::HighResolutionNow();
  pc = reinterpret_cast<Address>(regs.pc);
  state = isolate->current_vm_state();

  // Avoid collecting traces while doing GC.
  if (state == GC) return;

  Address js_entry_sp = isolate->js_entry_sp();
  if (js_entry_sp == 0) return;  // Not executing JS now.

  ExternalCallbackScope* scope = isolate->external_callback_scope();
  Address handler = Isolate::handler(isolate->thread_local_top());
  // If there is a handler on top of the external callback scope then
  // we have already entrered JavaScript again and the external callback
  // is not the top function.
  if (scope && scope->scope_address() < handler) {
    external_callback = scope->callback();
    has_external_callback = true;
  } else {
    // Sample potential return address value for frameless invocation of
    // stubs (we'll figure out later, if this value makes sense).
    tos = Memory::Address_at(reinterpret_cast<Address>(regs.sp));
    has_external_callback = false;
  }

  SafeStackFrameIterator it(isolate, reinterpret_cast<Address>(regs.fp),
                            reinterpret_cast<Address>(regs.sp), js_entry_sp);
  top_frame_type = it.top_frame_type();

  SampleInfo info;
  GetStackSample(isolate, regs, record_c_entry_frame,
                 reinterpret_cast<void**>(&stack[0]), kMaxFramesCount, &info);
  frames_count = static_cast<unsigned>(info.frames_count);
}


void TickSample::GetStackSample(Isolate* isolate, const v8::RegisterState& regs,
                                RecordCEntryFrame record_c_entry_frame,
                                void** frames, size_t frames_limit,
                                v8::SampleInfo* sample_info) {
  sample_info->frames_count = 0;
  sample_info->vm_state = isolate->current_vm_state();
  if (sample_info->vm_state == GC) return;

  Address js_entry_sp = isolate->js_entry_sp();
  if (js_entry_sp == 0) return;  // Not executing JS now.

  SafeStackFrameIterator it(isolate, reinterpret_cast<Address>(regs.fp),
                            reinterpret_cast<Address>(regs.sp), js_entry_sp);
  size_t i = 0;
  if (record_c_entry_frame == kIncludeCEntryFrame && !it.done() &&
      it.top_frame_type() == StackFrame::EXIT) {
    frames[i++] = isolate->c_function();
  }
  while (!it.done() && i < frames_limit) {
    frames[i++] = it.frame()->pc();
    it.Advance();
  }
  sample_info->frames_count = i;
}


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
      is_counting_samples_(false),
      js_and_external_sample_count_(0) {
  data_ = new PlatformData;
}


Sampler::~Sampler() {
  DCHECK(!IsActive());
  delete data_;
}


void Sampler::Start() {
  DCHECK(!IsActive());
  SetActive(true);
  SamplerThread::AddActiveSampler(this);
}


void Sampler::Stop() {
  DCHECK(IsActive());
  SamplerThread::RemoveActiveSampler(this);
  SetActive(false);
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
  sample->Init(isolate_, state, TickSample::kIncludeCEntryFrame);
  if (is_counting_samples_) {
    if (sample->state == JS || sample->state == EXTERNAL) {
      ++js_and_external_sample_count_;
    }
  }
  Tick(sample);
  if (sample != &sample_obj) {
    isolate_->cpu_profiler()->FinishTickSample();
  }
}


#if defined(USE_SIGNALS)

void Sampler::DoSample() {
  if (!SignalHandler::Installed()) return;
  pthread_kill(platform_data()->vm_tid(), SIGPROF);
}

#elif V8_OS_WIN || V8_OS_CYGWIN

void Sampler::DoSample() {
  HANDLE profiled_thread = platform_data()->profiled_thread();
  if (profiled_thread == NULL) return;

#if defined(USE_SIMULATOR)
  SimulatorHelper helper;
  if (!helper.Init(isolate())) return;
#endif

  const DWORD kSuspendFailed = static_cast<DWORD>(-1);
  if (SuspendThread(profiled_thread) == kSuspendFailed) return;

  // Context used for sampling the register state of the profiled thread.
  CONTEXT context;
  memset(&context, 0, sizeof(context));
  context.ContextFlags = CONTEXT_FULL;
  if (GetThreadContext(profiled_thread, &context) != 0) {
    v8::RegisterState state;
#if defined(USE_SIMULATOR)
    helper.FillRegisters(&state);
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


} }  // namespace v8::internal
