/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/profiling/perf/unwinding.h"

#include <mutex>

#include <inttypes.h>

#include "perfetto/ext/base/metatrace.h"
#include "perfetto/ext/base/thread_utils.h"

namespace {
constexpr size_t kUnwindingMaxFrames = 1000;
constexpr uint32_t kDataSourceShutdownRetryDelayMs = 400;

void MaybeReleaseAllocatorMemToOS() {
#if defined(__BIONIC__)
  // TODO(b/152414415): libunwindstack's volume of small allocations is
  // adverarial to scudo, which doesn't automatically release small
  // allocation regions back to the OS. Forceful purge does reclaim all size
  // classes.
  mallopt(M_PURGE, 0);
#endif
}

}  // namespace

namespace perfetto {
namespace profiling {

Unwinder::Delegate::~Delegate() = default;

Unwinder::Unwinder(Delegate* delegate, base::UnixTaskRunner* task_runner)
    : task_runner_(task_runner), delegate_(delegate) {
  ResetAndEnableUnwindstackCache();
  base::MaybeSetThreadName("stack-unwinding");
}

void Unwinder::PostStartDataSource(DataSourceInstanceID ds_id) {
  // No need for a weak pointer as the associated task runner quits (stops
  // running tasks) strictly before the Unwinder's destruction.
  task_runner_->PostTask([this, ds_id] { StartDataSource(ds_id); });
}

void Unwinder::StartDataSource(DataSourceInstanceID ds_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Unwinder::StartDataSource(%zu)", static_cast<size_t>(ds_id));

  auto it_and_inserted = data_sources_.emplace(ds_id, DataSourceState{});
  PERFETTO_DCHECK(it_and_inserted.second);
}

// c++11: use shared_ptr to transfer resource handles, so that the resources get
// released even if the task runner is destroyed with pending tasks.
// "Cleverness" warning:
// the task will be executed on a different thread, and will mutate the
// pointed-to memory. It may be the case that this posting thread will not
// decrement its shared_ptr refcount until *after* the task has executed. In
// that scenario, the destruction of the pointed-to memory will be happening on
// the posting thread. This implies a data race between the mutation on the task
// thread, and the destruction on the posting thread. *However*, we assume that
// there is no race in practice due to refcount decrements having
// release-acquire semantics. The refcount decrements pair with each other, and
// therefore also serve as a memory barrier between the destructor, and any
// previous modifications of the pointed-to memory.
// TODO(rsavitski): present a more convincing argument, or reimplement
// without relying on shared_ptr implementation details.
void Unwinder::PostAdoptProcDescriptors(DataSourceInstanceID ds_id,
                                        pid_t pid,
                                        base::ScopedFile maps_fd,
                                        base::ScopedFile mem_fd) {
  auto shared_maps = std::make_shared<base::ScopedFile>(std::move(maps_fd));
  auto shared_mem = std::make_shared<base::ScopedFile>(std::move(mem_fd));
  task_runner_->PostTask([this, ds_id, pid, shared_maps, shared_mem] {
    base::ScopedFile maps = std::move(*shared_maps.get());
    base::ScopedFile mem = std::move(*shared_mem.get());
    AdoptProcDescriptors(ds_id, pid, std::move(maps), std::move(mem));
  });
}

void Unwinder::AdoptProcDescriptors(DataSourceInstanceID ds_id,
                                    pid_t pid,
                                    base::ScopedFile maps_fd,
                                    base::ScopedFile mem_fd) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Unwinder::AdoptProcDescriptors(%zu, %d, %d, %d)",
                static_cast<size_t>(ds_id), static_cast<int>(pid),
                maps_fd.get(), mem_fd.get());

  auto it = data_sources_.find(ds_id);
  PERFETTO_CHECK(it != data_sources_.end());
  DataSourceState& ds = it->second;

  ProcessState& proc_state = ds.process_states[pid];  // insert if new
  PERFETTO_DCHECK(proc_state.status != ProcessState::Status::kResolved);
  PERFETTO_DCHECK(!proc_state.unwind_state.has_value());

  PERFETTO_METATRACE_SCOPED(TAG_PRODUCER, PROFILER_MAPS_PARSE);

  proc_state.status = ProcessState::Status::kResolved;
  proc_state.unwind_state =
      UnwindingMetadata{std::move(maps_fd), std::move(mem_fd)};
}

void Unwinder::PostRecordTimedOutProcDescriptors(DataSourceInstanceID ds_id,
                                                 pid_t pid) {
  task_runner_->PostTask(
      [this, ds_id, pid] { RecordTimedOutProcDescriptors(ds_id, pid); });
}

