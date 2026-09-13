// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-internal.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"
#include "src/objects/js-objects.h"
#include "src/sandbox/cppheap-pointer-table-inl.h"
#include "src/sandbox/cppheap-pointer-table.h"
#include "src/sandbox/external-pointer-table-inl.h"
#include "src/sandbox/external-pointer-table.h"
#include "test/unittests/heap/heap-utils.h"  // For ManualGCScope
#include "test/unittests/test-utils.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8::internal {

template <typename Table>
struct PointerTableTraits;

template <>
struct PointerTableTraits<CppHeapPointerTable> {
  using Handle = CppHeapPointerHandle;
};

template <>
struct PointerTableTraits<ExternalPointerTable> {
  using Handle = ExternalPointerHandle;
};

class PointerTableTest : public TestWithContext {
 public:
  PointerTableTest() { v8_flags.stress_compaction = true; }

  template <typename Table>
  class SpaceScope {
   public:
    using Space = typename Table::Space;
    explicit SpaceScope(Table* table) : table_(table) {
      table_->InitializeSpace(&space_);
    }
    ~SpaceScope() { table_->TearDownSpace(&space_); }

    Space* get() { return &space_; }
    Space* operator->() { return &space_; }
    explicit operator Space*() { return &space_; }

   private:
    Table* table_;
    Space space_;
  };

  template <typename Table, typename Tag>
  typename PointerTableTraits<Table>::Handle* AllocateEntry(
      Table& table, typename Table::Space* space, Address val, Tag tag,
      std::vector<std::unique_ptr<typename PointerTableTraits<Table>::Handle>>&
          handles) {
    auto handle = AllocateEntry(table, space, val, tag);
    handles.push_back(std::move(handle));
    return handles.back().get();
  }

  template <typename Table, typename Tag>
  std::unique_ptr<typename PointerTableTraits<Table>::Handle> AllocateEntry(
      Table& table, typename Table::Space* space, Address val, Tag tag) {
    using Handle = typename PointerTableTraits<Table>::Handle;
    auto handle = std::make_unique<Handle>();
    *handle = table.AllocateAndInitializeEntry(space, val, tag);
    return handle;
  }

  template <typename Table, typename Tag>
  std::vector<std::unique_ptr<typename PointerTableTraits<Table>::Handle>>
  FillFirstSegment(Table& table, typename Table::Space* space, Address val,
                   Tag tag) {
    using Handle = typename PointerTableTraits<Table>::Handle;
    std::vector<std::unique_ptr<Handle>> handles;
    do {
      AllocateEntry(table, space, val, tag, handles);
    } while (space->freelist_length() > 0);
    CHECK_EQ(1, space->NumSegmentsForTesting());
    return handles;
  }

  template <typename Table, typename Handles>
  void MarkAll(Table& table, typename Table::Space* space,
               const Handles& handles) {
    for (const auto& handle : handles) {
      table.Mark(space, *handle, reinterpret_cast<Address>(handle.get()));
    }
  }
};

struct DummyAddress {
  // NOLINTNEXTLINE
  operator Address() const { return reinterpret_cast<Address>(&value); }
  int value = 0;
};

TEST_F(PointerTableTest, ExternalPointerTableCompaction) {
  auto& table = i_isolate()->external_pointer_table();
  SpaceScope<ExternalPointerTable> space(&table);

  DummyAddress val1;
  DummyAddress val2;

  // Allocate one entry that will be dead (unmarked) to leave a free slot in the
  // first segment.
  auto dead_handle =
      AllocateEntry(table, space.get(), val1, kLastExternalTypeTag);

  auto handles =
      FillFirstSegment(table, space.get(), val1, kLastExternalTypeTag);

  // Allocate one more entry, which must end up on a new segment.
  auto target_loc =
      AllocateEntry(table, space.get(), val2, kLastExternalTypeTag);
  CHECK_EQ(2, space->NumSegmentsForTesting());
  ExternalPointerHandle original_handle = *target_loc;

  // Simulate a sweep where dead_handle is dead (unmarked) and all other entries
  // are marked.
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CHECK_GE(space->freelist_length(), 1u);

  // Now perform compaction. Start compaction and mark all live entries.
  space->StartCompactingIfNeeded();
  CHECK(space->IsCompacting());
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));

  // Sweeping should now evacuate target_loc into the first segment and
  // deallocate the second segment.
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(1, space->NumSegmentsForTesting());
  CHECK_NE(original_handle, *target_loc);
  CHECK_EQ(val2, table.Get(*target_loc, kLastExternalTypeTag));
}

