// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "sampler.h"

#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
    || defined(__NetBSD__) || defined(__sun) || defined(__ANDROID__) \
    || defined(__native_client__)

#define USE_SIGNALS

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/syscall.h>
#if !defined(__ANDROID__) || defined(__BIONIC_HAVE_UCONTEXT_T)
#include <ucontext.h>
#endif
#include <unistd.h>

// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#if defined(__ANDROID__) && !defined(__BIONIC_HAVE_UCONTEXT_T) && \
    defined(__arm__) && !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#include <asm/sigcontext.h>
#endif

#elif defined(__MACH__)

#include <mach/mach.h>

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

#include "win32-headers.h"

#endif

#include "v8.h"

#include "cpu-profiler.h"
#include "flags.h"
#include "frames-inl.h"
#include "log.h"
#include "platform.h"
#include "simulator.h"
#include "v8threads.h"


#if defined(__ANDROID__) && !defined(__BIONIC_HAVE_UCONTEXT_T)

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
#endif

#endif  // __ANDROID__ && !defined(__BIONIC_HAVE_UCONTEXT_T)


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

#elif defined(__MACH__)

class Sampler::PlatformData : public PlatformDataCommon {
 public:
  PlatformData() : profiled_thread_(mach_thread_self()) {}

  ~PlatformData() {
    // Deallocate Mach port for thread.
    mach_port_deallocate(mach_task_self(), profiled_thread_);
  }

  thread_act_t profiled_thread() { return profiled_thread_; }

 private:
  // Note: for profiled_thread_ Mach primitives are used instead of PThread's
  // because the latter doesn't provide thread manipulation primitives required.
  // For details, consult "Mac OS X Internals" book, Section 7.3.
  thread_act_t profiled_thread_;
};

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

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
  inline bool Init(Sampler* sampler, Isolate* isolate) {
    ThreadId thread_id = sampler->platform_data()->profiled_thread_id();
    Isolate::PerIsolateThreadData* per_thread_data = isolate->
        FindPerThreadDataForThread(thread_id);
    if (!per_thread_data) return false;
    simulator_ = per_thread_data->simulator();
    // Check if there is active simulator.
    return simulator_ != NULL;
  }

  inline void FillRegisters(RegisterState* state) {
    state->pc = reinterpret_cast<Address>(simulator_->get_pc());
    state->sp = reinterpret_cast<Address>(simulator_->get_register(
        Simulator::sp));
#if V8_TARGET_ARCH_ARM
    state->fp = reinterpret_cast<Address>(simulator_->get_register(
        Simulator::r11));
#elif V8_TARGET_ARCH_MIPS
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
  static inline void EnsureInstalled() {
    if (signal_handler_installed_) return;
    struct sigaction sa;
    sa.sa_sigaction = &HandleProfilerSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    signal_handler_installed_ =
        (sigaction(SIGPROF, &sa, &old_signal_handler_) == 0);
  }

  static inline void Restore() {
    if (signal_handler_installed_) {
      sigaction(SIGPROF, &old_signal_handler_, 0);
      signal_handler_installed_ = false;
    }
  }

  static inline bool Installed() {
    return signal_handler_installed_;
  }

 private:
  static void HandleProfilerSignal(int signal, siginfo_t* info, void* context);
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
};

struct sigaction SignalHandler::old_signal_handler_;
bool SignalHandler::signal_handler_installed_ = false;


void SignalHandler::HandleProfilerSignal(int signal, siginfo_t* info,
                                         void* context) {
#if defined(__native_client__)
  // As Native Client does not support signal handling, profiling
  // is disabled.
  return;
#else
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

  RegisterState state;

#if defined(USE_SIMULATOR)
  SimulatorHelper helper;
  if (!helper.Init(sampler, isolate)) return;
  helper.FillRegisters(&state);
#else
  // Extracting the sample from the context is extremely machine dependent.
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  mcontext_t& mcontext = ucontext->uc_mcontext;
#if defined(__linux__) || defined(__ANDROID__)
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(mcontext.gregs[REG_EIP]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[REG_ESP]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[REG_EBP]);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(mcontext.gregs[REG_RIP]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[REG_RSP]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[REG_RBP]);
#elif V8_HOST_ARCH_ARM
#if defined(__GLIBC__) && !defined(__UCLIBC__) && \
    (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 3))
  // Old GLibc ARM versions used a gregs[] array to access the register
  // values from mcontext_t.
  state.pc = reinterpret_cast<Address>(mcontext.gregs[R15]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[R13]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[R11]);
#else
  state.pc = reinterpret_cast<Address>(mcontext.arm_pc);
  state.sp = reinterpret_cast<Address>(mcontext.arm_sp);
  state.fp = reinterpret_cast<Address>(mcontext.arm_fp);
#endif  // defined(__GLIBC__) && !defined(__UCLIBC__) &&
        // (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 3))
