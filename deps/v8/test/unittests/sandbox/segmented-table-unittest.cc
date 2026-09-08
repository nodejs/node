// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/segmented-table.h"

#include <set>
#include <vector>

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

namespace {

class TestSegmentedTable : public SegmentedTableBase {
 public:
  using SegmentedTableBase::kAlignment;
  using SegmentedTableBase::kInternalReadOnlySegmentsOffset;
  using SegmentedTableBase::kSegmentSize;
  using SegmentedTableBase::kUseContiguousMemory;
  using SegmentedTableBase::SegmentBase;

  TestSegmentedTable() = default;
  ~TestSegmentedTable() {
    if (is_initialized()) {
      TearDown();
    }
  }

  using SegmentedTableBase::AllocateReadOnlySegment;
  using SegmentedTableBase::base;
  using SegmentedTableBase::FreeTableSegment;
  using SegmentedTableBase::Initialize;
  using SegmentedTableBase::is_initialized;
  using SegmentedTableBase::read_only_segments_used;
  using SegmentedTableBase::ResetReadOnlySegments;
  using SegmentedTableBase::TearDown;
  using SegmentedTableBase::TryAllocateSegment;
};

V8_NOINLINE uint8_t ReadMemory(Address addr) {
  return *reinterpret_cast<volatile const uint8_t*>(addr);
}

V8_NOINLINE void WriteMemory(Address addr, uint8_t value) {
  *reinterpret_cast<volatile uint8_t*>(addr) = value;
}

}  // namespace

TEST(SegmentedTableBaseTest, SegmentBaseProperties) {
  auto seg0 = TestSegmentedTable::SegmentBase::At(0);
  EXPECT_EQ(0u, seg0.number());
  EXPECT_EQ(0u, seg0.offset());

  auto seg1 =
      TestSegmentedTable::SegmentBase::At(TestSegmentedTable::kSegmentSize);
  EXPECT_EQ(1u, seg1.number());
  EXPECT_EQ(TestSegmentedTable::kSegmentSize, seg1.offset());

  auto seg3 =
      TestSegmentedTable::SegmentBase::At(3 * TestSegmentedTable::kSegmentSize);
  EXPECT_EQ(3u, seg3.number());
  EXPECT_EQ(3 * TestSegmentedTable::kSegmentSize, seg3.offset());

  EXPECT_EQ(seg0, seg0);
  EXPECT_NE(seg0, seg1);
  EXPECT_LT(seg0, seg1);
  EXPECT_FALSE(seg1 < seg0);
  EXPECT_FALSE(seg0 < seg0);
}

TEST(SegmentedTableBaseTest, InitializationAndTearDown) {
  TestSegmentedTable table;
  EXPECT_FALSE(table.is_initialized());
  EXPECT_EQ(0u, TestSegmentedTable::BaseOffset());

  constexpr size_t kReservationSize = 4 * TestSegmentedTable::kSegmentSize;
  table.Initialize(kReservationSize, 0, false);
  EXPECT_TRUE(table.is_initialized());
#ifdef V8_TARGET_ARCH_64_BIT
  EXPECT_NE(kNullAddress, table.base());
#else
  EXPECT_EQ(kNullAddress, table.base());
#endif

  table.TearDown();
  EXPECT_FALSE(table.is_initialized());

  // Can be re-initialized after TearDown.
  table.Initialize(kReservationSize, 0, false);
  EXPECT_TRUE(table.is_initialized());
#ifdef V8_TARGET_ARCH_64_BIT
  EXPECT_NE(kNullAddress, table.base());
#else
  EXPECT_EQ(kNullAddress, table.base());
#endif
  table.TearDown();
  EXPECT_FALSE(table.is_initialized());
}

TEST(SegmentedTableBaseTest, AllocateAndFreeSegment) {
  TestSegmentedTable table;
  constexpr size_t kReservationSize = 4 * TestSegmentedTable::kSegmentSize;
  table.Initialize(kReservationSize, 0, false);

  auto segment_opt = table.TryAllocateSegment();
  ASSERT_TRUE(segment_opt.has_value());
  auto segment = *segment_opt;
  EXPECT_EQ(segment.offset(),
            segment.number() * TestSegmentedTable::kSegmentSize);
#ifdef V8_TARGET_ARCH_64_BIT
  EXPECT_LT(segment.offset(), kReservationSize);
#endif

  Address base_addr = table.base() + segment.offset();
  WriteMemory(base_addr, 0xAA);
  WriteMemory(base_addr + TestSegmentedTable::kSegmentSize / 2, 0xBB);
  WriteMemory(base_addr + TestSegmentedTable::kSegmentSize - 1, 0xCC);

  EXPECT_EQ(0xAA, ReadMemory(base_addr));
  EXPECT_EQ(0xBB, ReadMemory(base_addr + TestSegmentedTable::kSegmentSize / 2));
  EXPECT_EQ(0xCC, ReadMemory(base_addr + TestSegmentedTable::kSegmentSize - 1));

  table.FreeTableSegment(segment);
}

