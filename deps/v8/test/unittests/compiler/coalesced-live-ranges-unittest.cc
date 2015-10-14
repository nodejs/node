// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/coalesced-live-ranges.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace compiler {


// Utility offering shorthand syntax for building up a range by providing its ID
// and pairs (start, end) specifying intervals. Circumvents current incomplete
// support for C++ features such as instantiation lists, on OS X and Android.
class TestRangeBuilder {
 public:
  explicit TestRangeBuilder(Zone* zone) : id_(-1), pairs_(), zone_(zone) {}

  TestRangeBuilder& Id(int id) {
    id_ = id;
    return *this;
  }
  TestRangeBuilder& Add(int start, int end) {
    pairs_.push_back({start, end});
    return *this;
  }

  LiveRange* Build(int start, int end) { return Add(start, end).Build(); }

  LiveRange* Build() {
    LiveRange* range = new (zone_) LiveRange(id_, MachineType::kRepTagged);
    // Traverse the provided interval specifications backwards, because that is
    // what LiveRange expects.
    for (int i = static_cast<int>(pairs_.size()) - 1; i >= 0; --i) {
      Interval pair = pairs_[i];
      LifetimePosition start = LifetimePosition::FromInt(pair.first);
      LifetimePosition end = LifetimePosition::FromInt(pair.second);
      CHECK(start < end);
      range->AddUseInterval(start, end, zone_);
    }

    pairs_.clear();
    return range;
  }

 private:
  typedef std::pair<int, int> Interval;
  typedef std::vector<Interval> IntervalList;
  int id_;
  IntervalList pairs_;
  Zone* zone_;
};


class CoalescedLiveRangesTest : public TestWithZone {
 public:
  CoalescedLiveRangesTest() : TestWithZone(), ranges_(zone()) {}
  bool HasNoConflicts(const LiveRange* range);
  bool ConflictsPreciselyWith(const LiveRange* range, int id);
  bool ConflictsPreciselyWith(const LiveRange* range, int id1, int id2);

  CoalescedLiveRanges& ranges() { return ranges_; }
  const CoalescedLiveRanges& ranges() const { return ranges_; }
  bool AllocationsAreValid() const;
  void RemoveConflicts(LiveRange* range);

 private:
  typedef ZoneSet<int> LiveRangeIDs;
  bool IsRangeConflictingWith(const LiveRange* range, const LiveRangeIDs& ids);
  CoalescedLiveRanges ranges_;
};


bool CoalescedLiveRangesTest::ConflictsPreciselyWith(const LiveRange* range,
                                                     int id) {
  LiveRangeIDs set(zone());
  set.insert(id);
  return IsRangeConflictingWith(range, set);
}


bool CoalescedLiveRangesTest::ConflictsPreciselyWith(const LiveRange* range,
                                                     int id1, int id2) {
  LiveRangeIDs set(zone());
  set.insert(id1);
  set.insert(id2);
  return IsRangeConflictingWith(range, set);
}


bool CoalescedLiveRangesTest::HasNoConflicts(const LiveRange* range) {
  LiveRangeIDs set(zone());
  return IsRangeConflictingWith(range, set);
}


void CoalescedLiveRangesTest::RemoveConflicts(LiveRange* range) {
  auto conflicts = ranges().GetConflicts(range);
  LiveRangeIDs seen(zone());
  for (auto c = conflicts.Current(); c != nullptr;
       c = conflicts.RemoveCurrentAndGetNext()) {
    EXPECT_FALSE(seen.count(c->id()) > 0);
    seen.insert(c->id());
  }
}


bool CoalescedLiveRangesTest::AllocationsAreValid() const {
  return ranges().VerifyAllocationsAreValidForTesting();
}


bool CoalescedLiveRangesTest::IsRangeConflictingWith(const LiveRange* range,
                                                     const LiveRangeIDs& ids) {
  LiveRangeIDs found_ids(zone());

  auto conflicts = ranges().GetConflicts(range);
  for (auto conflict = conflicts.Current(); conflict != nullptr;
       conflict = conflicts.GetNext()) {
    found_ids.insert(conflict->id());
  }
  return found_ids == ids;
}


TEST_F(CoalescedLiveRangesTest, VisitEmptyAllocations) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(1, 5);
  ASSERT_TRUE(ranges().empty());
  ASSERT_TRUE(AllocationsAreValid());
  ASSERT_TRUE(HasNoConflicts(range));
}


TEST_F(CoalescedLiveRangesTest, CandidateBeforeAfterAllocations) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(5, 6);
  ranges().AllocateRange(range);
  ASSERT_FALSE(ranges().empty());
  ASSERT_TRUE(AllocationsAreValid());
  LiveRange* query = TestRangeBuilder(zone()).Id(2).Build(1, 2);
  ASSERT_TRUE(HasNoConflicts(query));
  query = TestRangeBuilder(zone()).Id(3).Build(1, 5);
  ASSERT_TRUE(HasNoConflicts(query));
}


TEST_F(CoalescedLiveRangesTest, CandidateBeforeAfterManyAllocations) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(5, 7).Add(10, 12).Build();
  ranges().AllocateRange(range);
  ASSERT_FALSE(ranges().empty());
  ASSERT_TRUE(AllocationsAreValid());
  LiveRange* query =
      TestRangeBuilder(zone()).Id(2).Add(1, 2).Add(13, 15).Build();
  ASSERT_TRUE(HasNoConflicts(query));
  query = TestRangeBuilder(zone()).Id(3).Add(1, 5).Add(12, 15).Build();
  ASSERT_TRUE(HasNoConflicts(query));
}


