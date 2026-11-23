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

#ifndef SRC_PPROF_SIGPROF_HANDLER_H_
#define SRC_PPROF_SIGPROF_HANDLER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#ifdef _WIN32

namespace node {
namespace pprof {

// V8 does not sample with signals on Windows.
class SignalSampleSink {
 protected:
  ~SignalSampleSink() = default;
};

}  // namespace pprof
}  // namespace node

#else  // _WIN32

#include <signal.h>

namespace node {
namespace pprof {

// Receives the SIGPROF signals that V8's CPU profiler sends to the thread that
// registered it with SignalHandler::SetThreadSink().
class SignalSampleSink {
 public:
  // Runs in signal handler context, so it must be async-signal-safe: no
  // allocations, no locks, no V8 API calls. It is responsible for calling
  // SignalHandler::ForwardSignal() so that V8 can take its sample.
  virtual void OnProfilerSignal(int sig, siginfo_t* info, void* context) = 0;

 protected:
  ~SignalSampleSink() = default;
};

// Process-wide SIGPROF handler installed in front of the one that V8's sampler
// installs, so that the time at which V8 takes each sample can be observed.
//
// It must be installed after V8's handler, i.e. after a v8::CpuProfiler has
// started profiling, and removed before that profiling stops, so that both
// handlers are restored in the reverse order of their installation.
class SignalHandler {
 public:
  // Installs the handler if it is not installed yet. Returns false if
  // sigaction() failed or if the previous handler was already this one (which
  // happens when another SIGPROF user restored a disposition it captured while
  // this handler was installed): chaining to it would loop, and V8's sampler
  // would be unreachable. On failure the use count is left unchanged.
  static bool IncreaseUseCount();

  // Restores the previous handler once the last user is gone.
  static void DecreaseUseCount();

  // Sets the sink that receives the signals delivered to the calling thread,
  // or clears it if `sink` is nullptr. Signals delivered to threads without a
  // sink are forwarded to the previous handler.
  static void SetThreadSink(SignalSampleSink* sink);
  static SignalSampleSink* GetThreadSink();

  // Calls the handler that was installed before this one.
  static void ForwardSignal(int sig, siginfo_t* info, void* context);

 private:
  static void HandleProfilerSignal(int sig, siginfo_t* info, void* context);
};

}  // namespace pprof
}  // namespace node

#endif  // !_WIN32

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PPROF_SIGPROF_HANDLER_H_
