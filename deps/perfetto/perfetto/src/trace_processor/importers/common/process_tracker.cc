/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/storage/stats.h"

#include <utility>

#include <inttypes.h>

namespace perfetto {
namespace trace_processor {

ProcessTracker::ProcessTracker(TraceProcessorContext* context)
    : context_(context) {}

ProcessTracker::~ProcessTracker() = default;

UniqueTid ProcessTracker::StartNewThread(base::Optional<int64_t> timestamp,
                                         uint32_t tid,
                                         StringId thread_name_id) {
  tables::ThreadTable::Row row;
  row.tid = tid;
  row.name = thread_name_id;
  row.start_ts = timestamp;

  auto* thread_table = context_->storage->mutable_thread_table();
  UniqueTid new_utid = thread_table->Insert(row).row;
  tids_[tid].emplace_back(new_utid);
  return new_utid;
}

void ProcessTracker::EndThread(int64_t timestamp, uint32_t tid) {
  auto* thread_table = context_->storage->mutable_thread_table();
  auto* process_table = context_->storage->mutable_process_table();

  UniqueTid utid = GetOrCreateThread(tid);
  thread_table->mutable_end_ts()->Set(utid, timestamp);

  // Remove the thread from the list of threads being tracked as any event after
  // this one should be ignored.
  auto& vector = tids_[tid];
  vector.erase(std::remove(vector.begin(), vector.end(), utid));

  auto opt_upid = thread_table->upid()[utid];
  if (opt_upid.has_value()) {
    // If the process pid and thread tid are equal, then this is the main thread
    // of the process.
    if (process_table->pid()[*opt_upid] == thread_table->tid()[utid]) {
      process_table->mutable_end_ts()->Set(*opt_upid, timestamp);
    }
  }
}

base::Optional<UniqueTid> ProcessTracker::GetThreadOrNull(uint32_t tid) {
  auto opt_utid = GetThreadOrNull(tid, base::nullopt);
  if (!opt_utid)
    return base::nullopt;

  auto* threads = context_->storage->mutable_thread_table();
  UniqueTid utid = *opt_utid;

  // Ensure that the tid matches the tid we were looking for.
  PERFETTO_DCHECK(threads->tid()[utid] == tid);

  // If the thread is being tracked by the process tracker, it should not be
  // known to have ended.
  PERFETTO_DCHECK(!threads->end_ts()[utid].has_value());

  return utid;
}

UniqueTid ProcessTracker::GetOrCreateThread(uint32_t tid) {
  auto utid = GetThreadOrNull(tid);
  return utid ? *utid : StartNewThread(base::nullopt, tid, kNullStringId);
}

UniqueTid ProcessTracker::UpdateThreadName(uint32_t tid,
                                           StringId thread_name_id) {
  auto* thread_table = context_->storage->mutable_thread_table();
  auto utid = GetOrCreateThread(tid);
  if (!thread_name_id.is_null())
    thread_table->mutable_name()->Set(utid, thread_name_id);
  return utid;
}

void ProcessTracker::SetThreadNameIfUnset(UniqueTid utid,
                                          StringId thread_name_id) {
  auto* thread_table = context_->storage->mutable_thread_table();
  if (thread_table->name()[utid].is_null())
    thread_table->mutable_name()->Set(utid, thread_name_id);
}

bool ProcessTracker::IsThreadAlive(UniqueTid utid) {
  auto* threads = context_->storage->mutable_thread_table();
  auto* processes = context_->storage->mutable_process_table();

  // If the thread has an end ts, it's certainly dead.
  if (threads->end_ts()[utid].has_value())
    return false;

  // If we don't know the parent process, we have to consider this thread alive.
  auto opt_current_upid = threads->upid()[utid];
  if (!opt_current_upid)
    return true;

  // If the process is already dead, the thread can't be alive.
  UniquePid current_upid = *opt_current_upid;
  if (processes->end_ts()[current_upid].has_value())
    return false;

  // If the process has been replaced in |pids_|, this thread is dead.
  uint32_t current_pid = processes->pid()[current_upid];
  auto pid_it = pids_.find(current_pid);
  if (pid_it != pids_.end() && pid_it->second != current_upid)
    return false;

  return true;
}

base::Optional<UniqueTid> ProcessTracker::GetThreadOrNull(
    uint32_t tid,
    base::Optional<uint32_t> pid) {
  auto* threads = context_->storage->mutable_thread_table();
  auto* processes = context_->storage->mutable_process_table();

  auto vector_it = tids_.find(tid);
  if (vector_it == tids_.end())
    return base::nullopt;

  // Iterate backwards through the threads so ones later in the trace are more
  // likely to be picked.
  const auto& vector = vector_it->second;
  for (auto it = vector.rbegin(); it != vector.rend(); it++) {
    UniqueTid current_utid = *it;

    // If we finished this thread, we should have removed it from the vector
    // entirely.
    PERFETTO_DCHECK(!threads->end_ts()[current_utid].has_value());

    // If the thread is dead, ignore it.
    if (!IsThreadAlive(current_utid))
      continue;

    // If we don't know the parent process, we have to choose this thread.
    auto opt_current_upid = threads->upid()[current_utid];
    if (!opt_current_upid)
      return current_utid;

    // We found a thread that matches both the tid and its parent pid.
    uint32_t current_pid = processes->pid()[*opt_current_upid];
    if (!pid || current_pid == *pid)
      return current_utid;
  }
  return base::nullopt;
}

UniqueTid ProcessTracker::UpdateThread(uint32_t tid, uint32_t pid) {
  auto* thread_table = context_->storage->mutable_thread_table();

  // Try looking for a thread that matches both tid and thread group id (pid).
  base::Optional<UniqueTid> opt_utid = GetThreadOrNull(tid, pid);

  // If no matching thread was found, create a new one.
  UniqueTid utid =
      opt_utid ? *opt_utid : StartNewThread(base::nullopt, tid, kNullStringId);
  PERFETTO_DCHECK(thread_table->tid()[utid] == tid);

  // Find matching process or create new one.
  if (!thread_table->upid()[utid].has_value()) {
    thread_table->mutable_upid()->Set(utid, GetOrCreateProcess(pid));
  }

  ResolvePendingAssociations(utid, *thread_table->upid()[utid]);

  return utid;
}

UniquePid ProcessTracker::StartNewProcess(base::Optional<int64_t> timestamp,
                                          base::Optional<uint32_t> parent_tid,
                                          uint32_t pid,
                                          StringId main_thread_name) {
  pids_.erase(pid);
  // TODO(eseckler): Consider erasing all old entries in |tids_| that match the
  // |pid| (those would be for an older process with the same pid). Right now,
  // we keep them in |tids_| (if they weren't erased by EndThread()), but ignore
  // them in GetThreadOrNull().

  // Create a new UTID for the main thread, so we don't end up reusing an old
  // entry in case of TID recycling.
  StartNewThread(timestamp, /*tid=*/pid, kNullStringId);

  // Note that we erased the pid above so this should always return a new
  // process.
  UniquePid upid = GetOrCreateProcess(pid);

  auto* process_table = context_->storage->mutable_process_table();
  auto* thread_table = context_->storage->mutable_thread_table();

  PERFETTO_DCHECK(process_table->name()[upid].is_null());
  PERFETTO_DCHECK(!process_table->start_ts()[upid].has_value());

  if (timestamp) {
    process_table->mutable_start_ts()->Set(upid, *timestamp);
  }
  process_table->mutable_name()->Set(upid, main_thread_name);

  if (parent_tid) {
    UniqueTid parent_utid = GetOrCreateThread(*parent_tid);
    auto opt_parent_upid = thread_table->upid()[parent_utid];
    if (opt_parent_upid.has_value()) {
      process_table->mutable_parent_upid()->Set(upid, *opt_parent_upid);
    } else {
      pending_parent_assocs_.emplace_back(parent_utid, upid);
    }
  }
  return upid;
}

UniquePid ProcessTracker::SetProcessMetadata(uint32_t pid,
                                             base::Optional<uint32_t> ppid,
                                             base::StringView name) {
  auto proc_name_id = context_->storage->InternString(name);

  base::Optional<UniquePid> pupid;
  if (ppid.has_value()) {
    pupid = GetOrCreateProcess(ppid.value());
  }

  UniquePid upid = GetOrCreateProcess(pid);

  auto* process_table = context_->storage->mutable_process_table();
  process_table->mutable_name()->Set(upid, proc_name_id);

  if (pupid)
    process_table->mutable_parent_upid()->Set(upid, *pupid);

  return upid;
}

void ProcessTracker::SetProcessUid(UniquePid upid, uint32_t uid) {
  auto* process_table = context_->storage->mutable_process_table();
  process_table->mutable_uid()->Set(upid, uid);

  // The notion of the app ID (as derived from the uid) is defined in
  // frameworks/base/core/java/android/os/UserHandle.java
  process_table->mutable_android_appid()->Set(upid, uid % 100000);
}

void ProcessTracker::SetProcessNameIfUnset(UniquePid upid,
                                           StringId process_name_id) {
  auto* process_table = context_->storage->mutable_process_table();
  if (process_table->name()[upid].is_null())
    process_table->mutable_name()->Set(upid, process_name_id);
}

void ProcessTracker::UpdateProcessNameFromThreadName(uint32_t tid,
                                                     StringId thread_name) {
  auto* thread_table = context_->storage->mutable_thread_table();
  auto* process_table = context_->storage->mutable_process_table();

  auto utid = GetOrCreateThread(tid);
  auto opt_upid = thread_table->upid()[utid];
  if (opt_upid.has_value()) {
    if (process_table->pid()[*opt_upid] == tid) {
      process_table->mutable_name()->Set(*opt_upid, thread_name);
    }
  }
}

UniquePid ProcessTracker::GetOrCreateProcess(uint32_t pid) {
  UniquePid upid;
  auto it = pids_.find(pid);
  if (it != pids_.end()) {
    upid = it->second;
  } else {
    tables::ProcessTable::Row row;
    row.pid = pid;
    upid = context_->storage->mutable_process_table()->Insert(row).row;

    pids_.emplace(pid, upid);

    // Create an entry for the main thread.
    // We cannot call StartNewThread() here, because threads for this process
    // (including the main thread) might have been seen already prior to this
    // call. This call usually comes from the ProcessTree dump which is delayed.
    UpdateThread(/*tid=*/pid, pid);
  }
  return upid;
}

void ProcessTracker::AssociateThreads(UniqueTid utid1, UniqueTid utid2) {
  auto* tt = context_->storage->mutable_thread_table();

  // First of all check if one of the two threads is already bound to a process.
  // If that is the case, map the other thread to the same process and resolve
  // recursively any associations pending on the other thread.

  auto opt_upid1 = tt->upid()[utid1];
  auto opt_upid2 = tt->upid()[utid2];

  if (opt_upid1.has_value() && !opt_upid2.has_value()) {
    tt->mutable_upid()->Set(utid2, *opt_upid1);
    ResolvePendingAssociations(utid2, *opt_upid1);
    return;
  }

  if (opt_upid2.has_value() && !opt_upid1.has_value()) {
    tt->mutable_upid()->Set(utid1, *opt_upid2);
    ResolvePendingAssociations(utid1, *opt_upid2);
    return;
  }

  if (opt_upid1.has_value() && opt_upid1 != opt_upid2) {
    // Cannot associate two threads that belong to two different processes.
    PERFETTO_ELOG("Process tracker failure. Cannot associate threads %u, %u",
                  tt->tid()[utid1], tt->tid()[utid2]);
    context_->storage->IncrementStats(stats::process_tracker_errors);
    return;
  }

  pending_assocs_.emplace_back(utid1, utid2);
}

void ProcessTracker::ResolvePendingAssociations(UniqueTid utid_arg,
                                                UniquePid upid) {
  auto* tt = context_->storage->mutable_thread_table();
  auto* pt = context_->storage->mutable_process_table();
  PERFETTO_DCHECK(tt->upid()[utid_arg] == upid);

  std::vector<UniqueTid> resolved_utids;
  resolved_utids.emplace_back(utid_arg);

  while (!resolved_utids.empty()) {
    UniqueTid utid = resolved_utids.back();
    resolved_utids.pop_back();
    for (auto it = pending_parent_assocs_.begin();
         it != pending_parent_assocs_.end();) {
      UniqueTid parent_utid = it->first;
      UniquePid child_upid = it->second;

      if (parent_utid != utid) {
        ++it;
        continue;
      }
      PERFETTO_DCHECK(child_upid != upid);

      // Set the parent pid of the other process
      PERFETTO_DCHECK(!pt->parent_upid()[child_upid] ||
                      pt->parent_upid()[child_upid] == upid);
      pt->mutable_parent_upid()->Set(child_upid, upid);

      // Erase the pair. The |pending_parent_assocs_| vector is not sorted and
      // swapping a std::pair<uint32_t, uint32_t> is cheap.
      std::swap(*it, pending_parent_assocs_.back());
      pending_parent_assocs_.pop_back();
    }

    for (auto it = pending_assocs_.begin(); it != pending_assocs_.end();) {
      UniqueTid other_utid;
      if (it->first == utid) {
        other_utid = it->second;
      } else if (it->second == utid) {
        other_utid = it->first;
      } else {
        ++it;
        continue;
      }

      PERFETTO_DCHECK(other_utid != utid);

      // Update the other thread and associated it to the same process.
      PERFETTO_DCHECK(!tt->upid()[other_utid] ||
                      tt->upid()[other_utid] == upid);
      tt->mutable_upid()->Set(other_utid, upid);

      // Erase the pair. The |pending_assocs_| vector is not sorted and swapping
      // a std::pair<uint32_t, uint32_t> is cheap.
      std::swap(*it, pending_assocs_.back());
      pending_assocs_.pop_back();

      // Recurse into the newly resolved thread. Some other threads might have
      // been bound to that.
      resolved_utids.emplace_back(other_utid);
    }
  }  // while (!resolved_utids.empty())
}

void ProcessTracker::SetPidZeroIgnoredForIdleProcess() {
  // Create a mapping from (t|p)id 0 -> u(t|p)id 0 for the idle process.
  tids_.emplace(0, std::vector<UniqueTid>{0});
  pids_.emplace(0, 0);
}

}  // namespace trace_processor
}  // namespace perfetto
