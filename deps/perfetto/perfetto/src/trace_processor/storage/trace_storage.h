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

#ifndef SRC_TRACE_PROCESSOR_STORAGE_TRACE_STORAGE_H_
#define SRC_TRACE_PROCESSOR_STORAGE_TRACE_STORAGE_H_

#include <array>
#include <deque>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/time.h"
#include "perfetto/ext/base/hash.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/storage/metadata.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/tables/android_tables.h"
#include "src/trace_processor/tables/counter_tables.h"
#include "src/trace_processor/tables/metadata_tables.h"
#include "src/trace_processor/tables/profiler_tables.h"
#include "src/trace_processor/tables/slice_tables.h"
#include "src/trace_processor/tables/track_tables.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto {
namespace trace_processor {

// UniquePid is an offset into |unique_processes_|. This is necessary because
// Unix pids are reused and thus not guaranteed to be unique over a long
// period of time.
using UniquePid = uint32_t;

// UniqueTid is an offset into |unique_threads_|. Necessary because tids can
// be reused.
using UniqueTid = uint32_t;

// StringId is an offset into |string_pool_|.
using StringId = StringPool::Id;
static const StringId kNullStringId = StringId::Null();

using ArgSetId = uint32_t;
static const ArgSetId kInvalidArgSetId = 0;

using TrackId = tables::TrackTable::Id;

using CounterId = tables::CounterTable::Id;

using SliceId = tables::SliceTable::Id;

using InstantId = tables::InstantTable::Id;

using SchedId = tables::SchedSliceTable::Id;

using MappingId = tables::StackProfileMappingTable::Id;

using FrameId = tables::StackProfileFrameTable::Id;

using SymbolId = tables::SymbolTable::Id;

using CallsiteId = tables::StackProfileCallsiteTable::Id;

using MetadataId = tables::MetadataTable::Id;

using RawId = tables::RawTable::Id;

using FlamegraphId = tables::ExperimentalFlamegraphNodesTable::Id;

using VulkanAllocId = tables::VulkanMemoryAllocationsTable::Id;

// TODO(lalitm): this is a temporary hack while migrating the counters table and
// will be removed when the migration is complete.
static const TrackId kInvalidTrackId =
    TrackId(std::numeric_limits<TrackId>::max());

enum class RefType {
  kRefNoRef = 0,
  kRefUtid = 1,
  kRefCpuId = 2,
  kRefIrq = 3,
  kRefSoftIrq = 4,
  kRefUpid = 5,
  kRefGpuId = 6,
  kRefTrack = 7,
  kRefMax
};

const std::vector<NullTermStringView>& GetRefTypeStringMap();

// Stores a data inside a trace file in a columnar form. This makes it efficient
// to read or search across a single field of the trace (e.g. all the thread
// names for a given CPU).
class TraceStorage {
 public:
  TraceStorage(const Config& = Config());

  virtual ~TraceStorage();

  class ThreadSlices {
   public:
    inline uint32_t AddThreadSlice(uint32_t slice_id,
                                   int64_t thread_timestamp_ns,
                                   int64_t thread_duration_ns,
                                   int64_t thread_instruction_count,
                                   int64_t thread_instruction_delta) {
      slice_ids_.emplace_back(slice_id);
      thread_timestamp_ns_.emplace_back(thread_timestamp_ns);
      thread_duration_ns_.emplace_back(thread_duration_ns);
      thread_instruction_counts_.emplace_back(thread_instruction_count);
      thread_instruction_deltas_.emplace_back(thread_instruction_delta);
      return slice_count() - 1;
    }

    uint32_t slice_count() const {
      return static_cast<uint32_t>(slice_ids_.size());
    }

    const std::deque<uint32_t>& slice_ids() const { return slice_ids_; }
    const std::deque<int64_t>& thread_timestamp_ns() const {
      return thread_timestamp_ns_;
    }
    const std::deque<int64_t>& thread_duration_ns() const {
      return thread_duration_ns_;
    }
    const std::deque<int64_t>& thread_instruction_counts() const {
      return thread_instruction_counts_;
    }
    const std::deque<int64_t>& thread_instruction_deltas() const {
      return thread_instruction_deltas_;
    }

