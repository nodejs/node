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

#include "src/trace_processor/storage/trace_storage.h"

#include <string.h>
#include <algorithm>
#include <limits>

#include "perfetto/ext/base/no_destructor.h"

namespace perfetto {
namespace trace_processor {

namespace {

void DbTableMaybeUpdateMinMax(const TypedColumn<int64_t>& ts_col,
                              int64_t* min_value,
                              int64_t* max_value,
                              const TypedColumn<int64_t>* dur_col = nullptr) {
  if (ts_col.row_map().empty())
    return;

  int64_t col_min = ts_col.Min()->AsLong();
  int64_t col_max = ts_col.Max()->AsLong();

  if (dur_col) {
    PERFETTO_CHECK(ts_col.IsSorted());
    PERFETTO_CHECK(dur_col->row_map().size() == ts_col.row_map().size());
    for (uint32_t i = 0; i < dur_col->row_map().size(); i++) {
      col_max =
          std::max(ts_col.Get(i).AsLong() + dur_col->Get(i).AsLong(), col_max);
    }
  }

  *min_value = std::min(*min_value, col_min);
  *max_value = std::max(*max_value, col_max);
}

std::vector<NullTermStringView> CreateRefTypeStringMap() {
  std::vector<NullTermStringView> map(static_cast<size_t>(RefType::kRefMax));
  map[static_cast<size_t>(RefType::kRefNoRef)] = NullTermStringView();
  map[static_cast<size_t>(RefType::kRefUtid)] = "utid";
  map[static_cast<size_t>(RefType::kRefCpuId)] = "cpu";
  map[static_cast<size_t>(RefType::kRefGpuId)] = "gpu";
  map[static_cast<size_t>(RefType::kRefIrq)] = "irq";
  map[static_cast<size_t>(RefType::kRefSoftIrq)] = "softirq";
  map[static_cast<size_t>(RefType::kRefUpid)] = "upid";
  map[static_cast<size_t>(RefType::kRefTrack)] = "track";
  return map;
}

}  // namespace

const std::vector<NullTermStringView>& GetRefTypeStringMap() {
  static const base::NoDestructor<std::vector<NullTermStringView>> map(
      CreateRefTypeStringMap());
  return map.ref();
}

TraceStorage::TraceStorage(const Config&) {
  // Reserve utid/upid 0. These are special as embedders (e.g. Perfetto UI)
  // exclude them by filtering them out. If the parsed trace contains ftrace
  // data, ProcessTracker::SetPidZeroIgnoredForIdleProcess will create a mapping
  // to these rows for tid/pid 0.
  tables::ThreadTable::Row thread_row;
  thread_row.tid = 0;
  thread_table_.Insert(thread_row);

  tables::ProcessTable::Row process_row;
  process_row.pid = 0;
  process_table_.Insert(process_row);

  for (uint32_t i = 0; i < variadic_type_ids_.size(); ++i) {
    variadic_type_ids_[i] = InternString(Variadic::kTypeNames[i]);
  }
}

TraceStorage::~TraceStorage() {}

uint32_t TraceStorage::SqlStats::RecordQueryBegin(const std::string& query,
                                                  int64_t time_queued,
                                                  int64_t time_started) {
  if (queries_.size() >= kMaxLogEntries) {
    queries_.pop_front();
    times_queued_.pop_front();
    times_started_.pop_front();
    times_first_next_.pop_front();
    times_ended_.pop_front();
    popped_queries_++;
  }
  queries_.push_back(query);
  times_queued_.push_back(time_queued);
  times_started_.push_back(time_started);
  times_first_next_.push_back(0);
  times_ended_.push_back(0);
  return static_cast<uint32_t>(popped_queries_ + queries_.size() - 1);
}

void TraceStorage::SqlStats::RecordQueryFirstNext(uint32_t row,
                                                  int64_t time_first_next) {
  // This means we've popped this query off the queue of queries before it had
  // a chance to finish. Just silently drop this number.
  if (popped_queries_ > row)
    return;
  uint32_t queue_row = row - popped_queries_;
  PERFETTO_DCHECK(queue_row < queries_.size());
  times_first_next_[queue_row] = time_first_next;
}

void TraceStorage::SqlStats::RecordQueryEnd(uint32_t row, int64_t time_ended) {
  // This means we've popped this query off the queue of queries before it had
  // a chance to finish. Just silently drop this number.
  if (popped_queries_ > row)
    return;
  uint32_t queue_row = row - popped_queries_;
  PERFETTO_DCHECK(queue_row < queries_.size());
  times_ended_[queue_row] = time_ended;
}

std::pair<int64_t, int64_t> TraceStorage::GetTraceTimestampBoundsNs() const {
  int64_t start_ns = std::numeric_limits<int64_t>::max();
  int64_t end_ns = std::numeric_limits<int64_t>::min();

  DbTableMaybeUpdateMinMax(raw_table_.ts(), &start_ns, &end_ns);
  DbTableMaybeUpdateMinMax(sched_slice_table_.ts(), &start_ns, &end_ns,
                           &sched_slice_table_.dur());
  DbTableMaybeUpdateMinMax(counter_table_.ts(), &start_ns, &end_ns);
  DbTableMaybeUpdateMinMax(slice_table_.ts(), &start_ns, &end_ns,
                           &slice_table_.dur());
  DbTableMaybeUpdateMinMax(heap_profile_allocation_table_.ts(), &start_ns,
                           &end_ns);
  DbTableMaybeUpdateMinMax(instant_table_.ts(), &start_ns, &end_ns);
  DbTableMaybeUpdateMinMax(android_log_table_.ts(), &start_ns, &end_ns);
  DbTableMaybeUpdateMinMax(heap_graph_object_table_.graph_sample_ts(),
                           &start_ns, &end_ns);

  if (start_ns == std::numeric_limits<int64_t>::max()) {
    return std::make_pair(0, 0);
  }
  if (start_ns == end_ns) {
    end_ns += 1;
  }
  return std::make_pair(start_ns, end_ns);
}

}  // namespace trace_processor
}  // namespace perfetto