TEST_F(PointerTableTest, CppHeapPointerTableCompaction) {
  CppHeapPointerTable& table = i_isolate()->cpp_heap_pointer_table();
  SpaceScope<CppHeapPointerTable> space(&table);

  DummyAddress val1;
  DummyAddress val2;

  // Allocate one entry that will be dead (unmarked) to leave a free slot in the
  // first segment.
  auto dead_handle = AllocateEntry(table, space.get(), val1,
                                   CppHeapPointerTag::kFirstObjectWrappableTag);

  auto handles = FillFirstSegment(table, space.get(), val1,
                                  CppHeapPointerTag::kFirstObjectWrappableTag);

  // Allocate one more entry, which must end up on a new segment.
  auto target_loc = AllocateEntry(table, space.get(), val2,
                                  CppHeapPointerTag::kFirstObjectWrappableTag);
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CppHeapPointerHandle original_handle = *target_loc;

  // Simulate a sweep where dead_handle is dead (unmarked) and all other entries
  // are marked.
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CHECK_GE(space->freelist_length(), 1u);

  // Now perform compaction. Start compaction and mark all live entries.
  space->StartCompactingIfNeeded();
  CHECK(space->IsCompacting());
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));

  // Sweeping should now evacuate target_loc into the first segment and
  // deallocate the second segment.
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(1, space->NumSegmentsForTesting());
  CHECK_NE(original_handle, *target_loc);
  CHECK_EQ(val2,
           table.Get(*target_loc, CppHeapPointerTag::kFirstObjectWrappableTag));
}

TEST_F(PointerTableTest, CppHeapPointerTableFieldInvalidation) {
  auto& table = i_isolate()->cpp_heap_pointer_table();
  SpaceScope<CppHeapPointerTable> space(&table);

  DummyAddress val;

  // Allocate one entry that will be dead (unmarked) to leave a free slot in the
  // first segment.
  auto dead_handle = AllocateEntry(table, space.get(), val,
                                   CppHeapPointerTag::kFirstObjectWrappableTag);

  auto handles = FillFirstSegment(table, space.get(), val,
                                  CppHeapPointerTag::kFirstObjectWrappableTag);

  // Allocate entry on second segment.
  auto target_loc = AllocateEntry(table, space.get(), val,
                                  CppHeapPointerTag::kFirstObjectWrappableTag);
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CppHeapPointerHandle handle_before_sweep = *target_loc;

  // Free slot 0 on first segment by marking the other handles.
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CHECK_GE(space->freelist_length(), 1u);

  // Start compaction and mark all live entries, creating an evacuation entry
  // for target_loc.
  space->StartCompactingIfNeeded();
  CHECK(space->IsCompacting());
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));

  // Invalidate the field pointing to target_loc before sweeping.
  space->NotifyCppHeapPointerFieldInvalidated(
      reinterpret_cast<Address>(target_loc.get()));

  // Sweeping should abort evacuation of target_loc because its field was
  // invalidated.
  table.SweepAndCompact(space.get(), i_isolate()->counters());

  // Because target_loc evacuation bailed out, its old entry on segment 2 is
  // dead and segment 2 is deallocated.
  CHECK_EQ(1, space->NumSegmentsForTesting());
  CHECK_EQ(handle_before_sweep, *target_loc);
}

TEST_F(PointerTableTest, CppHeapPointerTableHandleOverwriteBailouts) {
  auto& table = i_isolate()->cpp_heap_pointer_table();
  SpaceScope<CppHeapPointerTable> space(&table);

  DummyAddress val1;
  DummyAddress val2;

  // Allocate two entries that will be dead (unmarked) to leave free slots in
  // the first segment.
  auto dead_handle1 = AllocateEntry(
      table, space.get(), val1, CppHeapPointerTag::kFirstObjectWrappableTag);
  auto dead_handle2 = AllocateEntry(
      table, space.get(), val1, CppHeapPointerTag::kFirstObjectWrappableTag);

  auto handles = FillFirstSegment(table, space.get(), val1,
                                  CppHeapPointerTag::kFirstObjectWrappableTag);

  // Allocate two entries on second segment.
  auto target_loc_null = AllocateEntry(
      table, space.get(), val2, CppHeapPointerTag::kFirstObjectWrappableTag);
  auto target_loc_valid = AllocateEntry(
      table, space.get(), val2, CppHeapPointerTag::kFirstObjectWrappableTag);
  CHECK_EQ(2, space->NumSegmentsForTesting());

  // Free slots 0 and 1 on first segment by marking the other handles.
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc_null,
             reinterpret_cast<Address>(target_loc_null.get()));
  table.Mark(space.get(), *target_loc_valid,
             reinterpret_cast<Address>(target_loc_valid.get()));
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CHECK_GE(space->freelist_length(), 2u);

  // Start compaction and mark all live entries, creating evacuation entries.
  space->StartCompactingIfNeeded();
  CHECK(space->IsCompacting());
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc_null,
             reinterpret_cast<Address>(target_loc_null.get()));
  table.Mark(space.get(), *target_loc_valid,
             reinterpret_cast<Address>(target_loc_valid.get()));

  // Simulate mutator overwriting handles after evacuation entries were created:
  // 1. Overwrite with null handle.
  *target_loc_null = kNullCppHeapPointerHandle;
  // 2. Overwrite with a valid handle below the compaction frontier (from
  // segment 1).
  CppHeapPointerHandle pre_frontier_handle = *handles[0];
  *target_loc_valid = pre_frontier_handle;

  // Sweeping should detect the overwritten handles and bail out of evacuating
  // both entries.
  table.SweepAndCompact(space.get(), i_isolate()->counters());

  CHECK_EQ(1, space->NumSegmentsForTesting());
  CHECK_EQ(kNullCppHeapPointerHandle, *target_loc_null);
  CHECK_EQ(pre_frontier_handle, *target_loc_valid);
  CHECK_EQ(val1, table.Get(*target_loc_valid,
                           CppHeapPointerTag::kFirstObjectWrappableTag));
}

