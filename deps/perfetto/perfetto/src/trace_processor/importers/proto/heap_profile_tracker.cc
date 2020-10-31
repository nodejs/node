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

#include "src/trace_processor/importers/proto/heap_profile_tracker.h"

#include "perfetto/base/logging.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/types/trace_processor_context.h"

#include "protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "protos/perfetto/trace/profiling/profile_packet.pbzero.h"

namespace perfetto {
namespace trace_processor {

namespace {
struct MergedCallsite {
  StringId frame_name;
  StringId mapping_name;
  base::Optional<uint32_t> parent_idx;
  bool operator<(const MergedCallsite& o) const {
    return std::tie(frame_name, mapping_name, parent_idx) <
           std::tie(o.frame_name, o.mapping_name, o.parent_idx);
  }
};

std::vector<MergedCallsite> GetMergedCallsites(TraceStorage* storage,
                                               uint32_t callstack_row) {
  const tables::StackProfileCallsiteTable& callsites_tbl =
      storage->stack_profile_callsite_table();
  const tables::StackProfileFrameTable& frames_tbl =
      storage->stack_profile_frame_table();
  const tables::SymbolTable& symbols_tbl = storage->symbol_table();
  const tables::StackProfileMappingTable& mapping_tbl =
      storage->stack_profile_mapping_table();

  uint32_t frame_idx =
      *frames_tbl.id().IndexOf(callsites_tbl.frame_id()[callstack_row]);

  uint32_t mapping_idx =
      *mapping_tbl.id().IndexOf(frames_tbl.mapping()[frame_idx]);
  StringId mapping_name = mapping_tbl.name()[mapping_idx];

  base::Optional<uint32_t> symbol_set_id =
      frames_tbl.symbol_set_id()[frame_idx];

  if (!symbol_set_id) {
    StringId frame_name = frames_tbl.name()[frame_idx];
    return {{frame_name, mapping_name, base::nullopt}};
  }

  std::vector<MergedCallsite> result;
  // id == symbol_set_id for the bottommost frame.
  // TODO(lalitm): Encode this optimization in the table and remove this
  // custom optimization.
  uint32_t symbol_set_idx = *symbols_tbl.id().IndexOf(SymbolId(*symbol_set_id));
  for (uint32_t i = symbol_set_idx;
       i < symbols_tbl.row_count() &&
       symbols_tbl.symbol_set_id()[i] == *symbol_set_id;
       ++i) {
    result.emplace_back(
        MergedCallsite{symbols_tbl.name()[i], mapping_name, base::nullopt});
  }
  return result;
}
}  // namespace

std::unique_ptr<tables::ExperimentalFlamegraphNodesTable> BuildNativeFlamegraph(
    TraceStorage* storage,
    UniquePid upid,
    int64_t timestamp) {
  const tables::HeapProfileAllocationTable& allocation_tbl =
      storage->heap_profile_allocation_table();
  const tables::StackProfileCallsiteTable& callsites_tbl =
      storage->stack_profile_callsite_table();

  StringId profile_type = storage->InternString("native");

  std::vector<uint32_t> callsite_to_merged_callsite(callsites_tbl.row_count(),
                                                    0);
  std::map<MergedCallsite, uint32_t> merged_callsites_to_table_idx;

  std::unique_ptr<tables::ExperimentalFlamegraphNodesTable> tbl(
      new tables::ExperimentalFlamegraphNodesTable(
          storage->mutable_string_pool(), nullptr));

  // FORWARD PASS:
  // Aggregate callstacks by frame name / mapping name. Use symbolization
  // data.
  for (uint32_t i = 0; i < callsites_tbl.row_count(); ++i) {
    base::Optional<uint32_t> parent_idx;

    auto opt_parent_id = callsites_tbl.parent_id()[i];
    if (opt_parent_id) {
      parent_idx = callsites_tbl.id().IndexOf(*opt_parent_id);
      parent_idx = callsite_to_merged_callsite[*parent_idx];
      PERFETTO_CHECK(*parent_idx < i);
    }

    auto callsites = GetMergedCallsites(storage, i);
    for (MergedCallsite& merged_callsite : callsites) {
      merged_callsite.parent_idx = parent_idx;
      auto it = merged_callsites_to_table_idx.find(merged_callsite);
      if (it == merged_callsites_to_table_idx.end()) {
        std::tie(it, std::ignore) = merged_callsites_to_table_idx.emplace(
            merged_callsite, merged_callsites_to_table_idx.size());
        tables::ExperimentalFlamegraphNodesTable::Row row{};
        if (parent_idx) {
          row.depth = tbl->depth()[*parent_idx] + 1;
        } else {
          row.depth = 0;
        }
        row.ts = timestamp;
        row.upid = upid;
        row.profile_type = profile_type;
        row.name = merged_callsite.frame_name;
        row.map_name = merged_callsite.mapping_name;
        if (parent_idx)
          row.parent_id = tbl->id()[*parent_idx];

        parent_idx = tbl->Insert(std::move(row)).row;
        PERFETTO_CHECK(merged_callsites_to_table_idx.size() ==
                       tbl->row_count());
      }
      parent_idx = it->second;
    }
    PERFETTO_CHECK(parent_idx);
    callsite_to_merged_callsite[i] = *parent_idx;
  }

  // PASS OVER ALLOCATIONS:
  // Aggregate allocations into the newly built tree.
  auto filtered = allocation_tbl.Filter(
      {allocation_tbl.ts().le(timestamp), allocation_tbl.upid().eq(upid)});

  if (filtered.row_count() == 0) {
    return nullptr;
  }

  for (auto it = filtered.IterateRows(); it; it.Next()) {
    int64_t size =
        it.Get(static_cast<uint32_t>(
                   tables::HeapProfileAllocationTable::ColumnIndex::size))
            .long_value;
    int64_t count =
        it.Get(static_cast<uint32_t>(
                   tables::HeapProfileAllocationTable::ColumnIndex::count))
            .long_value;
    int64_t callsite_id =
        it
            .Get(static_cast<uint32_t>(
                tables::HeapProfileAllocationTable::ColumnIndex::callsite_id))
            .long_value;

    PERFETTO_CHECK((size <= 0 && count <= 0) || (size >= 0 && count >= 0));
    uint32_t merged_idx =
        callsite_to_merged_callsite[*callsites_tbl.id().IndexOf(
            CallsiteId(static_cast<uint32_t>(callsite_id)))];
    if (count > 0) {
      tbl->mutable_alloc_size()->Set(merged_idx,
                                     tbl->alloc_size()[merged_idx] + size);
      tbl->mutable_alloc_count()->Set(merged_idx,
                                      tbl->alloc_count()[merged_idx] + count);
    }

    tbl->mutable_size()->Set(merged_idx, tbl->size()[merged_idx] + size);
    tbl->mutable_count()->Set(merged_idx, tbl->count()[merged_idx] + count);
  }

  // BACKWARD PASS:
  // Propagate sizes to parents.
  for (int64_t i = tbl->row_count() - 1; i >= 0; --i) {
    uint32_t idx = static_cast<uint32_t>(i);

    tbl->mutable_cumulative_size()->Set(
        idx, tbl->cumulative_size()[idx] + tbl->size()[idx]);
    tbl->mutable_cumulative_count()->Set(
        idx, tbl->cumulative_count()[idx] + tbl->count()[idx]);

    tbl->mutable_cumulative_alloc_size()->Set(
        idx, tbl->cumulative_alloc_size()[idx] + tbl->alloc_size()[idx]);
    tbl->mutable_cumulative_alloc_count()->Set(
        idx, tbl->cumulative_alloc_count()[idx] + tbl->alloc_count()[idx]);

    auto parent = tbl->parent_id()[idx];
    if (parent) {
      uint32_t parent_idx = *tbl->id().IndexOf(
          tables::ExperimentalFlamegraphNodesTable::Id(*parent));
      tbl->mutable_cumulative_size()->Set(
          parent_idx,
          tbl->cumulative_size()[parent_idx] + tbl->cumulative_size()[idx]);
      tbl->mutable_cumulative_count()->Set(
          parent_idx,
          tbl->cumulative_count()[parent_idx] + tbl->cumulative_count()[idx]);

      tbl->mutable_cumulative_alloc_size()->Set(
          parent_idx, tbl->cumulative_alloc_size()[parent_idx] +
                          tbl->cumulative_alloc_size()[idx]);
      tbl->mutable_cumulative_alloc_count()->Set(
          parent_idx, tbl->cumulative_alloc_count()[parent_idx] +
                          tbl->cumulative_alloc_count()[idx]);
    }
  }

  return tbl;
}

HeapProfileTracker::HeapProfileTracker(TraceProcessorContext* context)
    : context_(context), empty_(context_->storage->InternString({"", 0})) {}

HeapProfileTracker::~HeapProfileTracker() = default;

void HeapProfileTracker::SetProfilePacketIndex(uint32_t seq_id,
                                               uint64_t index) {
  SequenceState& sequence_state = sequence_state_[seq_id];
  bool dropped_packet = false;
  // heapprofd starts counting at index = 0.
  if (!sequence_state.prev_index && index != 0) {
    dropped_packet = true;
  }

  if (sequence_state.prev_index && *sequence_state.prev_index + 1 != index) {
    dropped_packet = true;
  }

  if (dropped_packet) {
    if (sequence_state.prev_index) {
      PERFETTO_ELOG("Missing packets between %" PRIu64 " and %" PRIu64,
                    *sequence_state.prev_index, index);
    } else {
      PERFETTO_ELOG("Invalid first packet index %" PRIu64 " (!= 0)", index);
    }

    context_->storage->IncrementStats(stats::heapprofd_missing_packet);
  }
  sequence_state.prev_index = index;
}

void HeapProfileTracker::AddAllocation(
    uint32_t seq_id,
    StackProfileTracker* stack_profile_tracker,
    const SourceAllocation& alloc,
    const StackProfileTracker::InternLookup* intern_lookup) {
  SequenceState& sequence_state = sequence_state_[seq_id];

  auto opt_callstack_id = stack_profile_tracker->FindOrInsertCallstack(
      alloc.callstack_id, intern_lookup);
  if (!opt_callstack_id)
    return;

  CallsiteId callstack_id = *opt_callstack_id;

  UniquePid upid = context_->process_tracker->GetOrCreateProcess(
      static_cast<uint32_t>(alloc.pid));

  tables::HeapProfileAllocationTable::Row alloc_row{
      alloc.timestamp, upid, callstack_id,
      static_cast<int64_t>(alloc.alloc_count),
      static_cast<int64_t>(alloc.self_allocated)};

  tables::HeapProfileAllocationTable::Row free_row{
      alloc.timestamp, upid, callstack_id,
      -static_cast<int64_t>(alloc.free_count),
      -static_cast<int64_t>(alloc.self_freed)};

  auto prev_alloc_it = sequence_state.prev_alloc.find({upid, callstack_id});
  if (prev_alloc_it == sequence_state.prev_alloc.end()) {
    std::tie(prev_alloc_it, std::ignore) = sequence_state.prev_alloc.emplace(
        std::make_pair(upid, callstack_id),
        tables::HeapProfileAllocationTable::Row{});
  }

  tables::HeapProfileAllocationTable::Row& prev_alloc = prev_alloc_it->second;

  auto prev_free_it = sequence_state.prev_free.find({upid, callstack_id});
  if (prev_free_it == sequence_state.prev_free.end()) {
    std::tie(prev_free_it, std::ignore) = sequence_state.prev_free.emplace(
        std::make_pair(upid, callstack_id),
        tables::HeapProfileAllocationTable::Row{});
  }

  tables::HeapProfileAllocationTable::Row& prev_free = prev_free_it->second;

  std::set<CallsiteId>& callstacks_for_source_callstack_id =
      sequence_state.seen_callstacks[std::make_pair(upid, alloc.callstack_id)];
  bool new_callstack;
  std::tie(std::ignore, new_callstack) =
      callstacks_for_source_callstack_id.emplace(callstack_id);

  if (new_callstack) {
    sequence_state.alloc_correction[alloc.callstack_id] = prev_alloc;
    sequence_state.free_correction[alloc.callstack_id] = prev_free;
  }

  auto alloc_correction_it =
      sequence_state.alloc_correction.find(alloc.callstack_id);
  if (alloc_correction_it != sequence_state.alloc_correction.end()) {
    const auto& alloc_correction = alloc_correction_it->second;
    alloc_row.count += alloc_correction.count;
    alloc_row.size += alloc_correction.size;
  }

  auto free_correction_it =
      sequence_state.free_correction.find(alloc.callstack_id);
  if (free_correction_it != sequence_state.free_correction.end()) {
    const auto& free_correction = free_correction_it->second;
    free_row.count += free_correction.count;
    free_row.size += free_correction.size;
  }

  tables::HeapProfileAllocationTable::Row alloc_delta = alloc_row;
  tables::HeapProfileAllocationTable::Row free_delta = free_row;

  alloc_delta.count -= prev_alloc.count;
  alloc_delta.size -= prev_alloc.size;

  free_delta.count -= prev_free.count;
  free_delta.size -= prev_free.size;

  if (alloc_delta.count < 0 || alloc_delta.size < 0 || free_delta.count > 0 ||
      free_delta.size > 0) {
    PERFETTO_DLOG("Non-monotonous allocation.");
    context_->storage->IncrementIndexedStats(stats::heapprofd_malformed_packet,
                                             static_cast<int>(upid));
    return;
  }

  if (alloc_delta.count) {
    context_->storage->mutable_heap_profile_allocation_table()->Insert(
        alloc_delta);
  }
  if (free_delta.count) {
    context_->storage->mutable_heap_profile_allocation_table()->Insert(
        free_delta);
  }

  prev_alloc = alloc_row;
  prev_free = free_row;
}

void HeapProfileTracker::StoreAllocation(uint32_t seq_id,
                                         SourceAllocation alloc) {
  SequenceState& sequence_state = sequence_state_[seq_id];
  sequence_state.pending_allocs.emplace_back(std::move(alloc));
}

void HeapProfileTracker::CommitAllocations(
    uint32_t seq_id,
    StackProfileTracker* stack_profile_tracker,
    const StackProfileTracker::InternLookup* intern_lookup) {
  SequenceState& sequence_state = sequence_state_[seq_id];
  for (const auto& p : sequence_state.pending_allocs)
    AddAllocation(seq_id, stack_profile_tracker, p, intern_lookup);
  sequence_state.pending_allocs.clear();
}

void HeapProfileTracker::FinalizeProfile(
    uint32_t seq_id,
    StackProfileTracker* stack_profile_tracker,
    const StackProfileTracker::InternLookup* intern_lookup) {
  CommitAllocations(seq_id, stack_profile_tracker, intern_lookup);
  stack_profile_tracker->ClearIndices();
}

void HeapProfileTracker::NotifyEndOfFile() {
  for (const auto& key_and_sequence_state : sequence_state_) {
    const SequenceState& sequence_state = key_and_sequence_state.second;
    if (!sequence_state.pending_allocs.empty()) {
      context_->storage->IncrementStats(stats::heapprofd_non_finalized_profile);
    }
  }
}

}  // namespace trace_processor
}  // namespace perfetto