#elif V8_HOST_ARCH_MIPS
  state.pc = reinterpret_cast<Address>(mcontext.pc);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[29]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[30]);
#endif  // V8_HOST_ARCH_*
#elif defined(__FreeBSD__)
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
#elif defined(__NetBSD__)
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(mcontext.__gregs[_REG_EIP]);
  state.sp = reinterpret_cast<Address>(mcontext.__gregs[_REG_ESP]);
  state.fp = reinterpret_cast<Address>(mcontext.__gregs[_REG_EBP]);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(mcontext.__gregs[_REG_RIP]);
  state.sp = reinterpret_cast<Address>(mcontext.__gregs[_REG_RSP]);
  state.fp = reinterpret_cast<Address>(mcontext.__gregs[_REG_RBP]);
#endif  // V8_HOST_ARCH_*
#elif defined(__OpenBSD__)
  USE(mcontext);
#if V8_HOST_ARCH_IA32
  state.pc = reinterpret_cast<Address>(ucontext->sc_eip);
  state.sp = reinterpret_cast<Address>(ucontext->sc_esp);
  state.fp = reinterpret_cast<Address>(ucontext->sc_ebp);
#elif V8_HOST_ARCH_X64
  state.pc = reinterpret_cast<Address>(ucontext->sc_rip);
  state.sp = reinterpret_cast<Address>(ucontext->sc_rsp);
  state.fp = reinterpret_cast<Address>(ucontext->sc_rbp);
#endif  // V8_HOST_ARCH_*
#elif defined(__sun)
  state.pc = reinterpret_cast<Address>(mcontext.gregs[REG_PC]);
  state.sp = reinterpret_cast<Address>(mcontext.gregs[REG_SP]);
  state.fp = reinterpret_cast<Address>(mcontext.gregs[REG_FP]);
#endif  // __sun
#endif  // USE_SIMULATOR
  sampler->SampleStack(state);
#endif  // __native_client__
}

#endif


class SamplerThread : public Thread {
 public:
  static const int kSamplerThreadStackSize = 64 * KB;

  explicit SamplerThread(int interval)
      : Thread(Thread::Options("SamplerThread", kSamplerThreadStackSize)),
        interval_(interval) {}

  static void SetUp() { if (!mutex_) mutex_ = OS::CreateMutex(); }
  static void TearDown() { delete mutex_; }

  static void AddActiveSampler(Sampler* sampler) {
    bool need_to_start = false;
    ScopedLock lock(mutex_);
    if (instance_ == NULL) {
      // Start a thread that will send SIGPROF signal to VM threads,
      // when CPU profiling will be enabled.
      instance_ = new SamplerThread(sampler->interval());
      need_to_start = true;
    }

    ASSERT(sampler->IsActive());
    ASSERT(!instance_->active_samplers_.Contains(sampler));
    ASSERT(instance_->interval_ == sampler->interval());
    instance_->active_samplers_.Add(sampler);

#if defined(USE_SIGNALS)
    SignalHandler::EnsureInstalled();
#endif
    if (need_to_start) instance_->StartSynchronously();
  }

  static void RemoveActiveSampler(Sampler* sampler) {
    SamplerThread* instance_to_remove = NULL;
    {
      ScopedLock lock(mutex_);

      ASSERT(sampler->IsActive());
      bool removed = instance_->active_samplers_.RemoveElement(sampler);
      ASSERT(removed);
      USE(removed);

      // We cannot delete the instance immediately as we need to Join() the
      // thread but we are holding mutex_ and the thread may try to acquire it.
      if (instance_->active_samplers_.is_empty()) {
        instance_to_remove = instance_;
        instance_ = NULL;
#if defined(USE_SIGNALS)
        SignalHandler::Restore();
#endif
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
        ScopedLock lock(mutex_);
        if (active_samplers_.is_empty()) break;
        // When CPU profiling is enabled both JavaScript and C++ code is
        // profiled. We must not suspend.
        for (int i = 0; i < active_samplers_.length(); ++i) {
          Sampler* sampler = active_samplers_.at(i);
          if (!sampler->isolate()->IsInitialized()) continue;
          if (!sampler->IsProfiling()) continue;
          SampleContext(sampler);
        }
      }
      OS::Sleep(interval_);
    }
  }

 private:
#if defined(USE_SIGNALS)

  void SampleContext(Sampler* sampler) {
    if (!SignalHandler::Installed()) return;
    pthread_t tid = sampler->platform_data()->vm_tid();
    pthread_kill(tid, SIGPROF);
  }

#elif defined(__MACH__)