TEST_F(PointerTableTest, ExternalPointerTableFieldInvalidation) {
  auto& table = i_isolate()->external_pointer_table();
  SpaceScope<ExternalPointerTable> space(&table);

  DummyAddress val;

  // Allocate one entry that will be dead (unmarked) to leave a free slot in the
  // first segment.
  auto dead_handle =
      AllocateEntry(table, space.get(), val, kLastExternalTypeTag);

  auto handles =
      FillFirstSegment(table, space.get(), val, kLastExternalTypeTag);

  // Allocate entry on second segment.
  auto target_loc =
      AllocateEntry(table, space.get(), val, kLastExternalTypeTag);
  CHECK_EQ(2, space->NumSegmentsForTesting());
  ExternalPointerHandle handle_before_sweep = *target_loc;

  // Free slot 0 on first segment by marking the other handles.
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CHECK_GE(space->freelist_length(), 1u);

  // Start compaction and mark all live entries, creating an evacuation entry
  // for target_loc.
  space->StartCompactingIfNeeded();
  CHECK(space->IsCompacting());
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc,
             reinterpret_cast<Address>(target_loc.get()));

  // Invalidate the field pointing to target_loc before sweeping.
  space->NotifyExternalPointerFieldInvalidated(
      reinterpret_cast<Address>(target_loc.get()), kLastExternalTypeTag);

  // Sweeping should abort evacuation of target_loc because its field was
  // invalidated.
  table.SweepAndCompact(space.get(), i_isolate()->counters());

  // Because target_loc evacuation bailed out, its old entry on segment 2 is
  // dead and segment 2 is deallocated.
  CHECK_EQ(1, space->NumSegmentsForTesting());
  CHECK_EQ(handle_before_sweep, *target_loc);
}

TEST_F(PointerTableTest, ExternalPointerTableHandleOverwriteBailouts) {
  auto& table = i_isolate()->external_pointer_table();
  SpaceScope<ExternalPointerTable> space(&table);

  DummyAddress val1;
  DummyAddress val2;

  // Allocate two entries that will be dead (unmarked) to leave free slots in
  // the first segment.
  auto dead_handle1 =
      AllocateEntry(table, space.get(), val1, kLastExternalTypeTag);
  auto dead_handle2 =
      AllocateEntry(table, space.get(), val1, kLastExternalTypeTag);

  auto handles =
      FillFirstSegment(table, space.get(), val1, kLastExternalTypeTag);

  // Allocate two entries on second segment.
  auto target_loc_null =
      AllocateEntry(table, space.get(), val2, kLastExternalTypeTag);
  auto target_loc_valid =
      AllocateEntry(table, space.get(), val2, kLastExternalTypeTag);
  CHECK_EQ(2, space->NumSegmentsForTesting());

  // Free slots 0 and 1 on first segment by marking the other handles.
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc_null,
             reinterpret_cast<Address>(target_loc_null.get()));
  table.Mark(space.get(), *target_loc_valid,
             reinterpret_cast<Address>(target_loc_valid.get()));
  table.SweepAndCompact(space.get(), i_isolate()->counters());
  CHECK_EQ(2, space->NumSegmentsForTesting());
  CHECK_GE(space->freelist_length(), 2u);

  // Start compaction and mark all live entries, creating evacuation entries.
  space->StartCompactingIfNeeded();
  CHECK(space->IsCompacting());
  MarkAll(table, space.get(), handles);
  table.Mark(space.get(), *target_loc_null,
             reinterpret_cast<Address>(target_loc_null.get()));
  table.Mark(space.get(), *target_loc_valid,
             reinterpret_cast<Address>(target_loc_valid.get()));

  // Simulate mutator overwriting handles after evacuation entries were created:
  // 1. Overwrite with null handle.
  *target_loc_null = kNullExternalPointerHandle;
  // 2. Overwrite with a valid handle below the compaction frontier (from
  // segment 1).
  ExternalPointerHandle pre_frontier_handle = *handles[0];
  *target_loc_valid = pre_frontier_handle;

  // Sweeping should detect the overwritten handles and bail out of evacuating
  // both entries.
  table.SweepAndCompact(space.get(), i_isolate()->counters());

  CHECK_EQ(1, space->NumSegmentsForTesting());
  CHECK_EQ(kNullExternalPointerHandle, *target_loc_null);
  CHECK_EQ(pre_frontier_handle, *target_loc_valid);
  CHECK_EQ(val1, table.Get(*target_loc_valid, kLastExternalTypeTag));
}

}  // namespace v8::internal

#endif  // V8_ENABLE_SANDBOX