TEST(SegmentedTableBaseTest, AllocateMultipleSegments) {
  TestSegmentedTable table;
  constexpr size_t kNumSegments = 4;
  constexpr size_t kReservationSize =
      kNumSegments * TestSegmentedTable::kSegmentSize;
  table.Initialize(kReservationSize, 0, false);

  std::vector<TestSegmentedTable::SegmentBase> segments;
  for (size_t i = 0; i < kNumSegments; ++i) {
    auto seg = table.TryAllocateSegment();
    ASSERT_TRUE(seg.has_value());
    segments.push_back(*seg);
  }

  std::set<uint32_t> numbers;
  for (const auto& s : segments) {
    EXPECT_TRUE(numbers.insert(s.number()).second);
    Address addr = table.base() + s.offset();
    WriteMemory(addr, static_cast<uint8_t>(s.number()));
  }

  for (const auto& s : segments) {
    Address addr = table.base() + s.offset();
    EXPECT_EQ(static_cast<uint8_t>(s.number()), ReadMemory(addr));
  }

#ifdef V8_TARGET_ARCH_64_BIT
  // On 64-bit with a fixed contiguous subspace, the reservation is full.
  EXPECT_FALSE(table.TryAllocateSegment().has_value());
  // Free one segment and reallocate into the newly available slot.
  table.FreeTableSegment(segments[0]);
  auto new_seg = table.TryAllocateSegment();
  ASSERT_TRUE(new_seg.has_value());
  EXPECT_EQ(segments[0], *new_seg);
  for (const auto& s : segments) {
    table.FreeTableSegment(s);
  }
#else
  for (const auto& s : segments) {
    table.FreeTableSegment(s);
  }
#endif
}

#ifdef V8_TARGET_ARCH_64_BIT

TEST(SegmentedTableBaseTest, ReadOnlySegments) {
  TestSegmentedTable table;
  constexpr size_t kNumReadOnlySegments = 2;
  constexpr size_t kReadOnlySize =
      kNumReadOnlySegments * TestSegmentedTable::kSegmentSize;
  constexpr size_t kReservationSize = 6 * TestSegmentedTable::kSegmentSize;
  table.Initialize(kReservationSize, kReadOnlySize, false);

  EXPECT_EQ(0u, table.read_only_segments_used());

  auto ro_seg0 = table.AllocateReadOnlySegment();
  EXPECT_EQ(0u, ro_seg0.number());
  EXPECT_EQ(0u, ro_seg0.offset());
  EXPECT_EQ(1u, table.read_only_segments_used());

  auto ro_seg1 = table.AllocateReadOnlySegment();
  EXPECT_EQ(1u, ro_seg1.number());
  EXPECT_EQ(TestSegmentedTable::kSegmentSize, ro_seg1.offset());
  EXPECT_EQ(2u, table.read_only_segments_used());

  EXPECT_EQ(2u, table.ResetReadOnlySegments());
  EXPECT_EQ(0u, table.read_only_segments_used());

  auto ro_seg_again = table.AllocateReadOnlySegment();
  EXPECT_EQ(0u, ro_seg_again.number());
  EXPECT_EQ(1u, table.read_only_segments_used());

  // Regular segment allocation must not overlap with read-only segments.
  auto normal_seg = table.TryAllocateSegment();
  ASSERT_TRUE(normal_seg.has_value());
  EXPECT_GE(normal_seg->number(), kNumReadOnlySegments);
  EXPECT_GE(normal_seg->offset(), kReadOnlySize);
  table.FreeTableSegment(*normal_seg);
}

TEST(SegmentedTableBaseTest, UnsealReadOnlySegmentsScope) {
  TestSegmentedTable table;
  constexpr size_t kNumReadOnlySegments = 2;
  constexpr size_t kReadOnlySize =
      kNumReadOnlySegments * TestSegmentedTable::kSegmentSize;
  constexpr size_t kReservationSize = 4 * TestSegmentedTable::kSegmentSize;
  table.Initialize(kReservationSize, kReadOnlySize, false);

  auto ro_seg = table.AllocateReadOnlySegment();
  Address ro_addr = table.base() + ro_seg.offset();

  {
    TestSegmentedTable::UnsealReadOnlySegmentScope scope(&table);
    WriteMemory(ro_addr, 0x42);
    WriteMemory(ro_addr + 100, 0x43);
    EXPECT_EQ(0x42, ReadMemory(ro_addr));
    EXPECT_EQ(0x43, ReadMemory(ro_addr + 100));
  }

  // After sealing, reads still return the previously written values.
  EXPECT_EQ(0x42, ReadMemory(ro_addr));
  EXPECT_EQ(0x43, ReadMemory(ro_addr + 100));
}

#endif  // V8_TARGET_ARCH_64_BIT

