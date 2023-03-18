// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_H_
#define V8_MAGLEV_MAGLEV_GRAPH_H_

#include <vector>

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/heap-refs.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/zone/zone-allocator.h"

namespace v8 {
namespace internal {
namespace maglev {

using BlockConstIterator = ZoneVector<BasicBlock*>::const_iterator;
using BlockConstReverseIterator =
    ZoneVector<BasicBlock*>::const_reverse_iterator;

class Graph final : public ZoneObject {
 public:
  static Graph* New(Zone* zone) { return zone->New<Graph>(zone); }

  // Shouldn't be used directly; public so that Zone::New can access it.
  explicit Graph(Zone* zone)
      : blocks_(zone),
        root_(zone),
        smi_(zone),
        int_(zone),
        float_(zone),
        external_references_(zone),
        parameters_(zone),
        register_inputs_(),
        constants_(zone),
        inlined_functions_(zone) {}

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
  uint32_t max_call_stack_args() const { return max_call_stack_args_; }
  uint32_t max_deopted_stack_size() const { return max_deopted_stack_size_; }
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
  void set_max_call_stack_args(uint32_t stack_slots) {
    DCHECK_EQ(kMaxUInt32, max_call_stack_args_);
    DCHECK_NE(kMaxUInt32, stack_slots);
    max_call_stack_args_ = stack_slots;
  }
  void set_max_deopted_stack_size(uint32_t size) {
    DCHECK_EQ(kMaxUInt32, max_deopted_stack_size_);
    DCHECK_NE(kMaxUInt32, size);
    max_deopted_stack_size_ = size;
  }

  int total_inlined_bytecode_size() const {
    return total_inlined_bytecode_size_;
  }
  void add_inlined_bytecode_size(int size) {
    total_inlined_bytecode_size_ += size;
  }

  uint32_t stack_check_offset() {
    uint32_t stack_slots = tagged_stack_slots_ + untagged_stack_slots_;
    DCHECK(is_int32(stack_slots));
    int32_t optimized_frame_height = stack_slots * kSystemPointerSize;
    DCHECK(is_int32(max_deopted_stack_size_));
    int32_t signed_max_unoptimized_frame_height =
        static_cast<int32_t>(max_deopted_stack_size_);

    // The offset is either the delta between the optimized frames and the
    // interpreted frame, or the maximal number of bytes pushed to the stack
    // while preparing for function calls, whichever is bigger.
    uint32_t frame_height_delta = static_cast<uint32_t>(std::max(
        signed_max_unoptimized_frame_height - optimized_frame_height, 0));
    uint32_t max_pushed_argument_bytes =
        static_cast<uint32_t>(max_call_stack_args_ * kSystemPointerSize);
    return std::max(frame_height_delta, max_pushed_argument_bytes);
  }

  ZoneMap<RootIndex, RootConstant*>& root() { return root_; }
  ZoneMap<int, SmiConstant*>& smi() { return smi_; }
  ZoneMap<int, Int32Constant*>& int32() { return int_; }
  ZoneMap<double, Float64Constant*>& float64() { return float_; }
  ZoneMap<Address, ExternalConstant*>& external_references() {
    return external_references_;
  }
  ZoneVector<InitialValue*>& parameters() { return parameters_; }
  RegList& register_inputs() { return register_inputs_; }
  compiler::ZoneRefMap<compiler::ObjectRef, Constant*>& constants() {
    return constants_;
  }
  ZoneVector<OptimizedCompilationInfo::InlinedFunctionHolder>&
  inlined_functions() {
    return inlined_functions_;
  }
  Float64Constant* nan() const { return nan_; }
  void set_nan(Float64Constant* nan) {
    DCHECK_NULL(nan_);
    nan_ = nan;
  }
  FunctionEntryStackCheck* function_entry_stack_check() const {
    return function_entry_stack_check_;
  }
  void set_function_entry_stack_check(
      FunctionEntryStackCheck* function_entry_stack_check) {
    DCHECK_NULL(function_entry_stack_check_);
    function_entry_stack_check_ = function_entry_stack_check;
  }
  bool has_recursive_calls() const { return has_recursive_calls_; }
  void set_has_recursive_calls(bool value) { has_recursive_calls_ = value; }

 private:
  uint32_t tagged_stack_slots_ = kMaxUInt32;
  uint32_t untagged_stack_slots_ = kMaxUInt32;
  uint32_t max_call_stack_args_ = kMaxUInt32;
  uint32_t max_deopted_stack_size_ = kMaxUInt32;
  ZoneVector<BasicBlock*> blocks_;
  ZoneMap<RootIndex, RootConstant*> root_;
  ZoneMap<int, SmiConstant*> smi_;
  ZoneMap<int, Int32Constant*> int_;
  ZoneMap<double, Float64Constant*> float_;
  ZoneMap<Address, ExternalConstant*> external_references_;
  ZoneVector<InitialValue*> parameters_;
  RegList register_inputs_;
  compiler::ZoneRefMap<compiler::ObjectRef, Constant*> constants_;
  ZoneVector<OptimizedCompilationInfo::InlinedFunctionHolder>
      inlined_functions_;
  Float64Constant* nan_ = nullptr;
  FunctionEntryStackCheck* function_entry_stack_check_ = nullptr;
  bool has_recursive_calls_ = false;
  int total_inlined_bytecode_size_ = 0;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_H_