void Unwinder::RecordTimedOutProcDescriptors(DataSourceInstanceID ds_id,
                                             pid_t pid) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Unwinder::RecordTimedOutProcDescriptors(%zu, %d)",
                static_cast<size_t>(ds_id), static_cast<int>(pid));

  auto it = data_sources_.find(ds_id);
  PERFETTO_CHECK(it != data_sources_.end());
  DataSourceState& ds = it->second;

  ProcessState& proc_state = ds.process_states[pid];  // insert if new
  PERFETTO_DCHECK(proc_state.status == ProcessState::Status::kResolving);
  PERFETTO_DCHECK(!proc_state.unwind_state.has_value());

  proc_state.status = ProcessState::Status::kExpired;
}

void Unwinder::PostProcessQueue() {
  task_runner_->PostTask([this] { ProcessQueue(); });
}

// Note: we always walk the queue in order. So if there are multiple data
// sources, one of which is shutting down, its shutdown can be delayed by
// unwinding of other sources' samples. Instead, we could scan the queue
// multiple times, prioritizing the samples for shutting-down sources. At the
// time of writing, the earlier is considered to be fair enough.
void Unwinder::ProcessQueue() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_METATRACE_SCOPED(TAG_PRODUCER, PROFILER_UNWIND_TICK);
  PERFETTO_DLOG("Unwinder::ProcessQueue");

  base::FlatSet<DataSourceInstanceID> pending_sample_sources =
      ConsumeAndUnwindReadySamples();

  // Deal with the possiblity of data sources that are shutting down.
  bool post_delayed_reprocess = false;
  base::FlatSet<DataSourceInstanceID> sources_to_stop;
  for (auto& id_and_ds : data_sources_) {
    DataSourceInstanceID ds_id = id_and_ds.first;
    const DataSourceState& ds = id_and_ds.second;

    if (ds.status == DataSourceState::Status::kActive)
      continue;

    // Data source that is shutting down. If we're still waiting on proc-fds (or
    // the lookup to time out) for samples in the queue - repost a later
    // attempt (as there is no guarantee that there are any readers waking up
    // the unwinder anymore).
    if (pending_sample_sources.count(ds_id)) {
      PERFETTO_DLOG(
          "Unwinder delaying DS(%zu) stop: waiting on a pending sample",
          static_cast<size_t>(ds_id));
      post_delayed_reprocess = true;
    } else {
      // Otherwise, proceed with tearing down data source state (after
      // completing the loop, to avoid invalidating the iterator).
      sources_to_stop.insert(ds_id);
    }
  }

  for (auto ds_id : sources_to_stop)
    FinishDataSourceStop(ds_id);

  if (post_delayed_reprocess)
    task_runner_->PostDelayedTask([this] { ProcessQueue(); },
                                  kDataSourceShutdownRetryDelayMs);
}