// Tests that accessing freed segment memory immediately crashes.
// This is critical for sandbox security: freed table segments must be rendered
// inaccessible (e.g. PROT_NONE), preventing use-after-free exploits.
TEST(SegmentedTableBaseDeathTest, WriteToFreedSegmentCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, 0, false);
        auto segment = table.TryAllocateSegment();
        ASSERT_TRUE(segment.has_value());
        Address addr = table.base() + segment->offset();
        WriteMemory(addr, 0x42);
        EXPECT_EQ(0x42, ReadMemory(addr));
        table.FreeTableSegment(*segment);
        WriteMemory(addr, 0x43);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest, ReadFromFreedSegmentCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, 0, false);
        auto segment = table.TryAllocateSegment();
        ASSERT_TRUE(segment.has_value());
        Address addr = table.base() + segment->offset();
        WriteMemory(addr, 0x42);
        EXPECT_EQ(0x42, ReadMemory(addr));
        table.FreeTableSegment(*segment);
        ReadMemory(addr);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest, ReadFromFreedSegmentAtEndCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, 0, false);
        auto segment = table.TryAllocateSegment();
        ASSERT_TRUE(segment.has_value());
        Address addr = table.base() + segment->offset() +
                       TestSegmentedTable::kSegmentSize - 1;
        WriteMemory(addr, 0x42);
        EXPECT_EQ(0x42, ReadMemory(addr));
        table.FreeTableSegment(*segment);
        ReadMemory(addr);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest, ReadFromFreedSegmentInMiddleCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, 0, false);
        auto segment = table.TryAllocateSegment();
        ASSERT_TRUE(segment.has_value());
        Address addr = table.base() + segment->offset() +
                       TestSegmentedTable::kSegmentSize / 2;
        WriteMemory(addr, 0x42);
        EXPECT_EQ(0x42, ReadMemory(addr));
        table.FreeTableSegment(*segment);
        ReadMemory(addr);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest,
     AccessFreedSegmentWithOtherActiveSegmentsCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, 0, false);
        auto seg1 = table.TryAllocateSegment();
        auto seg2 = table.TryAllocateSegment();
        ASSERT_TRUE(seg1.has_value());
        ASSERT_TRUE(seg2.has_value());
        Address addr1 = table.base() + seg1->offset();
        Address addr2 = table.base() + seg2->offset();
        WriteMemory(addr1, 0x11);
        WriteMemory(addr2, 0x22);
        EXPECT_EQ(0x11, ReadMemory(addr1));
        EXPECT_EQ(0x22, ReadMemory(addr2));

        // Free seg1.
        table.FreeTableSegment(*seg1);

        // Accessing seg2 still works normally.
        EXPECT_EQ(0x22, ReadMemory(addr2));
        WriteMemory(addr2, 0x33);
        EXPECT_EQ(0x33, ReadMemory(addr2));

        // Accessing seg1 must crash.
        ReadMemory(addr1);
      },
      "");
}

#ifdef V8_TARGET_ARCH_64_BIT

TEST(SegmentedTableBaseDeathTest, AccessAfterTearDownCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        Address addr = kNullAddress;
        {
          TestSegmentedTable table;
          constexpr size_t kReservationSize =
              4 * TestSegmentedTable::kSegmentSize;
          table.Initialize(kReservationSize, 0, false);
          auto seg = table.TryAllocateSegment();
          ASSERT_TRUE(seg.has_value());
          addr = table.base() + seg->offset();
          WriteMemory(addr, 0x42);
          table.TearDown();
        }
        ReadMemory(addr);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest, WriteToSealedReadOnlySegmentCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        constexpr size_t kReadOnlySize = 2 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, kReadOnlySize, false);
        auto ro_seg = table.AllocateReadOnlySegment();
        Address ro_addr = table.base() + ro_seg.offset();
        // Read is allowed on sealed segment.
        ReadMemory(ro_addr);
        // Write to sealed segment must crash.
        WriteMemory(ro_addr, 0x42);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest, WriteAfterUnsealScopeExitCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        constexpr size_t kReadOnlySize = 2 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, kReadOnlySize, false);
        auto ro_seg = table.AllocateReadOnlySegment();
        Address ro_addr = table.base() + ro_seg.offset();
        {
          TestSegmentedTable::UnsealReadOnlySegmentScope unseal_scope(&table);
          WriteMemory(ro_addr, 0x55);
          EXPECT_EQ(0x55, ReadMemory(ro_addr));
        }
        // After scope exit, reading still works.
        EXPECT_EQ(0x55, ReadMemory(ro_addr));
        // But writing now must crash.
        WriteMemory(ro_addr, 0x66);
      },
      "");
}

TEST(SegmentedTableBaseDeathTest, AllocateReadOnlySegmentBeyondLimitCrashes) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        TestSegmentedTable table;
        constexpr size_t kReservationSize =
            4 * TestSegmentedTable::kSegmentSize;
        constexpr size_t kReadOnlySize = 1 * TestSegmentedTable::kSegmentSize;
        table.Initialize(kReservationSize, kReadOnlySize, false);
        table.AllocateReadOnlySegment();
        // Second allocation exceeds kReadOnlySize (1 segment), must crash.
        table.AllocateReadOnlySegment();
      },
      "");
}

#endif  // V8_TARGET_ARCH_64_BIT

}  // namespace v8::internal