    base::Optional<uint32_t> FindRowForSliceId(uint32_t slice_id) const {
      auto it =
          std::lower_bound(slice_ids().begin(), slice_ids().end(), slice_id);
      if (it != slice_ids().end() && *it == slice_id) {
        return static_cast<uint32_t>(std::distance(slice_ids().begin(), it));
      }
      return base::nullopt;
    }

    void UpdateThreadDeltasForSliceId(uint32_t slice_id,
                                      int64_t end_thread_timestamp_ns,
                                      int64_t end_thread_instruction_count) {
      uint32_t row = *FindRowForSliceId(slice_id);
      int64_t begin_ns = thread_timestamp_ns_[row];
      thread_duration_ns_[row] = end_thread_timestamp_ns - begin_ns;
      int64_t begin_ticount = thread_instruction_counts_[row];
      thread_instruction_deltas_[row] =
          end_thread_instruction_count - begin_ticount;
    }

   private:
    std::deque<uint32_t> slice_ids_;
    std::deque<int64_t> thread_timestamp_ns_;
    std::deque<int64_t> thread_duration_ns_;
    std::deque<int64_t> thread_instruction_counts_;
    std::deque<int64_t> thread_instruction_deltas_;
  };

  class VirtualTrackSlices {
   public:
    inline uint32_t AddVirtualTrackSlice(uint32_t slice_id,
                                         int64_t thread_timestamp_ns,
                                         int64_t thread_duration_ns,
                                         int64_t thread_instruction_count,
                                         int64_t thread_instruction_delta) {
      slice_ids_.emplace_back(slice_id);
      thread_timestamp_ns_.emplace_back(thread_timestamp_ns);
      thread_duration_ns_.emplace_back(thread_duration_ns);
      thread_instruction_counts_.emplace_back(thread_instruction_count);
      thread_instruction_deltas_.emplace_back(thread_instruction_delta);
      return slice_count() - 1;
    }

    uint32_t slice_count() const {
      return static_cast<uint32_t>(slice_ids_.size());
    }

    const std::deque<uint32_t>& slice_ids() const { return slice_ids_; }
    const std::deque<int64_t>& thread_timestamp_ns() const {
      return thread_timestamp_ns_;
    }
    const std::deque<int64_t>& thread_duration_ns() const {
      return thread_duration_ns_;
    }
    const std::deque<int64_t>& thread_instruction_counts() const {
      return thread_instruction_counts_;
    }
    const std::deque<int64_t>& thread_instruction_deltas() const {
      return thread_instruction_deltas_;
    }

    base::Optional<uint32_t> FindRowForSliceId(uint32_t slice_id) const {
      auto it =
          std::lower_bound(slice_ids().begin(), slice_ids().end(), slice_id);
      if (it != slice_ids().end() && *it == slice_id) {
        return static_cast<uint32_t>(std::distance(slice_ids().begin(), it));
      }
      return base::nullopt;
    }

    void UpdateThreadDeltasForSliceId(uint32_t slice_id,
                                      int64_t end_thread_timestamp_ns,
                                      int64_t end_thread_instruction_count) {
      uint32_t row = *FindRowForSliceId(slice_id);
      int64_t begin_ns = thread_timestamp_ns_[row];
      thread_duration_ns_[row] = end_thread_timestamp_ns - begin_ns;
      int64_t begin_ticount = thread_instruction_counts_[row];
      thread_instruction_deltas_[row] =
          end_thread_instruction_count - begin_ticount;
    }

   private:
    std::deque<uint32_t> slice_ids_;
    std::deque<int64_t> thread_timestamp_ns_;
    std::deque<int64_t> thread_duration_ns_;
    std::deque<int64_t> thread_instruction_counts_;
    std::deque<int64_t> thread_instruction_deltas_;
  };