base::FlatSet<DataSourceInstanceID> Unwinder::ConsumeAndUnwindReadySamples() {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  base::FlatSet<DataSourceInstanceID> pending_sample_sources;

  // Use a single snapshot of the ring buffer pointers.
  ReadView read_view = unwind_queue_.BeginRead();

  PERFETTO_METATRACE_COUNTER(
      TAG_PRODUCER, PROFILER_UNWIND_QUEUE_SZ,
      static_cast<int32_t>(read_view.write_pos - read_view.read_pos));

  if (read_view.read_pos == read_view.write_pos)
    return pending_sample_sources;

  // Walk the queue.
  for (auto read_pos = read_view.read_pos; read_pos < read_view.write_pos;
       read_pos++) {
    UnwindEntry& entry = unwind_queue_.at(read_pos);

    if (!entry.valid)
      continue;  // already processed

    auto it = data_sources_.find(entry.data_source_id);
    PERFETTO_CHECK(it != data_sources_.end());
    DataSourceState& ds = it->second;

    pid_t pid = entry.sample.pid;
    ProcessState& proc_state = ds.process_states[pid];  // insert if new

    // Giving up on the sample (proc-fd lookup timed out).
    if (proc_state.status == ProcessState::Status::kExpired) {
      PERFETTO_DLOG("Unwinder skipping sample for pid [%d]",
                    static_cast<int>(pid));

      delegate_->PostEmitUnwinderSkippedSample(entry.data_source_id,
                                               std::move(entry.sample));
      entry = UnwindEntry::Invalid();
      continue;
    }

    // Still waiting on the proc-fds.
    if (proc_state.status == ProcessState::Status::kResolving) {
      PERFETTO_DLOG("Unwinder deferring sample for pid [%d]",
                    static_cast<int>(pid));

      pending_sample_sources.insert(entry.data_source_id);
      continue;
    }

    // Sample ready - process it.
    if (proc_state.status == ProcessState::Status::kResolved) {
      // Metatrace: emit both a scoped slice, as well as a "counter"
      // representing the pid being unwound.
      PERFETTO_METATRACE_SCOPED(TAG_PRODUCER, PROFILER_UNWIND_SAMPLE);
      PERFETTO_METATRACE_COUNTER(TAG_PRODUCER, PROFILER_UNWIND_CURRENT_PID,
                                 static_cast<int32_t>(pid));

      PERFETTO_CHECK(proc_state.unwind_state.has_value());
      CompletedSample unwound_sample =
          UnwindSample(entry.sample, &proc_state.unwind_state.value(),
                       proc_state.attempted_unwinding);
      proc_state.attempted_unwinding = true;

      PERFETTO_METATRACE_COUNTER(TAG_PRODUCER, PROFILER_UNWIND_CURRENT_PID, 0);

      delegate_->PostEmitSample(entry.data_source_id,
                                std::move(unwound_sample));
      entry = UnwindEntry::Invalid();
      continue;
    }
  }

  // Consume all leading processed entries in the queue.
  auto new_read_pos = read_view.read_pos;
  for (; new_read_pos < read_view.write_pos; new_read_pos++) {
    UnwindEntry& entry = unwind_queue_.at(new_read_pos);
    if (entry.valid)
      break;
  }
  if (new_read_pos != read_view.read_pos)
    unwind_queue_.CommitNewReadPosition(new_read_pos);

  PERFETTO_METATRACE_COUNTER(
      TAG_PRODUCER, PROFILER_UNWIND_QUEUE_SZ,
      static_cast<int32_t>(read_view.write_pos - new_read_pos));

  PERFETTO_DLOG("Unwind queue drain: [%" PRIu64 "]->[%" PRIu64 "]",
                read_view.write_pos - read_view.read_pos,
                read_view.write_pos - new_read_pos);

  return pending_sample_sources;
}

CompletedSample Unwinder::UnwindSample(const ParsedSample& sample,
                                       UnwindingMetadata* unwind_state,
                                       bool pid_unwound_before) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DCHECK(unwind_state);

  CompletedSample ret;
  ret.cpu = sample.cpu;
  ret.pid = sample.pid;
  ret.tid = sample.tid;
  ret.timestamp = sample.timestamp;
  ret.cpu_mode = sample.cpu_mode;

  // Overlay the stack bytes over /proc/<pid>/mem.
  std::shared_ptr<unwindstack::Memory> overlay_memory =
      std::make_shared<StackOverlayMemory>(
          unwind_state->fd_mem, sample.regs->sp(),
          reinterpret_cast<const uint8_t*>(sample.stack.data()),
          sample.stack.size());

  // Unwindstack clobbers registers, so make a copy in case we need to retry.
  auto regs_copy = std::unique_ptr<unwindstack::Regs>{sample.regs->Clone()};

  unwindstack::ErrorCode error_code = unwindstack::ERROR_NONE;
  unwindstack::Unwinder unwinder(kUnwindingMaxFrames, &unwind_state->fd_maps,
                                 regs_copy.get(), overlay_memory);

  // TODO(rsavitski): consider rate-limiting unwind retries.
  for (int attempt = 0; attempt < 2; attempt++) {
    metatrace::ScopedEvent m(metatrace::TAG_PRODUCER,
                             pid_unwound_before
                                 ? metatrace::PROFILER_UNWIND_ATTEMPT
                                 : metatrace::PROFILER_UNWIND_INITIAL_ATTEMPT);

#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
    unwinder.SetJitDebug(unwind_state->jit_debug.get(), regs_copy->Arch());
    unwinder.SetDexFiles(unwind_state->dex_files.get(), regs_copy->Arch());
#endif
    unwinder.Unwind(/*initial_map_names_to_skip=*/nullptr,
                    /*map_suffixes_to_ignore=*/nullptr);
    error_code = unwinder.LastErrorCode();
    if (error_code != unwindstack::ERROR_INVALID_MAP)
      break;

    // Otherwise, reparse the maps, and possibly retry the unwind.
    PERFETTO_DLOG("Reparsing maps for pid [%d]", static_cast<int>(sample.pid));
    PERFETTO_METATRACE_SCOPED(TAG_PRODUCER, PROFILER_MAPS_REPARSE);
    unwind_state->ReparseMaps();
  }

  PERFETTO_DLOG("Frames from unwindstack for pid [%d]:",
                static_cast<int>(sample.pid));
  std::vector<unwindstack::FrameData> frames = unwinder.ConsumeFrames();
  ret.frames.reserve(frames.size());
  for (unwindstack::FrameData& frame : frames) {
    if (PERFETTO_DLOG_IS_ON())
      PERFETTO_DLOG("%s", unwinder.FormatFrame(frame).c_str());

    ret.frames.emplace_back(unwind_state->AnnotateFrame(std::move(frame)));
  }

  // In case of an unwinding error, add a synthetic error frame (which will
  // appear as a caller of the partially-unwound fragment), for easier
  // visualization of errors.
  if (error_code != unwindstack::ERROR_NONE) {
    PERFETTO_DLOG("Unwinding error %" PRIu8, error_code);
    unwindstack::FrameData frame_data{};
    frame_data.function_name =
        "ERROR " + StringifyLibUnwindstackError(error_code);
    frame_data.map_name = "ERROR";
    ret.frames.emplace_back(std::move(frame_data), /*build_id=*/"");
    ret.unwind_error = error_code;
  }

  return ret;
}

