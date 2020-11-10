// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_LIVE_RANGE_SEPARATOR_H_
#define V8_COMPILER_BACKEND_LIVE_RANGE_SEPARATOR_H_

#include "src/zone/zone.h"
namespace v8 {
namespace internal {

class Zone;

namespace compiler {

class RegisterAllocationData;

// A register allocation pair of transformations: splinter and merge live ranges
class LiveRangeSeparator final : public ZoneObject {
 public:
  LiveRangeSeparator(RegisterAllocationData* data, Zone* zone)
      : data_(data), zone_(zone) {}

  void Splinter();

 private:
  RegisterAllocationData* data() const { return data_; }
  Zone* zone() const { return zone_; }

  RegisterAllocationData* const data_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeSeparator);
};

class LiveRangeMerger final : public ZoneObject {
 public:
  LiveRangeMerger(RegisterAllocationData* data, Zone* zone)
      : data_(data), zone_(zone) {}

  void Merge();

 private:
  RegisterAllocationData* data() const { return data_; }
  Zone* zone() const { return zone_; }

  // Mark ranges spilled in deferred blocks, that also cover non-deferred code.
  // We do nothing special for ranges fully contained in deferred blocks,
  // because they would "spill in deferred blocks" anyway.
  void MarkRangesSpilledInDeferredBlocks();

  RegisterAllocationData* const data_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeMerger);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8
#endif  // V8_COMPILER_BACKEND_LIVE_RANGE_SEPARATOR_H_