  void SampleContext(Sampler* sampler) {
    thread_act_t profiled_thread = sampler->platform_data()->profiled_thread();

#if defined(USE_SIMULATOR)
    SimulatorHelper helper;
    Isolate* isolate = sampler->isolate();
    if (!helper.Init(sampler, isolate)) return;
#endif

    if (KERN_SUCCESS != thread_suspend(profiled_thread)) return;

#if V8_HOST_ARCH_X64
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
    x86_thread_state64_t thread_state;
    mach_msg_type_number_t count = x86_THREAD_STATE64_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __r ## name
#else
#define REGISTER_FIELD(name) r ## name
#endif  // __DARWIN_UNIX03
#elif V8_HOST_ARCH_IA32
    thread_state_flavor_t flavor = i386_THREAD_STATE;
    i386_thread_state_t thread_state;
    mach_msg_type_number_t count = i386_THREAD_STATE_COUNT;
#if __DARWIN_UNIX03
#define REGISTER_FIELD(name) __e ## name
#else
#define REGISTER_FIELD(name) e ## name
#endif  // __DARWIN_UNIX03
#else
#error Unsupported Mac OS X host architecture.
#endif  // V8_HOST_ARCH

    if (thread_get_state(profiled_thread,
                         flavor,
                         reinterpret_cast<natural_t*>(&thread_state),
                         &count) == KERN_SUCCESS) {
      RegisterState state;
#if defined(USE_SIMULATOR)
      helper.FillRegisters(&state);
#else
      state.pc = reinterpret_cast<Address>(thread_state.REGISTER_FIELD(ip));
      state.sp = reinterpret_cast<Address>(thread_state.REGISTER_FIELD(sp));
      state.fp = reinterpret_cast<Address>(thread_state.REGISTER_FIELD(bp));
#endif  // USE_SIMULATOR
#undef REGISTER_FIELD
      sampler->SampleStack(state);
    }
    thread_resume(profiled_thread);
  }

#elif defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)

  void SampleContext(Sampler* sampler) {
    HANDLE profiled_thread = sampler->platform_data()->profiled_thread();
    if (profiled_thread == NULL) return;

    Isolate* isolate = sampler->isolate();
#if defined(USE_SIMULATOR)
    SimulatorHelper helper;
    if (!helper.Init(sampler, isolate)) return;
#endif

    const DWORD kSuspendFailed = static_cast<DWORD>(-1);
    if (SuspendThread(profiled_thread) == kSuspendFailed) return;

    // Context used for sampling the register state of the profiled thread.
    CONTEXT context;
    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(profiled_thread, &context) != 0) {
      RegisterState state;
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
      sampler->SampleStack(state);
    }
    ResumeThread(profiled_thread);
  }

#endif  // USE_SIGNALS


  // Protects the process wide state below.
  static Mutex* mutex_;
  static SamplerThread* instance_;

  const int interval_;
  List<Sampler*> active_samplers_;

  DISALLOW_COPY_AND_ASSIGN(SamplerThread);
};


Mutex* SamplerThread::mutex_ = NULL;
SamplerThread* SamplerThread::instance_ = NULL;


//
// StackTracer implementation
//
DISABLE_ASAN void TickSample::Init(Isolate* isolate,
                                   const RegisterState& regs) {
  ASSERT(isolate->IsInitialized());
  pc = regs.pc;
  state = isolate->current_vm_state();

  // Avoid collecting traces while doing GC.
  if (state == GC) return;

  const Address js_entry_sp =
      Isolate::js_entry_sp(isolate->thread_local_top());
  if (js_entry_sp == 0) {
    // Not executing JS now.
    return;
  }

  const Address callback = isolate->external_callback();
  if (callback != NULL) {
    external_callback = callback;
    has_external_callback = true;
  } else {
    // Sample potential return address value for frameless invocation of
    // stubs (we'll figure out later, if this value makes sense).
    tos = Memory::Address_at(regs.sp);
    has_external_callback = false;
  }

  SafeStackFrameIterator it(isolate, regs.fp, regs.sp, js_entry_sp);
  top_frame_type = it.top_frame_type();
  int i = 0;
  while (!it.done() && i < TickSample::kMaxFramesCount) {
    stack[i++] = it.frame()->pc();
    it.Advance();
  }
  frames_count = i;
}


void Sampler::SetUp() {
  SamplerThread::SetUp();
}


void Sampler::TearDown() {
  SamplerThread::TearDown();
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
  SamplerThread::AddActiveSampler(this);
}


void Sampler::Stop() {
  ASSERT(IsActive());
  SamplerThread::RemoveActiveSampler(this);
  SetActive(false);
}

void Sampler::SampleStack(const RegisterState& state) {
  TickSample* sample = isolate_->cpu_profiler()->TickSampleEvent();
  TickSample sample_obj;
  if (sample == NULL) sample = &sample_obj;
  sample->Init(isolate_, state);
  if (++samples_taken_ < 0) samples_taken_ = 0;
  Tick(sample);
}

} }  // namespace v8::internal
