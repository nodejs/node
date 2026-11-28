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
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/profilers/wall.cc
// and
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/translate-time-profile.cc
// (Copyright 2018 Google Inc.), without sampling contexts, CPED and async ID
// support. Profiles are returned as flat tables of nodes rather than as trees
// of objects.
//
// SPDX-License-Identifier: Apache-2.0

#include "node_pprof.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <unordered_set>
#include <utility>

#ifndef _WIN32
#include <time.h>
#endif

namespace node {
namespace pprof {

using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::CpuProfile;
using v8::CpuProfileNode;
using v8::CpuProfiler;
using v8::CpuProfilingMode;
using v8::CpuProfilingResult;
using v8::CpuProfilingStatus;
using v8::Float64Array;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int32Array;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::ProfilerId;
using v8::String;
using v8::Uint32Array;
using v8::Undefined;
using v8::Value;

namespace {

// Upper bound of the number of SampleRecords buffered per profile.
constexpr size_t kMaxSampleRecords = 1 << 22;

// The profiler running on the current thread, if any. There is at most one
// per thread, even when several Environments share it, since V8 delivers its
// sampling signals to threads, and samples are attributed through a single
// per-thread signal sink.
// NOLINTNEXTLINE(runtime/thread_local)
thread_local WallProfiler* thread_profiler = nullptr;

// Registry of the profilers running on all threads. Profilers publish the CPU
// time consumed by their thread through it, so that the main thread's profiler
// can compute the CPU time consumed by threads that do not run JavaScript.
class ActiveProfilers {
 public:
  void Add(WallProfiler* profiler) {
    std::lock_guard<std::mutex> lock(mutex_);
    profilers_.insert(profiler);
  }

  void Remove(WallProfiler* profiler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = profilers_.find(profiler);
    if (it == profilers_.end()) return;
    terminated_threads_cpu_time_ += profiler->GetAndResetThreadCpu();
    profilers_.erase(it);
  }

  int64_t GatherTotalThreadCpuAndReset() {
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t total = terminated_threads_cpu_time_;
    terminated_threads_cpu_time_ = 0;
    for (WallProfiler* profiler : profilers_) {
      total += profiler->GetAndResetThreadCpu();
    }
    return total;
  }