  class SqlStats {
   public:
    static constexpr size_t kMaxLogEntries = 100;
    uint32_t RecordQueryBegin(const std::string& query,
                              int64_t time_queued,
                              int64_t time_started);
    void RecordQueryFirstNext(uint32_t row, int64_t time_first_next);
    void RecordQueryEnd(uint32_t row, int64_t time_end);
    size_t size() const { return queries_.size(); }
    const std::deque<std::string>& queries() const { return queries_; }
    const std::deque<int64_t>& times_queued() const { return times_queued_; }
    const std::deque<int64_t>& times_started() const { return times_started_; }
    const std::deque<int64_t>& times_first_next() const {
      return times_first_next_;
    }
    const std::deque<int64_t>& times_ended() const { return times_ended_; }

   private:
    uint32_t popped_queries_ = 0;

    std::deque<std::string> queries_;
    std::deque<int64_t> times_queued_;
    std::deque<int64_t> times_started_;
    std::deque<int64_t> times_first_next_;
    std::deque<int64_t> times_ended_;
  };

  struct Stats {
    using IndexMap = std::map<int, int64_t>;
    int64_t value = 0;
    IndexMap indexed_values;
  };
  using StatsMap = std::array<Stats, stats::kNumKeys>;

  // Return an unqiue identifier for the contents of each string.
  // The string is copied internally and can be destroyed after this called.
  // Virtual for testing.
  virtual StringId InternString(base::StringView str) {
    return string_pool_.InternString(str);
  }

