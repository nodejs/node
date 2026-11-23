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
// Modified for inclusion in Node.js core: ported from
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/profilers/wall.hh
// without sampling contexts, CPED and async ID support.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef SRC_NODE_PPROF_H_
#define SRC_NODE_PPROF_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "memory_tracker.h"
#include "pprof/cpu_clock.h"
#include "pprof/sigprof_handler.h"
#include "pprof/time_profile.h"
#include "v8-profiler.h"
#include "v8.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace node {

class ExternalReferenceRegistry;

namespace pprof {

// A wall time profiler built on v8::CpuProfiler that produces the profiles
// that lib/internal/perf/pprof turns into pprof profiles.
//
// On POSIX systems, it can intercept the SIGPROF signals that V8's sampler
// sends, to work around a V8 bug that leaves the profiler's event processor
// stuck, and to attribute thread CPU time to the samples.
class WallProfiler final : public BaseObject, public SignalSampleSink {
 public:
  enum class CollectionMode { kNoCollect, kPassThrough, kCollect };

  struct Options {
    int sampling_interval_us;
    int duration_ms;
    bool line_numbers;
    bool workaround_v8_bug;
    bool collect_cpu_time;
  };

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);
  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Dispose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void V8ProfilerStuckEventLoopDetected(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  WallProfiler(Environment* env,
               v8::Local<v8::Object> object,
               const Options& options);
  ~WallProfiler() override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(WallProfiler)
  SET_SELF_SIZE(WallProfiler)

#ifndef _WIN32
  void OnProfilerSignal(int sig, siginfo_t* info, void* context) override;
#endif

  // Returns the CPU time consumed by the profiler's thread since the previous
  // call. Called with the registry of active profilers locked.
  int64_t GetAndResetThreadCpu() {
    return thread_cpu_stop_watch_.GetAndReset();
  }

 private:
  enum class StartStatus { kOk, kTooManyProfiles, kSignalHandlerFailed };

  // Lets the profiler be garbage collected once it no longer runs.
  void MakeWeakIfStopped();
  StartStatus StartInternal();
  v8::MaybeLocal<v8::Object> StopImpl(bool restart);
  v8::MaybeLocal<v8::Object> TranslateProfile(
      const v8::CpuProfile* profile,
      const AttributionByNode* attribution,
      int64_t non_js_threads_cpu_time,
      int64_t v8_to_epoch_offset,
      bool restarted);
  void DisposeProfiler();
  bool InterceptSignal() const;
  void SetCollectionMode(CollectionMode mode);
  bool WaitForSignal(uint64_t target_call_count = 0);

  const Options options_;
  const bool is_main_thread_;

  v8::CpuProfiler* cpu_profiler_ = nullptr;
  v8::ProfilerId profile_id_ = 0;
  uint64_t profile_index_ = 0;
  bool started_ = false;
  int v8_profiler_stuck_event_loop_detected_ = 0;
  // Number of uses of the SIGPROF handler held by the running profiles.
  int signal_handler_uses_ = 0;

  std::atomic<CollectionMode> collection_mode_{CollectionMode::kNoCollect};
  // Number of signals received while not collecting.
  std::atomic<uint64_t> no_collect_call_count_{0};

  int64_t start_thread_cpu_time_ = 0;
  int64_t start_process_cpu_time_ = 0;
  ThreadCpuStopWatch thread_cpu_stop_watch_;

  // Appended to by the signal handler, which runs on the profiler's thread, so
  // its capacity is reserved upfront to never allocate there.
  std::vector<SampleRecord> records_;
};

}  // namespace pprof
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PPROF_H_