 private:
  std::mutex mutex_;
  std::unordered_set<WallProfiler*> profilers_;
  int64_t terminated_threads_cpu_time_ = 0;
};

// Never destroyed, since worker threads may still use it while the process
// exits.
ActiveProfilers& GetActiveProfilers() {
  static ActiveProfilers* profilers = new ActiveProfilers();
  return *profilers;
}

// Returns the total hit count of the profile, and whether it has a leaf node
// without hits.
int GetTotalHitCount(const CpuProfileNode* root, bool* no_hit_leaf) {
  int total = 0;
  std::vector<const CpuProfileNode*> stack{root};
  while (!stack.empty()) {
    const CpuProfileNode* node = stack.back();
    stack.pop_back();
    int hit_count = node->GetHitCount();
    int child_count = node->GetChildrenCount();
    total += hit_count;
    if (child_count == 0 && hit_count == 0) *no_hit_leaf = true;
    for (int i = 0; i < child_count; i++) stack.push_back(node->GetChild(i));
  }
  return total;
}

// Returns 0 if no bug was detected, 1 if one possibly was (which may be a
// false positive), or 2 if one certainly was.
//
// When the profiler works correctly, at least one node has hits, and there
// are strictly more samples than hits, because of the sample taken when
// profiling starts and possibly some deoptimization samples. Otherwise,
// v8::SamplingEventsProcessor::ProcessOneSample() is stuck on ticks_buffer_ or
// vm_ticks_buffer_.
int DetectV8Bug(const CpuProfile* profile) {
  bool no_hit_leaf = false;
  int total_hit_count =
      GetTotalHitCount(profile->GetTopDownRoot(), &no_hit_leaf);
  if (total_hit_count == 0) return 2;
  // Tick samples older than the start of the profile can be discarded because
  // of queueing, which can make the sample count equal to the hit count even
  // though the profiler works. A leaf without hits means that a non-tick
  // sample was processed.
  if (profile->GetSamplesCount() == total_hit_count && !no_hit_leaf) return 1;
  return 0;
}

#ifndef _WIN32
// Returns the offset to add to a V8 timer value to get microseconds since the
// Unix epoch. Makes at most kMaxAttempts attempts to read the system clock
// within a single V8 timer microsecond, and keeps the one with the smallest
// error. This is redone for every profile, since the difference between the
// clocks can drift.
int64_t GetV8ToEpochOffset() {
  constexpr int kMaxAttempts = 20;
  int64_t offset = 0;
  int64_t smallest_diff = std::numeric_limits<int64_t>::max();
  for (int i = 0; i < kMaxAttempts; i++) {
    int64_t v8_now = MonotonicNowMicros();
    int64_t epoch_now = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    int64_t v8_now2 = MonotonicNowMicros();
    int64_t diff = v8_now2 - v8_now;
    if (diff < smallest_diff) {
      offset = epoch_now - Midpoint(v8_now, v8_now2);
      if (diff == 0) break;
      smallest_diff = diff;
    }
  }
  return offset;
}

// Spins until the V8 timer moves on, so that samples taken from now on cannot
// have the same timestamp as those taken before.
void WaitForTimerTick() {
  int64_t now = MonotonicNowMicros();
  while (MonotonicNowMicros() == now) {
  }
}
#endif  // _WIN32

template <typename T, typename Array>
Local<Array> NewTypedArray(Isolate* isolate, const std::vector<T>& values) {
  Local<ArrayBuffer> buffer =
      ArrayBuffer::New(isolate, values.size() * sizeof(T));
  if (!values.empty()) {
    memcpy(buffer->Data(), values.data(), values.size() * sizeof(T));
  }
  return Array::New(buffer, 0, values.size());
}

}  // namespace

WallProfiler::WallProfiler(Environment* env,
                           Local<Object> object,
                           const Options& options)
    : BaseObject(env, object),
      options_(options),
      is_main_thread_(env->is_main_thread()) {
  MakeWeak();
  if (options_.collect_cpu_time) {
    // Records are taken once per sample, i.e. about duration / interval times
    // per profile. Leave room for twice as many.
    size_t capacity = static_cast<size_t>(options_.duration_ms) * 1000 * 2 /
                      options_.sampling_interval_us;
    records_.reserve(
        std::min(std::max<size_t>(capacity, 1), kMaxSampleRecords));
  }
}

WallProfiler::~WallProfiler() {
  DisposeProfiler();
}

void WallProfiler::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("records",
                              records_.capacity() * sizeof(SampleRecord));
}

bool WallProfiler::InterceptSignal() const {
#ifdef _WIN32
  return false;
#else
  return options_.workaround_v8_bug || options_.collect_cpu_time;
#endif
}

void WallProfiler::SetCollectionMode(CollectionMode mode) {
  std::atomic_signal_fence(std::memory_order_release);
  collection_mode_.store(mode, std::memory_order_relaxed);
}

#ifndef _WIN32
void WallProfiler::OnProfilerSignal(int sig, siginfo_t* info, void* context) {
  CollectionMode mode = collection_mode_.load(std::memory_order_relaxed);
  std::atomic_signal_fence(std::memory_order_acquire);
  switch (mode) {
    case CollectionMode::kNoCollect:
      // Drop the sample, and let WaitForSignal() know that a signal arrived.
      no_collect_call_count_.fetch_add(1, std::memory_order_relaxed);
      return;
    case CollectionMode::kPassThrough:
      SignalHandler::ForwardSignal(sig, info, context);
      return;
    case CollectionMode::kCollect:
      break;
  }

  int64_t cpu_time = CurrentThreadCpuNanos();
  int64_t time_from = MonotonicNowMicros();
  SignalHandler::ForwardSignal(sig, info, context);
  int64_t time_to = MonotonicNowMicros();
  // The capacity was reserved upfront, so this never allocates.
  if (records_.size() < records_.capacity()) {
    records_.push_back({time_from, time_to, cpu_time});
  }
}
#endif  // _WIN32

bool WallProfiler::WaitForSignal(uint64_t target_call_count) {
  uint64_t call_count = no_collect_call_count_.load(std::memory_order_relaxed);
  std::atomic_signal_fence(std::memory_order_acquire);
  if (target_call_count != 0) {
    if (call_count >= target_call_count) return true;
  } else {
    target_call_count = call_count + 1;
  }
#ifndef _WIN32
  // Wait for at most two sampling intervals. Unlike uv_sleep(), nanosleep()
  // returns as soon as a signal interrupts it.
  constexpr int64_t kNanosPerSecond = 1000 * 1000 * 1000;
  int64_t nanos = int64_t{options_.sampling_interval_us} * 2 * 1000;
  timespec ts;
  ts.tv_sec = static_cast<time_t>(nanos / kNanosPerSecond);
  ts.tv_nsec =
      static_cast<long>(nanos % kNanosPerSecond);  // NOLINT(runtime/int)
  nanosleep(&ts, nullptr);
#endif  // _WIN32
  call_count = no_collect_call_count_.load(std::memory_order_relaxed);
  std::atomic_signal_fence(std::memory_order_acquire);
  return call_count >= target_call_count;
}

WallProfiler::StartStatus WallProfiler::StartInternal() {
  Isolate* isolate = env()->isolate();
  // The strings used as profile titles are only released when the
  // v8::CpuProfiler is disposed, so alternate between two of them.
  Local<String> title = (profile_index_++ % 2 == 0)
                            ? FIXED_ONE_BYTE_STRING(isolate, "pprof-0")
                            : FIXED_ONE_BYTE_STRING(isolate, "pprof-1");
  // Always record samples, to be able to tell whether non-tick samples (the
  // starting sample and deoptimization samples) were processed, and therefore
  // whether SamplingEventsProcessor::ProcessOneSample() is stuck on
  // vm_ticks_buffer_.
  CpuProfilingResult result = cpu_profiler_->Start(
      title,
      options_.line_numbers ? CpuProfilingMode::kCallerLineNumbers
                            : CpuProfilingMode::kLeafNodeLineNumbers,
      /* record_samples */ true);
  if (result.status == CpuProfilingStatus::kErrorTooManyProfilers) {
    return StartStatus::kTooManyProfiles;
  }
  profile_id_ = result.id;

#ifndef _WIN32
  // V8 installs its SIGPROF handler when it starts sampling, so ours goes in
  // front of it, and is (re)installed for every profile.
  if (InterceptSignal()) {
    if (!SignalHandler::IncreaseUseCount()) {
      return StartStatus::kSignalHandlerFailed;
    }
    signal_handler_uses_++;
  }
#endif  // _WIN32

  if (options_.collect_cpu_time) {
    start_thread_cpu_time_ = CurrentThreadCpuNanos();
    start_process_cpu_time_ = ProcessCpuNanos();
  }

  // Collect two more non-tick samples, which do not add to the hit count, so
  // that DetectV8Bug() can tell when V8's event processor is stuck on
  // ticks_from_vm_buffer_. A non-tick sample is already taken when profiling
  // starts, but the first tick sample can be discarded if its timestamp
  // predates the start of the profile because of queueing, while it still
  // adds to the hit count. This is not needed with the workaround, which waits
  // for a signal before starting a new profile, leaving time to process
  // in-flight tick samples.
  if (!options_.workaround_v8_bug) {
    CpuProfiler::CollectSample(isolate);
    CpuProfiler::CollectSample(isolate);
  }
  return StartStatus::kOk;
}

MaybeLocal<Object> WallProfiler::StopImpl(bool restart) {
  CHECK(started_);
  uint64_t call_count = 0;
  ProfilerId old_profile_id = profile_id_;

#ifndef _WIN32
  if (restart && options_.workaround_v8_bug) {
    SetCollectionMode(CollectionMode::kNoCollect);
    WaitForSignal();
  } else if (options_.collect_cpu_time) {
    SetCollectionMode(CollectionMode::kNoCollect);
    // Make sure that the timer moves on, so that no sample of the next profile
    // is matched with a record of this one.
    WaitForTimerTick();
  }
#endif  // _WIN32

  int64_t start_thread_cpu_time = start_thread_cpu_time_;
  int64_t start_process_cpu_time = start_process_cpu_time_;

  bool restarted = false;
  if (restart) {
    // Start the next profile before stopping this one, so that no sample is
    // missed in between. This cannot fail in practice: the profiler only ever
    // runs two profiles, and the signal handler is already installed.
    restarted = StartInternal() == StartStatus::kOk;
    call_count = no_collect_call_count_.load(std::memory_order_relaxed);
    std::atomic_signal_fence(std::memory_order_acquire);
  }

#ifndef _WIN32
  // Release the use of the signal handler taken for the profile being stopped.
  if (InterceptSignal() && signal_handler_uses_ > 0) {
    SignalHandler::DecreaseUseCount();
    signal_handler_uses_--;
  }
#endif  // _WIN32

  CpuProfile* profile = cpu_profiler_->Stop(old_profile_id);

  std::vector<SampleRecord> records;
  if (options_.collect_cpu_time) {
    records.reserve(records_.capacity());
    std::swap(records, records_);
  }

  v8_profiler_stuck_event_loop_detected_ =
      profile != nullptr ? DetectV8Bug(profile) : 2;

#ifndef _WIN32
  if (restarted && options_.collect_cpu_time && !options_.workaround_v8_bug) {
    // Make sure that the starting sample of the next profile is not mixed up
    // with a sample taken from the signal handler. With the workaround,
    // collection resumes at the end instead.
    WaitForTimerTick();
    SetCollectionMode(CollectionMode::kCollect);
  }
#endif  // _WIN32

  MaybeLocal<Object> result;
  if (profile != nullptr) {
    int64_t non_js_threads_cpu_time = 0;
    AttributionByNode attribution;
    if (options_.collect_cpu_time) {
      if (is_main_thread_) {
        // The CPU time of the threads that do not run JavaScript is the
        // process CPU time minus that of all the threads that do, during the
        // profile of the main thread. It is only accounted for there.
        int64_t threads_cpu_time =
            GetActiveProfilers().GatherTotalThreadCpuAndReset();
        int64_t process_cpu_time = ProcessCpuNanos() - start_process_cpu_time;
        non_js_threads_cpu_time =
            std::max<int64_t>(process_cpu_time - threads_cpu_time, 0);
      }
      std::vector<ProfileSample> samples;
      int sample_count = profile->GetSamplesCount();
      samples.reserve(sample_count);
      for (int i = 0; i < sample_count; i++) {
        const CpuProfileNode* node = profile->GetSample(i);
        samples.push_back(
            {profile->GetSampleTimestamp(i),
             node,
             strcmp(node->GetFunctionNameStr(), "(program)") == 0});
      }
      attribution = AttributeSamples(samples, records, start_thread_cpu_time);
    }

#ifdef _WIN32
    int64_t v8_to_epoch_offset = 0;
#else
    // One offset for the profile, so that its start and end times and its
    // samples' timestamps agree.
    int64_t v8_to_epoch_offset = GetV8ToEpochOffset();
#endif  // _WIN32
    result =
        TranslateProfile(profile,
                         options_.collect_cpu_time ? &attribution : nullptr,
                         non_js_threads_cpu_time,
                         v8_to_epoch_offset,
                         restarted);
    profile->Delete();
  }

  if (!restarted) {
    DisposeProfiler();
  } else if (options_.workaround_v8_bug) {
    WaitForSignal(call_count + 1);
    SetCollectionMode(options_.collect_cpu_time ? CollectionMode::kCollect
                                                : CollectionMode::kPassThrough);
  }

  return result;
}

MaybeLocal<Object> WallProfiler::TranslateProfile(
    const CpuProfile* profile,
    const AttributionByNode* attribution,
    int64_t non_js_threads_cpu_time,
    int64_t v8_to_epoch_offset,
    bool restarted) {
  Environment* env = this->env();
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  // The nodes of the profile tree, in pre-order, root first. Every node comes
  // after its parent.
  std::vector<int32_t> parents;
  std::vector<int32_t> script_ids;
  std::vector<int32_t> line_numbers;
  std::vector<int32_t> column_numbers;
  std::vector<uint32_t> hit_counts;
  std::vector<double> cpu_times;
  LocalVector<Value> names(isolate);
  LocalVector<Value> script_names(isolate);
  double total_hit_count = 0;

  // `metadata` is the V8 node that the row describes the function of.
  auto add_row = [&](int32_t parent,
                     const CpuProfileNode* metadata,
                     int line_number,
                     int column_number,
                     uint32_t hit_count,
                     int64_t cpu_time) {
    parents.push_back(parent);
    names.push_back(metadata->GetFunctionName());
    script_names.push_back(metadata->GetScriptResourceName());
    script_ids.push_back(metadata->GetScriptId());
    line_numbers.push_back(line_number);
    column_numbers.push_back(column_number);
    hit_counts.push_back(hit_count);
    if (attribution != nullptr) {
      cpu_times.push_back(static_cast<double>(cpu_time));
    }
    total_hit_count += hit_count;
    return static_cast<int32_t>(parents.size() - 1);
  };

  const CpuProfileNode* root = profile->GetTopDownRoot();
  // Pairs of the row of a node's parent, and the node.
  std::vector<std::pair<int32_t, const CpuProfileNode*>> stack;

  if (!options_.line_numbers) {
    stack.emplace_back(-1, root);
    while (!stack.empty()) {
      auto [parent, node] = stack.back();
      stack.pop_back();
      uint32_t hit_count = node->GetHitCount();
      int64_t cpu_time = 0;
      if (attribution != nullptr) {
        // Samples that matched no record were not taken by the signal handler
        // (e.g. deoptimization samples), so they are discarded.
        auto it = attribution->find(node);
        hit_count = it != attribution->end() ? it->second.hit_count : 0;
        cpu_time = it != attribution->end() ? it->second.cpu_time : 0;
      }
      int32_t row = add_row(parent,
                            node,
                            node->GetLineNumber(),
                            node->GetColumnNumber(),
                            hit_count,
                            cpu_time);
      for (int i = node->GetChildrenCount() - 1; i >= 0; i--) {
        stack.emplace_back(row, node->GetChild(i));
      }
    }
  } else {
    // In caller line numbers mode, the line of a node is the line from which
    // its function was called. Each node turns into one leaf per line of its
    // function that has hits, and one node per call it makes, at the line of
    // the call. The children of the root are flattened into it.
    add_row(-1, root, root->GetLineNumber(), root->GetColumnNumber(), 0, 0);
    for (int i = root->GetChildrenCount() - 1; i >= 0; i--) {
      stack.emplace_back(0, root->GetChild(i));
    }
    std::vector<CpuProfileNode::LineTick> line_ticks;
    while (!stack.empty()) {
      auto [parent, node] = stack.back();
      stack.pop_back();
      unsigned int hit_line_count = node->GetHitLineCount();
      if (hit_line_count > 0) {
        line_ticks.resize(hit_line_count);
        if (node->GetLineTicks(line_ticks.data(), hit_line_count)) {
          for (const CpuProfileNode::LineTick& tick : line_ticks) {
            add_row(parent, node, tick.line, tick.column, tick.hit_count, 0);
          }
        }
      } else if (node->GetHitCount() > 0) {
        // Pseudo-functions such as `(program)` and `(garbage collector)` have
        // no line ticks.
        add_row(parent,
                node,
                node->GetLineNumber(),
                node->GetColumnNumber(),
                node->GetHitCount(),
                0);
      }
      for (int i = node->GetChildrenCount() - 1; i >= 0; i--) {
        const CpuProfileNode* child = node->GetChild(i);
        int32_t row = add_row(parent,
                              node,
                              child->GetLineNumber(),
                              child->GetColumnNumber(),
                              0,
                              0);
        stack.emplace_back(row, child);
      }
    }
  }

  Local<Object> result = Object::New(isolate);
  auto set = [&](const char* key, Local<Value> value) {
    return result->Set(context, OneByteString(isolate, key), value).IsJust();
  };
  Local<Value> epoch_offset =
#ifdef _WIN32
      Undefined(isolate).As<Value>();
#else
      Number::New(isolate, static_cast<double>(v8_to_epoch_offset)).As<Value>();
#endif  // _WIN32
  Local<Value> cpu_time_column =
      attribution != nullptr
          ? NewTypedArray<double, Float64Array>(isolate, cpu_times).As<Value>()
          : Undefined(isolate).As<Value>();

  if (!set(
          "startTime",
          Number::New(isolate, static_cast<double>(profile->GetStartTime()))) ||
      !set("endTime",
           Number::New(isolate, static_cast<double>(profile->GetEndTime()))) ||
      !set("epochOffset", epoch_offset) ||
      !set("hasCpuTime", Boolean::New(isolate, attribution != nullptr)) ||
      !set(
          "nonJSThreadsCpuTime",
          Number::New(isolate, static_cast<double>(non_js_threads_cpu_time))) ||
      !set("totalHitCount", Number::New(isolate, total_hit_count)) ||
      !set("restarted", Boolean::New(isolate, restarted)) ||
      !set("parent", NewTypedArray<int32_t, Int32Array>(isolate, parents)) ||
      !set("scriptId",
           NewTypedArray<int32_t, Int32Array>(isolate, script_ids)) ||
      !set("lineNumber",
           NewTypedArray<int32_t, Int32Array>(isolate, line_numbers)) ||
      !set("columnNumber",
           NewTypedArray<int32_t, Int32Array>(isolate, column_numbers)) ||
      !set("hitCount",
           NewTypedArray<uint32_t, Uint32Array>(isolate, hit_counts)) ||
      !set("cpuTime", cpu_time_column) ||
      !set("name", v8::Array::New(isolate, names.data(), names.size())) ||
      !set("scriptName",
           v8::Array::New(isolate, script_names.data(), script_names.size()))) {
    return MaybeLocal<Object>();
  }
  return result;
}

void WallProfiler::DisposeProfiler() {
  SetCollectionMode(CollectionMode::kNoCollect);
#ifndef _WIN32
  if (SignalHandler::GetThreadSink() == this) {
    SignalHandler::SetThreadSink(nullptr);
  }
  // Restore the signal handler before V8 restores its own, when the profiler
  // is disposed below.
  while (signal_handler_uses_ > 0) {
    SignalHandler::DecreaseUseCount();
    signal_handler_uses_--;
  }
#endif  // _WIN32
  if (cpu_profiler_ != nullptr) {
    // Stop the running profile before disposing of the profiler, as
    // Environment does with its own, e.g. when the profiler is torn down at
    // exit without having been stopped.
    if (started_) {
      CpuProfile* profile = cpu_profiler_->Stop(profile_id_);
      if (profile != nullptr) profile->Delete();
    }
    cpu_profiler_->Dispose();
    cpu_profiler_ = nullptr;
    GetActiveProfilers().Remove(this);
  }
  if (thread_profiler == this) thread_profiler = nullptr;
  started_ = false;
}

void WallProfiler::MakeWeakIfStopped() {
  if (!started_ && !IsWeakOrDetached()) MakeWeak();
}

void WallProfiler::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsInt32());    // samplingIntervalMicros
  CHECK(args[1]->IsInt32());    // durationMillis
  CHECK(args[2]->IsBoolean());  // lineNumbers
  CHECK(args[3]->IsBoolean());  // workaroundV8Bug
  CHECK(args[4]->IsBoolean());  // collectCpuTime

  Options options;
  options.sampling_interval_us = args[0].As<v8::Int32>()->Value();
  options.duration_ms = args[1].As<v8::Int32>()->Value();
  options.line_numbers = args[2]->IsTrue();
  options.workaround_v8_bug = args[3]->IsTrue();
  options.collect_cpu_time = args[4]->IsTrue();
  CHECK_GT(options.sampling_interval_us, 0);
  CHECK_GT(options.duration_ms, 0);