  // Example usage: SetStats(stats::android_log_num_failed, 42);
  void SetStats(size_t key, int64_t value) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kSingle);
    stats_[key].value = value;
  }

  // Example usage: IncrementStats(stats::android_log_num_failed, -1);
  void IncrementStats(size_t key, int64_t increment = 1) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kSingle);
    stats_[key].value += increment;
  }

  // Example usage: IncrementIndexedStats(stats::cpu_failure, 1);
  void IncrementIndexedStats(size_t key, int index, int64_t increment = 1) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kIndexed);
    stats_[key].indexed_values[index] += increment;
  }

  // Example usage: SetIndexedStats(stats::cpu_failure, 1, 42);
  void SetIndexedStats(size_t key, int index, int64_t value) {
    PERFETTO_DCHECK(key < stats::kNumKeys);
    PERFETTO_DCHECK(stats::kTypes[key] == stats::kIndexed);
    stats_[key].indexed_values[index] = value;
  }

  class ScopedStatsTracer {
   public:
    ScopedStatsTracer(TraceStorage* storage, size_t key)
        : storage_(storage), key_(key), start_ns_(base::GetWallTimeNs()) {}

    ~ScopedStatsTracer() {
      if (!storage_)
        return;
      auto delta_ns = base::GetWallTimeNs() - start_ns_;
      storage_->IncrementStats(key_, delta_ns.count());
    }

    ScopedStatsTracer(ScopedStatsTracer&& other) noexcept { MoveImpl(&other); }

    ScopedStatsTracer& operator=(ScopedStatsTracer&& other) {
      MoveImpl(&other);
      return *this;
    }

   private:
    ScopedStatsTracer(const ScopedStatsTracer&) = delete;
    ScopedStatsTracer& operator=(const ScopedStatsTracer&) = delete;

    void MoveImpl(ScopedStatsTracer* other) {
      storage_ = other->storage_;
      key_ = other->key_;
      start_ns_ = other->start_ns_;
      other->storage_ = nullptr;
    }

    TraceStorage* storage_;
    size_t key_;
    base::TimeNanos start_ns_;
  };

  ScopedStatsTracer TraceExecutionTimeIntoStats(size_t key) {
    return ScopedStatsTracer(this, key);
  }

  // Reading methods.
  // Virtual for testing.
  virtual NullTermStringView GetString(StringId id) const {
    return string_pool_.Get(id);
  }

  const tables::ThreadTable& thread_table() const { return thread_table_; }
  tables::ThreadTable* mutable_thread_table() { return &thread_table_; }

  const tables::ProcessTable& process_table() const { return process_table_; }
  tables::ProcessTable* mutable_process_table() { return &process_table_; }

  const tables::TrackTable& track_table() const { return track_table_; }
  tables::TrackTable* mutable_track_table() { return &track_table_; }

  const tables::ProcessTrackTable& process_track_table() const {
    return process_track_table_;
  }
  tables::ProcessTrackTable* mutable_process_track_table() {
    return &process_track_table_;
  }

  const tables::ThreadTrackTable& thread_track_table() const {
    return thread_track_table_;
  }
  tables::ThreadTrackTable* mutable_thread_track_table() {
    return &thread_track_table_;
  }

  const tables::CounterTrackTable& counter_track_table() const {
    return counter_track_table_;
  }
  tables::CounterTrackTable* mutable_counter_track_table() {
    return &counter_track_table_;
  }

  const tables::ThreadCounterTrackTable& thread_counter_track_table() const {
    return thread_counter_track_table_;
  }
  tables::ThreadCounterTrackTable* mutable_thread_counter_track_table() {
    return &thread_counter_track_table_;
  }

  const tables::ProcessCounterTrackTable& process_counter_track_table() const {
    return process_counter_track_table_;
  }
  tables::ProcessCounterTrackTable* mutable_process_counter_track_table() {
    return &process_counter_track_table_;
  }

  const tables::CpuCounterTrackTable& cpu_counter_track_table() const {
    return cpu_counter_track_table_;
  }
  tables::CpuCounterTrackTable* mutable_cpu_counter_track_table() {
    return &cpu_counter_track_table_;
  }

  const tables::IrqCounterTrackTable& irq_counter_track_table() const {
    return irq_counter_track_table_;
  }
  tables::IrqCounterTrackTable* mutable_irq_counter_track_table() {
    return &irq_counter_track_table_;
  }

  const tables::SoftirqCounterTrackTable& softirq_counter_track_table() const {
    return softirq_counter_track_table_;
  }
  tables::SoftirqCounterTrackTable* mutable_softirq_counter_track_table() {
    return &softirq_counter_track_table_;
  }

  const tables::GpuCounterTrackTable& gpu_counter_track_table() const {
    return gpu_counter_track_table_;
  }
  tables::GpuCounterTrackTable* mutable_gpu_counter_track_table() {
    return &gpu_counter_track_table_;
  }

  const tables::SchedSliceTable& sched_slice_table() const {
    return sched_slice_table_;
  }
  tables::SchedSliceTable* mutable_sched_slice_table() {
    return &sched_slice_table_;
  }

  const tables::SliceTable& slice_table() const { return slice_table_; }
  tables::SliceTable* mutable_slice_table() { return &slice_table_; }

  const ThreadSlices& thread_slices() const { return thread_slices_; }
  ThreadSlices* mutable_thread_slices() { return &thread_slices_; }

  const VirtualTrackSlices& virtual_track_slices() const {
    return virtual_track_slices_;
  }
  VirtualTrackSlices* mutable_virtual_track_slices() {
    return &virtual_track_slices_;
  }

  const tables::GpuSliceTable& gpu_slice_table() const {
    return gpu_slice_table_;
  }
  tables::GpuSliceTable* mutable_gpu_slice_table() { return &gpu_slice_table_; }

  const tables::CounterTable& counter_table() const { return counter_table_; }
  tables::CounterTable* mutable_counter_table() { return &counter_table_; }

  const SqlStats& sql_stats() const { return sql_stats_; }
  SqlStats* mutable_sql_stats() { return &sql_stats_; }

  const tables::InstantTable& instant_table() const { return instant_table_; }
  tables::InstantTable* mutable_instant_table() { return &instant_table_; }

  const tables::AndroidLogTable& android_log_table() const {
    return android_log_table_;
  }
  tables::AndroidLogTable* mutable_android_log_table() {
    return &android_log_table_;
  }

  const StatsMap& stats() const { return stats_; }

  const tables::MetadataTable& metadata_table() const {
    return metadata_table_;
  }
  tables::MetadataTable* mutable_metadata_table() { return &metadata_table_; }

  const tables::ArgTable& arg_table() const { return arg_table_; }
  tables::ArgTable* mutable_arg_table() { return &arg_table_; }

  const tables::RawTable& raw_table() const { return raw_table_; }
  tables::RawTable* mutable_raw_table() { return &raw_table_; }

  const tables::StackProfileMappingTable& stack_profile_mapping_table() const {
    return stack_profile_mapping_table_;
  }
  tables::StackProfileMappingTable* mutable_stack_profile_mapping_table() {
    return &stack_profile_mapping_table_;
  }

  const tables::StackProfileFrameTable& stack_profile_frame_table() const {
    return stack_profile_frame_table_;
  }
  tables::StackProfileFrameTable* mutable_stack_profile_frame_table() {
    return &stack_profile_frame_table_;
  }

  const tables::StackProfileCallsiteTable& stack_profile_callsite_table()
      const {
    return stack_profile_callsite_table_;
  }
  tables::StackProfileCallsiteTable* mutable_stack_profile_callsite_table() {
    return &stack_profile_callsite_table_;
  }

  const tables::HeapProfileAllocationTable& heap_profile_allocation_table()
      const {
    return heap_profile_allocation_table_;
  }
  tables::HeapProfileAllocationTable* mutable_heap_profile_allocation_table() {
    return &heap_profile_allocation_table_;
  }

  const tables::PackageListTable& package_list_table() const {
    return package_list_table_;
  }
  tables::PackageListTable* mutable_package_list_table() {
    return &package_list_table_;
  }

  const tables::ProfilerSmapsTable& profiler_smaps_table() const {
    return profiler_smaps_table_;
  }
  tables::ProfilerSmapsTable* mutable_profiler_smaps_table() {
    return &profiler_smaps_table_;
  }

  const tables::CpuProfileStackSampleTable& cpu_profile_stack_sample_table()
      const {
    return cpu_profile_stack_sample_table_;
  }
  tables::CpuProfileStackSampleTable* mutable_cpu_profile_stack_sample_table() {
    return &cpu_profile_stack_sample_table_;
  }

  const tables::SymbolTable& symbol_table() const { return symbol_table_; }

  tables::SymbolTable* mutable_symbol_table() { return &symbol_table_; }

  const tables::HeapGraphObjectTable& heap_graph_object_table() const {
    return heap_graph_object_table_;
  }

  tables::HeapGraphObjectTable* mutable_heap_graph_object_table() {
    return &heap_graph_object_table_;
  }
  const tables::HeapGraphClassTable& heap_graph_class_table() const {
    return heap_graph_class_table_;
  }

  tables::HeapGraphClassTable* mutable_heap_graph_class_table() {
    return &heap_graph_class_table_;
  }

  const tables::HeapGraphReferenceTable& heap_graph_reference_table() const {
    return heap_graph_reference_table_;
  }

  tables::HeapGraphReferenceTable* mutable_heap_graph_reference_table() {
    return &heap_graph_reference_table_;
  }

  const tables::GpuTrackTable& gpu_track_table() const {
    return gpu_track_table_;
  }
  tables::GpuTrackTable* mutable_gpu_track_table() { return &gpu_track_table_; }

  const tables::VulkanMemoryAllocationsTable& vulkan_memory_allocations_table()
      const {
    return vulkan_memory_allocations_table_;
  }

  tables::VulkanMemoryAllocationsTable*
  mutable_vulkan_memory_allocations_table() {
    return &vulkan_memory_allocations_table_;
  }

  const tables::GraphicsFrameSliceTable& graphics_frame_slice_table() const {
    return graphics_frame_slice_table_;
  }

  tables::GraphicsFrameSliceTable* mutable_graphics_frame_slice_table() {
    return &graphics_frame_slice_table_;
  }

  const tables::GraphicsFrameStatsTable& graphics_frame_stats_table() const {
    return graphics_frame_stats_table_;
  }

  tables::GraphicsFrameStatsTable* mutable_graphics_frame_stats_table() {
    return &graphics_frame_stats_table_;
  }

  const StringPool& string_pool() const { return string_pool_; }
  StringPool* mutable_string_pool() { return &string_pool_; }

  // Number of interned strings in the pool. Includes the empty string w/ ID=0.
  size_t string_count() const { return string_pool_.size(); }

  // Start / end ts (in nanoseconds) across the parsed trace events.
  // Returns (0, 0) if the trace is empty.
  std::pair<int64_t, int64_t> GetTraceTimestampBoundsNs() const;

  // TODO(lalitm): remove this when we have a better home.
  std::vector<MappingId> FindMappingRow(StringId name,
                                        StringId build_id) const {
    auto it = stack_profile_mapping_index_.find(std::make_pair(name, build_id));
    if (it == stack_profile_mapping_index_.end())
      return {};
    return it->second;
  }

  // TODO(lalitm): remove this when we have a better home.
  void InsertMappingId(StringId name, StringId build_id, MappingId row) {
    auto pair = std::make_pair(name, build_id);
    stack_profile_mapping_index_[pair].emplace_back(row);
  }

  // TODO(lalitm): remove this when we have a better home.
  std::vector<FrameId> FindFrameIds(MappingId mapping_row,
                                    uint64_t rel_pc) const {
    auto it =
        stack_profile_frame_index_.find(std::make_pair(mapping_row, rel_pc));
    if (it == stack_profile_frame_index_.end())
      return {};
    return it->second;
  }

  // TODO(lalitm): remove this when we have a better home.
  void InsertFrameRow(MappingId mapping_row, uint64_t rel_pc, FrameId row) {
    auto pair = std::make_pair(mapping_row, rel_pc);
    stack_profile_frame_index_[pair].emplace_back(row);
  }

  Variadic GetArgValue(uint32_t row) const {
    Variadic v;
    v.type = *GetVariadicTypeForId(arg_table_.value_type()[row]);

    // Force initialization of union to stop GCC complaining.
    v.int_value = 0;

    switch (v.type) {
      case Variadic::Type::kBool:
        v.bool_value = static_cast<bool>(*arg_table_.int_value()[row]);
        break;
      case Variadic::Type::kInt:
        v.int_value = *arg_table_.int_value()[row];
        break;
      case Variadic::Type::kUint:
        v.uint_value = static_cast<uint64_t>(*arg_table_.int_value()[row]);
        break;
      case Variadic::Type::kString: {
        auto opt_value = arg_table_.string_value()[row];
        v.string_value = opt_value ? *opt_value : kNullStringId;
        break;
      }
      case Variadic::Type::kPointer:
        v.pointer_value = static_cast<uint64_t>(*arg_table_.int_value()[row]);
        break;
      case Variadic::Type::kReal:
        v.real_value = *arg_table_.real_value()[row];
        break;
      case Variadic::Type::kJson: {
        auto opt_value = arg_table_.string_value()[row];
        v.json_value = opt_value ? *opt_value : kNullStringId;
        break;
      }
    }
    return v;
  }

  StringId GetIdForVariadicType(Variadic::Type type) const {
    return variadic_type_ids_[type];
  }

  base::Optional<Variadic::Type> GetVariadicTypeForId(StringId id) const {
    auto it =
        std::find(variadic_type_ids_.begin(), variadic_type_ids_.end(), id);
    if (it == variadic_type_ids_.end())
      return base::nullopt;

    int64_t idx = std::distance(variadic_type_ids_.begin(), it);
    return static_cast<Variadic::Type>(idx);
  }

 private:
  using StringHash = uint64_t;

  TraceStorage(const TraceStorage&) = delete;
  TraceStorage& operator=(const TraceStorage&) = delete;

  TraceStorage(TraceStorage&&) = delete;
  TraceStorage& operator=(TraceStorage&&) = delete;

  // TODO(lalitm): remove this when we find a better home for this.
  using MappingKey = std::pair<StringId /* name */, StringId /* build id */>;
  std::map<MappingKey, std::vector<MappingId>> stack_profile_mapping_index_;

  // TODO(lalitm): remove this when we find a better home for this.
  using FrameKey = std::pair<MappingId, uint64_t /* rel_pc */>;
  std::map<FrameKey, std::vector<FrameId>> stack_profile_frame_index_;

  // One entry for each unique string in the trace.
  StringPool string_pool_;

  // Stats about parsing the trace.
  StatsMap stats_{};

  // Extra data extracted from the trace. Includes:
  // * metadata from chrome and benchmarking infrastructure
  // * descriptions of android packages
  tables::MetadataTable metadata_table_{&string_pool_, nullptr};

  // Metadata for tracks.
  tables::TrackTable track_table_{&string_pool_, nullptr};
  tables::GpuTrackTable gpu_track_table_{&string_pool_, &track_table_};
  tables::ProcessTrackTable process_track_table_{&string_pool_, &track_table_};
  tables::ThreadTrackTable thread_track_table_{&string_pool_, &track_table_};

  // Track tables for counter events.
  tables::CounterTrackTable counter_track_table_{&string_pool_, &track_table_};
  tables::ThreadCounterTrackTable thread_counter_track_table_{
      &string_pool_, &counter_track_table_};
  tables::ProcessCounterTrackTable process_counter_track_table_{
      &string_pool_, &counter_track_table_};
  tables::CpuCounterTrackTable cpu_counter_track_table_{&string_pool_,
                                                        &counter_track_table_};
  tables::IrqCounterTrackTable irq_counter_track_table_{&string_pool_,
                                                        &counter_track_table_};
  tables::SoftirqCounterTrackTable softirq_counter_track_table_{
      &string_pool_, &counter_track_table_};
  tables::GpuCounterTrackTable gpu_counter_track_table_{&string_pool_,
                                                        &counter_track_table_};

  // Args for all other tables.
  tables::ArgTable arg_table_{&string_pool_, nullptr};

  // Information about all the threads and processes in the trace.
  tables::ThreadTable thread_table_{&string_pool_, nullptr};
  tables::ProcessTable process_table_{&string_pool_, nullptr};

  // Slices coming from userspace events (e.g. Chromium TRACE_EVENT macros).
  tables::SliceTable slice_table_{&string_pool_, nullptr};

  // Slices from CPU scheduling data.
  tables::SchedSliceTable sched_slice_table_{&string_pool_, nullptr};

  // Additional attributes for threads slices (sub-type of NestableSlices).
  ThreadSlices thread_slices_;

  // Additional attributes for virtual track slices (sub-type of
  // NestableSlices).
  VirtualTrackSlices virtual_track_slices_;

  // Additional attributes for gpu track slices (sub-type of
  // NestableSlices).
  tables::GpuSliceTable gpu_slice_table_{&string_pool_, &slice_table_};

  // The values from the Counter events from the trace. This includes CPU
  // frequency events as well systrace trace_marker counter events.
  tables::CounterTable counter_table_{&string_pool_, nullptr};

  SqlStats sql_stats_;

  // These are instantaneous events in the trace. They have no duration
  // and do not have a value that make sense to track over time.
  // e.g. signal events
  tables::InstantTable instant_table_{&string_pool_, nullptr};

  // Raw events are every ftrace event in the trace. The raw event includes
  // the timestamp and the pid. The args for the raw event will be in the
  // args table. This table can be used to generate a text version of the
  // trace.
  tables::RawTable raw_table_{&string_pool_, nullptr};
  tables::AndroidLogTable android_log_table_{&string_pool_, nullptr};

  tables::StackProfileMappingTable stack_profile_mapping_table_{&string_pool_,
                                                                nullptr};
  tables::StackProfileFrameTable stack_profile_frame_table_{&string_pool_,
                                                            nullptr};
  tables::StackProfileCallsiteTable stack_profile_callsite_table_{&string_pool_,
                                                                  nullptr};
  tables::HeapProfileAllocationTable heap_profile_allocation_table_{
      &string_pool_, nullptr};
  tables::CpuProfileStackSampleTable cpu_profile_stack_sample_table_{
      &string_pool_, nullptr};
  tables::PackageListTable package_list_table_{&string_pool_, nullptr};
  tables::ProfilerSmapsTable profiler_smaps_table_{&string_pool_, nullptr};

  // Symbol tables (mappings from frames to symbol names)
  tables::SymbolTable symbol_table_{&string_pool_, nullptr};
  tables::HeapGraphObjectTable heap_graph_object_table_{&string_pool_, nullptr};
  tables::HeapGraphClassTable heap_graph_class_table_{&string_pool_, nullptr};
  tables::HeapGraphReferenceTable heap_graph_reference_table_{&string_pool_,
                                                              nullptr};

  tables::VulkanMemoryAllocationsTable vulkan_memory_allocations_table_{
      &string_pool_, nullptr};

  tables::GraphicsFrameSliceTable graphics_frame_slice_table_{&string_pool_,
                                                              &slice_table_};
  tables::GraphicsFrameStatsTable graphics_frame_stats_table_{&string_pool_,
                                                              nullptr};

  // The below array allow us to map between enums and their string
  // representations.
  std::array<StringId, Variadic::kMaxType + 1> variadic_type_ids_;
};

}  // namespace trace_processor
}  // namespace perfetto

