// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_H_
#define V8_MAGLEV_MAGLEV_GRAPH_H_

#include <vector>

#include "src/maglev/maglev-basic-block.h"
#include "src/zone/zone-allocator.h"

namespace v8 {
namespace internal {
namespace maglev {

using BlockConstIterator =
    std::vector<BasicBlock*, ZoneAllocator<BasicBlock*>>::const_iterator;
using BlockConstReverseIterator =
    std::vector<BasicBlock*,
                ZoneAllocator<BasicBlock*>>::const_reverse_iterator;

class Graph final : public ZoneObject {
 public:
  static Graph* New(Zone* zone) { return zone->New<Graph>(zone); }

  // Shouldn't be used directly; public so that Zone::New can access it.
  explicit Graph(Zone* zone) : blocks_(zone) {}

  BasicBlock* operator[](int i) { return blocks_[i]; }
  const BasicBlock* operator[](int i) const { return blocks_[i]; }

  int num_blocks() const { return static_cast<int>(blocks_.size()); }

  BlockConstIterator begin() const { return blocks_.begin(); }
  BlockConstIterator end() const { return blocks_.end(); }
  BlockConstReverseIterator rbegin() const { return blocks_.rbegin(); }
  BlockConstReverseIterator rend() const { return blocks_.rend(); }

  BasicBlock* last_block() const { return blocks_.back(); }

  void Add(BasicBlock* block) { blocks_.push_back(block); }

  uint32_t tagged_stack_slots() const { return tagged_stack_slots_; }
  uint32_t untagged_stack_slots() const { return untagged_stack_slots_; }
  void set_tagged_stack_slots(uint32_t stack_slots) {
    DCHECK_EQ(kMaxUInt32, tagged_stack_slots_);
    DCHECK_NE(kMaxUInt32, stack_slots);
    tagged_stack_slots_ = stack_slots;
  }
  void set_untagged_stack_slots(uint32_t stack_slots) {
    DCHECK_EQ(kMaxUInt32, untagged_stack_slots_);
    DCHECK_NE(kMaxUInt32, stack_slots);
    untagged_stack_slots_ = stack_slots;
  }

  std::map<RootIndex, RootConstant*>& root() { return root_; }
  std::map<int, SmiConstant*>& smi() { return smi_; }
  std::map<int, Int32Constant*>& int32() { return int_; }
  std::map<double, Float64Constant*>& float64() { return float_; }
  std::vector<Constant*>& constants() { return constants_; }
  Float64Constant* nan() const { return nan_; }
  void set_nan(Float64Constant* nan) {
    DCHECK_NULL(nan_);
    nan_ = nan;
  }

  void AddConstant(Constant* constant) { constants_.emplace_back(constant); }

 private:
  uint32_t tagged_stack_slots_ = kMaxUInt32;
  uint32_t untagged_stack_slots_ = kMaxUInt32;
  ZoneVector<BasicBlock*> blocks_;
  std::map<RootIndex, RootConstant*> root_;
  std::map<int, SmiConstant*> smi_;
  std::map<int, Int32Constant*> int_;
  std::map<double, Float64Constant*> float_;
  std::vector<Constant*> constants_;
  Float64Constant* nan_ = nullptr;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_H_
