/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cxxabi.h>
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <unwind.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/file_utils.h"

// Some glibc headers hit this when using signals.
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#endif

#if defined(NDEBUG)
#error This translation unit should not be used in release builds
#endif

#if !PERFETTO_BUILDFLAG(PERFETTO_STANDALONE_BUILD)
#error This translation unit should not be used in non-standalone builds
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <backtrace.h>
#endif

namespace {

constexpr size_t kDemangledNameLen = 4096;

bool g_sighandler_registered = false;
char* g_demangled_name = nullptr;

struct SigHandler {
  int sig_num;
  struct sigaction old_handler;
};

SigHandler g_signals[] = {{SIGSEGV, {}}, {SIGILL, {}}, {SIGTRAP, {}},
                          {SIGABRT, {}}, {SIGBUS, {}}, {SIGFPE, {}}};

template <typename T>
void Print(const T& str) {
  perfetto::base::WriteAll(STDERR_FILENO, str, sizeof(str));
}

template <typename T>
void PrintHex(T n) {
  for (unsigned i = 0; i < sizeof(n) * 8; i += 4) {
    char nibble = static_cast<char>(n >> (sizeof(n) * 8 - i - 4)) & 0x0F;
    char c = (nibble < 10) ? '0' + nibble : 'A' + nibble - 10;
    perfetto::base::WriteAll(STDERR_FILENO, &c, 1);
  }
}

struct StackCrawlState {
  StackCrawlState(uintptr_t* frames_arg, size_t max_depth_arg)
      : frames(frames_arg),
        frame_count(0),
        max_depth(max_depth_arg),
        skip_count(1) {}