namespace std {

template <>
struct hash<::perfetto::trace_processor::BaseId> {
  using argument_type = ::perfetto::trace_processor::BaseId;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<uint32_t>{}(r.value);
  }
};

template <>
struct hash<::perfetto::trace_processor::TrackId>
    : hash<::perfetto::trace_processor::BaseId> {};
template <>
struct hash<::perfetto::trace_processor::MappingId>
    : hash<::perfetto::trace_processor::BaseId> {};
template <>
struct hash<::perfetto::trace_processor::CallsiteId>
    : hash<::perfetto::trace_processor::BaseId> {};
template <>
struct hash<::perfetto::trace_processor::FrameId>
    : hash<::perfetto::trace_processor::BaseId> {};

template <>
struct hash<::perfetto::trace_processor::tables::StackProfileFrameTable::Row> {
  using argument_type =
      ::perfetto::trace_processor::tables::StackProfileFrameTable::Row;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<::perfetto::trace_processor::StringId>{}(r.name) ^
           std::hash<::perfetto::base::Optional<
               ::perfetto::trace_processor::MappingId>>{}(r.mapping) ^
           std::hash<int64_t>{}(r.rel_pc);
  }
};

template <>
struct hash<
    ::perfetto::trace_processor::tables::StackProfileCallsiteTable::Row> {
  using argument_type =
      ::perfetto::trace_processor::tables::StackProfileCallsiteTable::Row;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<int64_t>{}(r.depth) ^
           std::hash<::perfetto::base::Optional<
               ::perfetto::trace_processor::CallsiteId>>{}(r.parent_id) ^
           std::hash<::perfetto::trace_processor::FrameId>{}(r.frame_id);
  }
};

template <>
struct hash<
    ::perfetto::trace_processor::tables::StackProfileMappingTable::Row> {
  using argument_type =
      ::perfetto::trace_processor::tables::StackProfileMappingTable::Row;
  using result_type = size_t;

  result_type operator()(const argument_type& r) const {
    return std::hash<::perfetto::trace_processor::StringId>{}(r.build_id) ^
           std::hash<int64_t>{}(r.exact_offset) ^
           std::hash<int64_t>{}(r.start_offset) ^
           std::hash<int64_t>{}(r.start) ^ std::hash<int64_t>{}(r.end) ^
           std::hash<int64_t>{}(r.load_bias) ^
           std::hash<::perfetto::trace_processor::StringId>{}(r.name);
  }
};

}  // namespace std

#endif  // SRC_TRACE_PROCESSOR_STORAGE_TRACE_STORAGE_H_
