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

  uint32_t stack_slots() const { return stack_slots_; }
  void set_stack_slots(uint32_t stack_slots) {
    DCHECK_EQ(kMaxUInt32, stack_slots_);
    DCHECK_NE(kMaxUInt32, stack_slots);
    stack_slots_ = stack_slots;
  }

 private:
  uint32_t stack_slots_ = kMaxUInt32;
  ZoneVector<BasicBlock*> blocks_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_H_