#ifdef _WIN32
  // There is no signal to intercept.
  options.workaround_v8_bug = false;
  CHECK(!options.collect_cpu_time);
#endif  // _WIN32
  // Caller line numbers cannot be attributed CPU time: samples are associated
  // with profile nodes, not with line ticks.
  CHECK(!(options.collect_cpu_time && options.line_numbers));

  new WallProfiler(env, args.This(), options);
}

void WallProfiler::Start(const FunctionCallbackInfo<Value>& args) {
  WallProfiler* profiler;
  ASSIGN_OR_RETURN_UNWRAP(&profiler, args.This());
  Environment* env = profiler->env();

  if (profiler->started_) {
    return THROW_ERR_INVALID_STATE(env, "The profiler is already started");
  }
  if (thread_profiler != nullptr) {
    return THROW_ERR_INVALID_STATE(
        env, "Another time profiler is already running on this thread");
  }

  profiler->cpu_profiler_ = CpuProfiler::New(env->isolate());
  profiler->cpu_profiler_->SetSamplingInterval(
      profiler->options_.sampling_interval_us);
  thread_profiler = profiler;
  profiler->thread_cpu_stop_watch_.GetAndReset();
  GetActiveProfilers().Add(profiler);
#ifndef _WIN32
  // Used by the signal handler and to compute the profile's epoch offset.
  InitializeMonotonicClock();
  if (profiler->InterceptSignal()) {
    SignalHandler::SetThreadSink(profiler);
  }
#endif  // _WIN32

  profiler->profile_index_ = 0;
  profiler->v8_profiler_stuck_event_loop_detected_ = 0;
  profiler->records_.clear();
  // Keep the profiler alive while it runs.
  profiler->started_ = true;
  profiler->ClearWeak();

  switch (profiler->StartInternal()) {
    case StartStatus::kOk:
      break;
    case StartStatus::kTooManyProfiles:
      profiler->DisposeProfiler();
      profiler->MakeWeakIfStopped();
      return THROW_ERR_CPU_PROFILE_TOO_MANY(env,
                                            "There are too many CPU profiles");
    case StartStatus::kSignalHandlerFailed:
      profiler->DisposeProfiler();
      profiler->MakeWeakIfStopped();
      return THROW_ERR_INVALID_STATE(
          env, "Failed to install the SIGPROF signal handler");
  }

  profiler->SetCollectionMode(profiler->options_.collect_cpu_time
                                  ? CollectionMode::kCollect
                                  : (profiler->InterceptSignal()
                                         ? CollectionMode::kPassThrough
                                         : CollectionMode::kNoCollect));
}