  uintptr_t* frames;
  size_t frame_count;
  size_t max_depth;
  size_t skip_count;
};

_Unwind_Reason_Code TraceStackFrame(_Unwind_Context* context, void* arg) {
  StackCrawlState* state = static_cast<StackCrawlState*>(arg);
  uintptr_t ip = _Unwind_GetIP(context);

  if (ip != 0 && state->skip_count) {
    state->skip_count--;
    return _URC_NO_REASON;
  }

  state->frames[state->frame_count++] = ip;
  if (state->frame_count >= state->max_depth)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

void RestoreSignalHandlers() {
  for (size_t i = 0; i < sizeof(g_signals) / sizeof(g_signals[0]); i++)
    sigaction(g_signals[i].sig_num, &g_signals[i].old_handler, nullptr);
}

// Note: use only async-safe functions inside this.
void SignalHandler(int sig_num, siginfo_t* info, void* /*ucontext*/) {
  // Restore the old handlers.
  RestoreSignalHandlers();

  Print("\n------------------ BEGINNING OF CRASH ------------------\n");
  Print("Signal: ");
  if (sig_num == SIGSEGV) {
    Print("Segmentation fault");
  } else if (sig_num == SIGILL) {
    Print("Illegal instruction (possibly unaligned access)");
  } else if (sig_num == SIGTRAP) {
    Print("Trap");
  } else if (sig_num == SIGABRT) {
    Print("Abort");
  } else if (sig_num == SIGBUS) {
    Print("Bus Error (possibly unmapped memory access)");
  } else if (sig_num == SIGFPE) {
    Print("Floating point exception");
  } else {
    Print("Unexpected signal ");
    PrintHex(static_cast<uint32_t>(sig_num));
  }

  Print("\n");

  Print("Fault addr: ");
  PrintHex(reinterpret_cast<uintptr_t>(info->si_addr));
  Print("\n\nBacktrace:\n");

  const size_t kMaxFrames = 64;
  uintptr_t frames[kMaxFrames];
  StackCrawlState unwind_state(frames, kMaxFrames);
  _Unwind_Backtrace(&TraceStackFrame, &unwind_state);

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  auto bt_error = [](void*, const char* msg, int) { Print(msg); };
  struct backtrace_state* bt_state =
      backtrace_create_state(nullptr, 0, bt_error, nullptr);
#endif

  for (uint8_t i = 0; i < unwind_state.frame_count; i++) {
    struct SymbolInfo {
      char sym_name[255];
      char file_name[255];
    };
    SymbolInfo sym{{}, {}};

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
    auto symbolize_callback = [](void* data, uintptr_t /*pc*/,
                                 const char* filename, int lineno,
                                 const char* function) -> int {
      SymbolInfo* psym = reinterpret_cast<SymbolInfo*>(data);
      if (function)
        strncpy(psym->sym_name, function, sizeof(psym->sym_name));
      if (filename) {
        snprintf(psym->file_name, sizeof(psym->file_name), "%s:%d", filename,
                 lineno);
      }
      return 0;
    };
    backtrace_pcinfo(bt_state, frames[i], symbolize_callback, bt_error, &sym);
#else
    Dl_info dl_info = {};
    int res = dladdr(reinterpret_cast<void*>(frames[i]), &dl_info);
    if (res && dl_info.dli_sname)
      strncpy(sym.sym_name, dl_info.dli_sname, sizeof(sym.sym_name));
#endif

    Print("\n#");
    PrintHex(i);
    Print("  ");

    if (sym.sym_name[0]) {
      int ignored;
      size_t len = kDemangledNameLen;
      char* demangled =
          abi::__cxa_demangle(sym.sym_name, g_demangled_name, &len, &ignored);
      if (demangled) {
        strncpy(sym.sym_name, demangled, sizeof(sym.sym_name));
        // In the exceptional case of demangling something > kDemangledNameLen,
        // __cxa_demangle will realloc(). In that case the malloc()-ed pointer
        // might be moved.
        g_demangled_name = demangled;
      }
      perfetto::base::WriteAll(STDERR_FILENO, sym.sym_name,
                               strlen(sym.sym_name));
    } else {
      Print("0x");
      PrintHex(frames[i]);
    }
    if (sym.file_name[0]) {
      Print("\n     ");
      perfetto::base::WriteAll(STDERR_FILENO, sym.file_name,
                               strlen(sym.file_name));
    }
    Print("\n");
  }

  Print("------------------ END OF CRASH ------------------\n");

  // info->si_code <= 0 iff SI_FROMUSER (SI_FROMKERNEL otherwise).
  if (info->si_code <= 0 || sig_num == SIGABRT) {
// This signal was triggered by somebody sending us the signal with kill().
// In order to retrigger it, we have to queue a new signal by calling
// kill() ourselves.  The special case (si_pid == 0 && sig == SIGABRT) is
// due to the kernel sending a SIGABRT from a user request via SysRQ.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_MACOSX)
    if (kill(getpid(), sig_num) < 0) {
#else
    if (syscall(__NR_tgkill, getpid(), syscall(__NR_gettid), sig_num) < 0) {
#endif
      // If we failed to kill ourselves (e.g. because a sandbox disallows us
      // to do so), we instead resort to terminating our process. This will
      // result in an incorrect exit code.
      _exit(1);
    }
  }
}

// __attribute__((constructor)) causes a static initializer that automagically
// early runs this function before the main().
void __attribute__((constructor)) EnableStacktraceOnCrashForDebug();

void EnableStacktraceOnCrashForDebug() {
  if (g_sighandler_registered)
    return;
  g_sighandler_registered = true;

  // Pre-allocate the string for __cxa_demangle() to reduce the risk of that
  // invoking realloc() within the signal handler.
  g_demangled_name = reinterpret_cast<char*>(malloc(kDemangledNameLen));
  struct sigaction sigact = {};
  sigact.sa_sigaction = &SignalHandler;
  sigact.sa_flags = static_cast<decltype(sigact.sa_flags)>(
      SA_RESTART | SA_SIGINFO | SA_RESETHAND);
  for (size_t i = 0; i < sizeof(g_signals) / sizeof(g_signals[0]); i++)
    sigaction(g_signals[i].sig_num, &sigact, &g_signals[i].old_handler);

  // Prevents fork()-ed processes to inherit the crash signal handlers. This
  // significantly speeds up gtest death tests, because running the unwinder
  // takes some hundreds of ms. These signal handlers are completely useless
  // in death tests because: (i) death tests are expected to crash by design;
  // (ii) the output of death test is not visible.
  pthread_atfork(nullptr, nullptr, &RestoreSignalHandlers);
}

}  // namespace

#pragma GCC diagnostic pop