void Unwinder::PostInitiateDataSourceStop(DataSourceInstanceID ds_id) {
  task_runner_->PostTask([this, ds_id] { InitiateDataSourceStop(ds_id); });
}

void Unwinder::InitiateDataSourceStop(DataSourceInstanceID ds_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Unwinder::InitiateDataSourceStop(%zu)",
                static_cast<size_t>(ds_id));

  auto it = data_sources_.find(ds_id);
  PERFETTO_CHECK(it != data_sources_.end());
  DataSourceState& ds = it->second;

  PERFETTO_CHECK(ds.status == DataSourceState::Status::kActive);
  ds.status = DataSourceState::Status::kShuttingDown;

  // Make sure that there's an outstanding task to process the unwinding queue,
  // as it is the point that evaluates the stop condition.
  PostProcessQueue();
}

void Unwinder::FinishDataSourceStop(DataSourceInstanceID ds_id) {
  PERFETTO_DCHECK_THREAD(thread_checker_);
  PERFETTO_DLOG("Unwinder::FinishDataSourceStop(%zu)",
                static_cast<size_t>(ds_id));

  auto it = data_sources_.find(ds_id);
  PERFETTO_CHECK(it != data_sources_.end());
  DataSourceState& ds = it->second;

  // Drop unwinder's state tied to the source.
  PERFETTO_CHECK(ds.status == DataSourceState::Status::kShuttingDown);
  data_sources_.erase(it);

  // Clean up state if there are no more active sources.
  if (data_sources_.empty())
    ResetAndEnableUnwindstackCache();

  // Inform service thread that the unwinder is done with the source.
  delegate_->PostFinishDataSourceStop(ds_id);
}

void Unwinder::PostClearCachedStatePeriodic(DataSourceInstanceID ds_id,
                                            uint32_t period_ms) {
  task_runner_->PostDelayedTask(
      [this, ds_id, period_ms] { ClearCachedStatePeriodic(ds_id, period_ms); },
      period_ms);
}

// See header for rationale.
void Unwinder::ClearCachedStatePeriodic(DataSourceInstanceID ds_id,
                                        uint32_t period_ms) {
  auto it = data_sources_.find(ds_id);
  if (it == data_sources_.end())
    return;  // stop the periodic task

  DataSourceState& ds = it->second;
  if (ds.status != DataSourceState::Status::kActive)
    return;

  PERFETTO_METATRACE_SCOPED(TAG_PRODUCER, PROFILER_UNWIND_CACHE_CLEAR);
  PERFETTO_DLOG("Clearing unwinder's cached state.");

  for (auto& pid_and_process : ds.process_states) {
    pid_and_process.second.unwind_state->fd_maps.Reset();
  }
  ResetAndEnableUnwindstackCache();
  MaybeReleaseAllocatorMemToOS();

  PostClearCachedStatePeriodic(ds_id, period_ms);  // repost
}

void Unwinder::ResetAndEnableUnwindstackCache() {
  PERFETTO_DLOG("Resetting unwindstack cache");
  // Libunwindstack uses an unsynchronized variable for setting/checking whether
  // the cache is enabled. Therefore unwinding and cache toggling should stay on
  // the same thread, but we might be moving unwinding across threads if we're
  // recreating |Unwinder| instances (during a reconnect to traced). Therefore,
  // use our own static lock to synchronize the cache toggling.
  // TODO(rsavitski): consider fixing this in libunwindstack itself.
  static std::mutex* lock = new std::mutex{};
  std::lock_guard<std::mutex> guard{*lock};
  unwindstack::Elf::SetCachingEnabled(false);  // free any existing state
  unwindstack::Elf::SetCachingEnabled(true);   // reallocate a fresh cache
}

}  // namespace profiling
}  // namespace perfetto
