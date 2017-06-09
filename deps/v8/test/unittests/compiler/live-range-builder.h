// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIVE_RANGE_BUILDER_H_
#define V8_LIVE_RANGE_BUILDER_H_

#include "src/compiler/register-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {


// Utility offering shorthand syntax for building up a range by providing its ID
// and pairs (start, end) specifying intervals. Circumvents current incomplete
// support for C++ features such as instantiation lists, on OS X and Android.
class TestRangeBuilder {
 public:
  explicit TestRangeBuilder(Zone* zone)
      : id_(-1), pairs_(), uses_(), zone_(zone) {}

  TestRangeBuilder& Id(int id) {
    id_ = id;
    return *this;
  }
  TestRangeBuilder& Add(int start, int end) {
    pairs_.push_back({start, end});
    return *this;
  }

  TestRangeBuilder& AddUse(int pos) {
    uses_.insert(pos);
    return *this;
  }

  TopLevelLiveRange* Build(int start, int end) {
    return Add(start, end).Build();
  }

  TopLevelLiveRange* Build() {
    TopLevelLiveRange* range =
        new (zone_) TopLevelLiveRange(id_, MachineRepresentation::kTagged);
    // Traverse the provided interval specifications backwards, because that is
    // what LiveRange expects.
    for (int i = static_cast<int>(pairs_.size()) - 1; i >= 0; --i) {
      Interval pair = pairs_[i];
      LifetimePosition start = LifetimePosition::FromInt(pair.first);
      LifetimePosition end = LifetimePosition::FromInt(pair.second);
      CHECK(start < end);
      range->AddUseInterval(start, end, zone_);
    }
    for (int pos : uses_) {
      UsePosition* use_position =
          new (zone_) UsePosition(LifetimePosition::FromInt(pos), nullptr,
                                  nullptr, UsePositionHintType::kNone);
      range->AddUsePosition(use_position);
    }

    pairs_.clear();
    return range;
  }

 private:
  typedef std::pair<int, int> Interval;
  typedef std::vector<Interval> IntervalList;
  int id_;
  IntervalList pairs_;
  std::set<int> uses_;
  Zone* zone_;
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_LIVE_RANGE_BUILDER_H_