void WallProfiler::Stop(const FunctionCallbackInfo<Value>& args) {
  WallProfiler* profiler;
  ASSIGN_OR_RETURN_UNWRAP(&profiler, args.This());
  CHECK(args[0]->IsBoolean());
  if (!profiler->started_) {
    return THROW_ERR_INVALID_STATE(profiler->env(),
                                   "The profiler is not started");
  }
  Local<Object> profile;
  bool translated = profiler->StopImpl(args[0]->IsTrue()).ToLocal(&profile);
  profiler->MakeWeakIfStopped();
  if (translated) args.GetReturnValue().Set(profile);
}

void WallProfiler::Dispose(const FunctionCallbackInfo<Value>& args) {
  WallProfiler* profiler;
  ASSIGN_OR_RETURN_UNWRAP(&profiler, args.This());
  profiler->DisposeProfiler();
  profiler->MakeWeakIfStopped();
}

void WallProfiler::V8ProfilerStuckEventLoopDetected(
    const FunctionCallbackInfo<Value>& args) {
  WallProfiler* profiler;
  ASSIGN_OR_RETURN_UNWRAP(&profiler, args.This());
  args.GetReturnValue().Set(profiler->v8_profiler_stuck_event_loop_detected_);
}

void WallProfiler::Initialize(Local<Object> target,
                              Local<Value> unused,
                              Local<Context> context,
                              void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tmpl = NewFunctionTemplate(isolate, New);
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      WallProfiler::kInternalFieldCount);
  SetProtoMethod(isolate, tmpl, "start", Start);
  SetProtoMethod(isolate, tmpl, "stop", Stop);
  SetProtoMethod(isolate, tmpl, "dispose", Dispose);
  SetProtoMethodNoSideEffect(isolate,
                             tmpl,
                             "v8ProfilerStuckEventLoopDetected",
                             V8ProfilerStuckEventLoopDetected);
  SetConstructorFunction(context, target, "WallProfiler", tmpl);

#ifdef _WIN32
  constexpr bool kSignalSamplingSupported = false;
#else
  constexpr bool kSignalSamplingSupported = true;
#endif  // _WIN32
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "signalSamplingSupported"),
            Boolean::New(isolate, kSignalSamplingSupported))
      .Check();
}

void WallProfiler::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Start);
  registry->Register(Stop);
  registry->Register(Dispose);
  registry->Register(V8ProfilerStuckEventLoopDetected);
}

}  // namespace pprof
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(pprof,
                                    node::pprof::WallProfiler::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    pprof, node::pprof::WallProfiler::RegisterExternalReferences)