TEST_F(CoalescedLiveRangesTest, SelfConflictsPreciselyWithSelf) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(1, 5);
  ranges().AllocateRange(range);
  ASSERT_FALSE(ranges().empty());
  ASSERT_TRUE(AllocationsAreValid());
  ASSERT_TRUE(ConflictsPreciselyWith(range, 1));
  range = TestRangeBuilder(zone()).Id(2).Build(8, 10);
  ranges().AllocateRange(range);
  ASSERT_TRUE(ConflictsPreciselyWith(range, 2));
}


TEST_F(CoalescedLiveRangesTest, QueryStartsBeforeConflict) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(2, 5);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(2).Build(1, 3);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1));
  range = TestRangeBuilder(zone()).Id(3).Build(8, 10);
  ranges().AllocateRange(range);
  query = TestRangeBuilder(zone()).Id(4).Build(6, 9);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 3));
}


TEST_F(CoalescedLiveRangesTest, QueryStartsInConflict) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(2, 5);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(2).Build(3, 6);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1));
  range = TestRangeBuilder(zone()).Id(3).Build(8, 10);
  ranges().AllocateRange(range);
  query = TestRangeBuilder(zone()).Id(4).Build(9, 11);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 3));
}


TEST_F(CoalescedLiveRangesTest, QueryContainedInConflict) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(1, 5);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(2).Build(2, 3);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1));
}


TEST_F(CoalescedLiveRangesTest, QueryContainsConflict) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(2, 3);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(2).Build(1, 5);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1));
}


TEST_F(CoalescedLiveRangesTest, QueryCoversManyIntervalsSameRange) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(1, 5).Add(7, 9).Add(20, 25).Build();
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(2).Build(2, 8);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1));
}


TEST_F(CoalescedLiveRangesTest, QueryCoversManyIntervalsDifferentRanges) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(1, 5).Add(20, 25).Build();
  ranges().AllocateRange(range);
  range = TestRangeBuilder(zone()).Id(2).Build(7, 10);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(3).Build(2, 22);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1, 2));
}


TEST_F(CoalescedLiveRangesTest, QueryFitsInGaps) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(1, 5).Add(10, 15).Add(20, 25).Build();
  ranges().AllocateRange(range);
  LiveRange* query =
      TestRangeBuilder(zone()).Id(3).Add(5, 10).Add(16, 19).Add(27, 30).Build();
  ASSERT_TRUE(HasNoConflicts(query));
}


TEST_F(CoalescedLiveRangesTest, DeleteConflictBefore) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Add(1, 4).Add(5, 6).Build();
  ranges().AllocateRange(range);
  range = TestRangeBuilder(zone()).Id(2).Build(40, 50);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(3).Build(3, 7);
  RemoveConflicts(query);
  query = TestRangeBuilder(zone()).Id(4).Build(0, 60);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 2));
}


TEST_F(CoalescedLiveRangesTest, DeleteConflictAfter) {
  LiveRange* range = TestRangeBuilder(zone()).Id(1).Build(1, 5);
  ranges().AllocateRange(range);
  range = TestRangeBuilder(zone()).Id(2).Add(40, 50).Add(60, 70).Build();
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(3).Build(45, 60);
  RemoveConflicts(query);
  query = TestRangeBuilder(zone()).Id(4).Build(0, 60);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 1));
}


TEST_F(CoalescedLiveRangesTest, DeleteConflictStraddle) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(1, 5).Add(10, 20).Build();
  ranges().AllocateRange(range);
  range = TestRangeBuilder(zone()).Id(2).Build(40, 50);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(3).Build(4, 15);
  RemoveConflicts(query);
  query = TestRangeBuilder(zone()).Id(4).Build(0, 60);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 2));
}


TEST_F(CoalescedLiveRangesTest, DeleteConflictManyOverlapsBefore) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(1, 5).Add(6, 10).Add(10, 20).Build();
  ranges().AllocateRange(range);
  range = TestRangeBuilder(zone()).Id(2).Build(40, 50);
  ranges().AllocateRange(range);
  LiveRange* query = TestRangeBuilder(zone()).Id(3).Build(4, 15);
  RemoveConflicts(query);
  query = TestRangeBuilder(zone()).Id(4).Build(0, 60);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 2));
}


TEST_F(CoalescedLiveRangesTest, DeleteWhenConflictRepeatsAfterNonConflict) {
  LiveRange* range =
      TestRangeBuilder(zone()).Id(1).Add(1, 5).Add(6, 10).Add(20, 30).Build();
  ranges().AllocateRange(range);
  range = TestRangeBuilder(zone()).Id(2).Build(12, 15);
  ranges().AllocateRange(range);
  LiveRange* query =
      TestRangeBuilder(zone()).Id(3).Add(1, 8).Add(22, 25).Build();
  RemoveConflicts(query);
  query = TestRangeBuilder(zone()).Id(4).Build(0, 60);
  ASSERT_TRUE(ConflictsPreciselyWith(query, 2));
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
