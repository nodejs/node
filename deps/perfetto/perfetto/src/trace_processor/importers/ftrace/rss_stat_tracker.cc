/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "src/trace_processor/importers/ftrace/rss_stat_tracker.h"

#include "src/trace_processor/importers/common/event_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/ftrace/kmem.pbzero.h"

namespace perfetto {
namespace trace_processor {

RssStatTracker::RssStatTracker(TraceProcessorContext* context)
    : context_(context) {
  rss_members_.emplace_back(context->storage->InternString("mem.rss.file"));
  rss_members_.emplace_back(context->storage->InternString("mem.rss.anon"));
  rss_members_.emplace_back(context->storage->InternString("mem.swap"));
  rss_members_.emplace_back(context->storage->InternString("mem.rss.shmem"));
  rss_members_.emplace_back(
      context->storage->InternString("mem.rss.unknown"));  // Keep this last.
}

void RssStatTracker::ParseRssStat(int64_t ts, uint32_t pid, ConstBytes blob) {
  protos::pbzero::RssStatFtraceEvent::Decoder rss(blob.data, blob.size);
  const auto kRssStatUnknown = static_cast<uint32_t>(rss_members_.size()) - 1;
  auto member = static_cast<uint32_t>(rss.member());
  int64_t size = rss.size();
  if (member >= rss_members_.size()) {
    context_->storage->IncrementStats(stats::rss_stat_unknown_keys);
    member = kRssStatUnknown;
  }

  if (size < 0) {
    context_->storage->IncrementStats(stats::rss_stat_negative_size);
    return;
  }

  base::Optional<UniqueTid> utid;
  if (rss.has_mm_id()) {
    PERFETTO_DCHECK(rss.has_curr());
    utid = FindUtidForMmId(rss.mm_id(), rss.curr(), pid);
  } else {
    utid = context_->process_tracker->GetOrCreateThread(pid);
  }

  if (utid) {
    context_->event_tracker->PushProcessCounterForThread(
        ts, size, rss_members_[member], *utid);
  } else {
    context_->storage->IncrementStats(stats::rss_stat_unknown_thread_for_mm_id);
  }
}

base::Optional<UniqueTid> RssStatTracker::FindUtidForMmId(int64_t mm_id,
                                                          bool is_curr,
                                                          uint32_t pid) {
  // If curr is true, we can just overwrite the state in the map and return
  // the utid correspodning to |pid|.
  if (is_curr) {
    UniqueTid utid = context_->process_tracker->GetOrCreateThread(pid);
    mm_id_to_utid_[mm_id] = utid;
    return utid;
  }

  // If curr is false, try and lookup the utid we previously saw for this
  // mm id.
  auto it = mm_id_to_utid_.find(mm_id);
  if (it == mm_id_to_utid_.end())
    return base::nullopt;

  // If the utid in the map is the same as our current utid but curr is false,
  // that means we are in the middle of a process changing mm structs (i.e. in
  // the middle of a vfork + exec). Therefore, we should discard the association
  // of this vm struct with this thread.
  UniqueTid utid = context_->process_tracker->GetOrCreateThread(pid);
  if (it->second == utid) {
    mm_id_to_utid_.erase(it);
    return base::nullopt;
  }

  // Verify that the utid in the map is still alive. This can happen if an mm
  // struct we saw in the past is about to be reused after thread but we don't
  // know the new process that struct will be associated with.
  if (!context_->process_tracker->IsThreadAlive(it->second)) {
    mm_id_to_utid_.erase(it);
    return base::nullopt;
  }

  // This case happens when a process is changing the VM of another process and
  // we know that the utid corresponding to the target process. Just return that
  // utid.
  return it->second;
}

}  // namespace trace_processor
}  // namespace perfetto
