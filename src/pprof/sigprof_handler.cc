// Copyright 2023 Datadog, Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified for inclusion in Node.js core: ported from the SignalHandler class
// in
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/profilers/wall.cc
//
// SPDX-License-Identifier: Apache-2.0

#include "pprof/sigprof_handler.h"

#ifndef _WIN32

#include <atomic>
#include <mutex>

namespace node {
namespace pprof {

namespace {

using SigactionFunc = void (*)(int, siginfo_t*, void*);
using SighandlerFunc = void (*)(int);

// Protects the process-wide state below, except for the atomics, which the
// signal handler reads.
std::mutex handler_mutex;
int use_count = 0;
bool installed = false;
struct sigaction previous_action;

// The previous handler, in one of its two forms. Both are null when there is
// no handler to forward to (SIG_DFL or SIG_IGN).
std::atomic<SigactionFunc> previous_sigaction{nullptr};
std::atomic<SighandlerFunc> previous_handler{nullptr};

// Signals are delivered to threads, and in Node.js each isolate runs on a
// single thread, so the sink of the thread that receives a signal is the one
// of the isolate that V8 is sampling. This is per-thread rather than
// per-Environment state on purpose.
// The initial-exec TLS model makes every access a single load, without the
// __tls_get_addr() call (which is not async-signal-safe) that the default
// model may compile to when Node.js is built as a shared library.
#if defined(__GLIBC__)
__attribute__((tls_model("initial-exec")))
#endif
// NOLINTNEXTLINE(runtime/thread_local)
thread_local SignalSampleSink* thread_sink = nullptr;

}  // namespace

bool SignalHandler::IncreaseUseCount() {
  std::lock_guard<std::mutex> lock(handler_mutex);
  struct sigaction sa;
  sa.sa_sigaction = &HandleProfilerSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
  if (installed) {
    // Reinstall in case someone else replaced the handler in the meantime.
    sigaction(SIGPROF, &sa, nullptr);
    ++use_count;
    return true;
  }

  struct sigaction prev;
  if (sigaction(SIGPROF, &sa, &prev) != 0) return false;
  if ((prev.sa_flags & SA_SIGINFO) &&
      prev.sa_sigaction == &HandleProfilerSignal) {
    sigaction(SIGPROF, &prev, nullptr);
    return false;
  }

  previous_action = prev;
  if (prev.sa_flags & SA_SIGINFO) {
    previous_handler.store(nullptr, std::memory_order_relaxed);
    previous_sigaction.store(prev.sa_sigaction, std::memory_order_release);
  } else if (prev.sa_handler != SIG_DFL && prev.sa_handler != SIG_IGN) {
    previous_sigaction.store(nullptr, std::memory_order_relaxed);
    previous_handler.store(prev.sa_handler, std::memory_order_release);
  } else {
    previous_sigaction.store(nullptr, std::memory_order_relaxed);
    previous_handler.store(nullptr, std::memory_order_release);
  }
  installed = true;
  ++use_count;
  return true;
}

void SignalHandler::DecreaseUseCount() {
  std::lock_guard<std::mutex> lock(handler_mutex);
  if (use_count == 0 || --use_count > 0 || !installed) return;
  sigaction(SIGPROF, &previous_action, nullptr);
  installed = false;
  previous_sigaction.store(nullptr, std::memory_order_relaxed);
  previous_handler.store(nullptr, std::memory_order_relaxed);
}

void SignalHandler::SetThreadSink(SignalSampleSink* sink) {
  std::atomic_signal_fence(std::memory_order_release);
  thread_sink = sink;
  std::atomic_signal_fence(std::memory_order_release);
}

SignalSampleSink* SignalHandler::GetThreadSink() {
  return thread_sink;
}

void SignalHandler::ForwardSignal(int sig, siginfo_t* info, void* context) {
  SigactionFunc action = previous_sigaction.load(std::memory_order_acquire);
  if (action != nullptr) {
    action(sig, info, context);
    return;
  }
  SighandlerFunc handler = previous_handler.load(std::memory_order_acquire);
  if (handler != nullptr) handler(sig);
}

void SignalHandler::HandleProfilerSignal(int sig,
                                         siginfo_t* info,
                                         void* context) {
  SignalSampleSink* sink = thread_sink;
  std::atomic_signal_fence(std::memory_order_acquire);
  if (sink == nullptr) {
    ForwardSignal(sig, info, context);
    return;
  }
  sink->OnProfilerSignal(sig, info, context);
}

}  // namespace pprof
}  // namespace node

#endif  // _WIN32
