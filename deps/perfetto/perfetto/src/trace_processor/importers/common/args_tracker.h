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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_ARGS_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_ARGS_TRACKER_H_

#include "src/trace_processor/importers/common/global_args_tracker.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/types/variadic.h"

namespace perfetto {
namespace trace_processor {

// Tracks and stores args for rows until the end of the packet. This allows
// allows args to pushed as a group into storage.
class ArgsTracker {
 public:
  using UpdatePolicy = GlobalArgsTracker::UpdatePolicy;

  // Stores the table and row at creation time which args are associated with.
  // This allows callers to directly add args without repeating the row the
  // args should be associated with.
  class BoundInserter {
   public:
    virtual ~BoundInserter();

    BoundInserter(BoundInserter&&);
    BoundInserter& operator=(BoundInserter&&);

    BoundInserter(const BoundInserter&) = delete;
    BoundInserter& operator=(const BoundInserter&) = delete;

    // Adds an arg with the same key and flat_key.
    BoundInserter& AddArg(
        StringId key,
        Variadic v,
        UpdatePolicy update_policy = UpdatePolicy::kAddOrUpdate) {
      return AddArg(key, key, v, update_policy);
    }

    virtual BoundInserter& AddArg(
        StringId flat_key,
        StringId key,
        Variadic v,
        UpdatePolicy update_policy = UpdatePolicy::kAddOrUpdate) {
      args_tracker_->AddArg(arg_set_id_column_, row_, flat_key, key, v,
                            update_policy);
      return *this;
    }

    // IncrementArrayEntryIndex() and GetNextArrayEntryIndex() provide a way to
    // track the next array index for an array under a specific key.
    void IncrementArrayEntryIndex(StringId key) {
      // Zero-initializes |key| in the map if it doesn't exist yet.
      args_tracker_
          ->array_indexes_[std::make_tuple(arg_set_id_column_, row_, key)]++;
    }

    size_t GetNextArrayEntryIndex(StringId key) {
      // Zero-initializes |key| in the map if it doesn't exist yet.
      return args_tracker_
          ->array_indexes_[std::make_tuple(arg_set_id_column_, row_, key)];
    }

   protected:
    BoundInserter(ArgsTracker* args_tracker,
                  Column* arg_set_id_column,
                  uint32_t row);

   private:
    friend class ArgsTracker;

    ArgsTracker* args_tracker_ = nullptr;
    Column* arg_set_id_column_ = nullptr;
    uint32_t row_ = 0;
  };

  explicit ArgsTracker(TraceProcessorContext*);
  virtual ~ArgsTracker();

  BoundInserter AddArgsTo(RawId id) {
    return AddArgsTo(context_->storage->mutable_raw_table(), id);
  }

  BoundInserter AddArgsTo(CounterId id) {
    return AddArgsTo(context_->storage->mutable_counter_table(), id);
  }

  BoundInserter AddArgsTo(InstantId id) {
    return AddArgsTo(context_->storage->mutable_instant_table(), id);
  }

  BoundInserter AddArgsTo(SliceId id) {
    return AddArgsTo(context_->storage->mutable_slice_table(), id);
  }

  BoundInserter AddArgsTo(MetadataId id) {
    auto* table = context_->storage->mutable_metadata_table();
    uint32_t row = *table->id().IndexOf(id);
    return BoundInserter(this, table->mutable_int_value(), row);
  }

  BoundInserter AddArgsTo(TrackId id) {
    auto* table = context_->storage->mutable_track_table();
    uint32_t row = *table->id().IndexOf(id);
    return BoundInserter(this, table->mutable_source_arg_set_id(), row);
  }

  BoundInserter AddArgsTo(VulkanAllocId id) {
    return AddArgsTo(
        context_->storage->mutable_vulkan_memory_allocations_table(), id);
  }

  // Commits the added args to storage.
  // Virtual for testing.
  virtual void Flush();

 private:
  template <typename Table>
  BoundInserter AddArgsTo(Table* table, typename Table::Id id) {
    uint32_t row = *table->id().IndexOf(id);
    return BoundInserter(this, table->mutable_arg_set_id(), row);
  }

  void AddArg(Column* arg_set_id,
              uint32_t row,
              StringId flat_key,
              StringId key,
              Variadic,
              UpdatePolicy);

  std::vector<GlobalArgsTracker::Arg> args_;
  TraceProcessorContext* const context_;

  using ArrayKeyTuple =
      std::tuple<Column* /*arg_set_id*/, uint32_t /*row*/, StringId /*key*/>;
  std::map<ArrayKeyTuple, size_t /*next_index*/> array_indexes_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_COMMON_ARGS_TRACKER_H_
