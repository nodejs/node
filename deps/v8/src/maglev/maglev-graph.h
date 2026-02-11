// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_H_
#define V8_MAGLEV_MAGLEV_GRAPH_H_

#include "src/compiler/heap-refs.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-ir.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace maglev {

using BlockConstIterator = ZoneVector<BasicBlock*>::const_iterator;
using BlockConstReverseIterator =
    ZoneVector<BasicBlock*>::const_reverse_iterator;

struct MaglevCallSiteInfo;
class MaglevCallSiteInfoCompare {
 public:
  V8_EXPORT_PRIVATE bool operator()(const MaglevCallSiteInfo*,
                                    const MaglevCallSiteInfo*);
};
using MaglevCallSiteCandidates =
    ZonePriorityQueue<MaglevCallSiteInfo*, MaglevCallSiteInfoCompare>;

class Graph final : public ZoneObject {
 public:
  static Graph* New(MaglevCompilationInfo* info) {
    return info->zone()->New<Graph>(info);
  }

  // Shouldn't be used directly; public so that Zone::New can access it.
  explicit Graph(MaglevCompilationInfo* info)
      : compilation_info_(info),
        blocks_(zone()),
        osr_values_(zone()),
        root_constants_(zone()),
        smi_constants_(zone()),
        tagged_index_constants_(zone()),
        int32_constants_(zone()),
        uint32_constants_(zone()),
        shifted_int53_constants_(zone()),
        intptr_constants_(zone()),
        float64_constants_(zone()),
        holey_float64_constants_(zone()),
        heap_number_constants_(zone()),
        parameters_(zone()),
        eager_deopt_top_frames_(zone()),
        lazy_deopt_top_frames_(zone()),
        inlineable_calls_(zone()),
        allocations_escape_map_(zone()),
        allocations_elide_map_(zone()),
        register_inputs_(),
        constants_(zone()),
        trusted_constants_(zone()),
        inlined_functions_(zone()),
        scope_infos_(zone()) {}

  BasicBlock* operator[](int i) { return blocks_[i]; }
  const BasicBlock* operator[](int i) const { return blocks_[i]; }

  int num_blocks() const { return static_cast<int>(blocks_.size()); }
  ZoneVector<BasicBlock*>& blocks() { return blocks_; }

  BlockConstIterator begin() const { return blocks_.begin(); }
  BlockConstIterator end() const { return blocks_.end(); }
  BlockConstReverseIterator rbegin() const { return blocks_.rbegin(); }
  BlockConstReverseIterator rend() const { return blocks_.rend(); }

  BasicBlock* last_block() const { return blocks_.back(); }

  void Add(BasicBlock* block) {
    if (block->has_id()) {
      // The inliner adds blocks multiple times.
      DCHECK(v8_flags.maglev_non_eager_inlining ||
             v8_flags.turbolev_non_eager_inlining);
    } else {
      block->set_id(max_block_id_++);
    }
    blocks_.push_back(block);
  }

  void set_blocks(ZoneVector<BasicBlock*> blocks) { blocks_ = blocks; }

  void RemoveUnreachableBlocks();

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

  int total_inlined_bytecode_size_small() const {
    return total_inlined_bytecode_size_small_;
  }
  void add_inlined_bytecode_size_small(int size) {
    total_inlined_bytecode_size_small_ += size;
  }

  int total_peeled_bytecode_size() const { return total_peeled_bytecode_size_; }
  void add_peeled_bytecode_size(int size) {
    total_peeled_bytecode_size_ += size;
  }

  compiler::ZoneRefMap<compiler::HeapObjectRef, Constant*>& constants() {
    return constants_;
  }
  ZoneMap<RootIndex, RootConstant*>& root() { return root_constants_; }
  ZoneMap<int, SmiConstant*>& smi() { return smi_constants_; }
  ZoneMap<int, TaggedIndexConstant*>& tagged_index() {
    return tagged_index_constants_;
  }
  ZoneMap<int32_t, Int32Constant*>& int32() { return int32_constants_; }
  ZoneMap<ShiftedInt53, ShiftedInt53Constant*>& shifted_int53() {
    return shifted_int53_constants_;
  }
  ZoneMap<uint32_t, Uint32Constant*>& uint32() { return uint32_constants_; }
  ZoneMap<intptr_t, IntPtrConstant*>& intptr() { return intptr_constants_; }
  ZoneMap<uint64_t, Float64Constant*>& float64() { return float64_constants_; }
  ZoneMap<uint64_t, HoleyFloat64Constant*>& holey_float64() {
    return holey_float64_constants_;
  }
  ZoneMap<uint64_t, Constant*>& heap_number() { return heap_number_constants_; }
  compiler::ZoneRefMap<compiler::HeapObjectRef, TrustedConstant*>&
  trusted_constants() {
    return trusted_constants_;
  }

  ZoneVector<InitialValue*>& osr_values() { return osr_values_; }
  ZoneVector<InitialValue*>& parameters() { return parameters_; }

  MaglevCallSiteCandidates& inlineable_calls() { return inlineable_calls_; }

  const ZoneAbslFlatHashSet<DeoptFrame*>& eager_deopt_top_frames() const {
    return eager_deopt_top_frames_;
  }
  void AddEagerTopFrame(DeoptFrame* frame) {
    eager_deopt_top_frames_.insert(frame);
  }

  const ZoneAbslFlatHashMap<DeoptFrame*, std::pair<interpreter::Register, int>>&
  lazy_deopt_top_frames() const {
    return lazy_deopt_top_frames_;
  }
  void AddLazyTopFrame(DeoptFrame* frame, interpreter::Register result_location,
                       int result_size) {
    auto it = lazy_deopt_top_frames_.find(frame);
    if (it == lazy_deopt_top_frames_.end()) {
      lazy_deopt_top_frames_.emplace(
          frame, std::make_pair(result_location, result_size));
    }
  }

  // Running JS2, 99.99% of the cases, we have less than 2 dependencies.
  using SmallAllocationVector = SmallZoneVector<InlinedAllocation*, 2>;

  // If the key K of the map escape, all the set allocations_escape_map[K] must
  // also escape.
  ZoneMap<InlinedAllocation*, SmallAllocationVector>& allocations_escape_map() {
    return allocations_escape_map_;
  }
  // The K of the map can be elided if it hasn't escaped and all the set
  // allocations_elide_map[K] can also be elided.
  ZoneMap<InlinedAllocation*, SmallAllocationVector>& allocations_elide_map() {
    return allocations_elide_map_;
  }

  RegList& register_inputs() { return register_inputs_; }

  ZoneVector<OptimizedCompilationInfo::InlinedFunctionHolder>&
  inlined_functions() {
    return inlined_functions_;
  }
  bool has_recursive_calls() const { return has_recursive_calls_; }
  void set_has_recursive_calls(bool value) { has_recursive_calls_ = value; }

  bool is_osr() const {
    return compilation_info_->toplevel_compilation_unit()->is_osr();
  }
  uint32_t min_maglev_stackslots_for_unoptimized_frame_size() {
    DCHECK(is_osr());
    if (osr_values().size() == 0) {
      return InitialValue::stack_slot(0);
    }
    return osr_values().back()->stack_slot() + 1;
  }

  uint32_t NewObjectId() { return object_ids_++; }

  void set_has_resumable_generator() { has_resumable_generator_ = true; }
  bool has_resumable_generator() const { return has_resumable_generator_; }
  void set_may_have_unreachable_blocks(bool value = true) {
    may_have_unreachable_blocks_ = value;
  }
  bool may_have_unreachable_blocks() const {
    return may_have_unreachable_blocks_;
  }

  void set_may_have_truncation(bool value = true) {
    may_have_truncation_ = value;
  }
  bool may_have_truncation() const { return may_have_truncation_; }

  // Resolve the scope info of a context value.
  // An empty result means we don't statically know the context's scope.
  compiler::OptionalScopeInfoRef TryGetScopeInfo(ValueNode* context);
  bool ContextMayAlias(ValueNode* context,
                       compiler::OptionalScopeInfoRef scope_info);

  void record_scope_info(ValueNode* context,
                         compiler::OptionalScopeInfoRef scope_info) {
    scope_infos_[context] = scope_info;
  }

  SmiConstant* GetSmiConstant(int constant) {
    DCHECK(Smi::IsValid(constant));
    return GetOrAddNewConstantNode(smi_constants_, constant);
  }

  TaggedIndexConstant* GetTaggedIndexConstant(int constant) {
    DCHECK(TaggedIndex::IsValid(constant));
    return GetOrAddNewConstantNode(tagged_index_constants_, constant);
  }

  Int32Constant* GetInt32Constant(int32_t constant) {
    return GetOrAddNewConstantNode(int32_constants_, constant);
  }

  ShiftedInt53Constant* GetShiftedInt53Constant(ShiftedInt53 constant) {
    return GetOrAddNewConstantNode(shifted_int53_constants_, constant);
  }

  IntPtrConstant* GetIntPtrConstant(intptr_t constant) {
    return GetOrAddNewConstantNode(intptr_constants_, constant);
  }

  Uint32Constant* GetUint32Constant(uint32_t constant) {
    return GetOrAddNewConstantNode(uint32_constants_, constant);
  }

  Float64Constant* GetFloat64Constant(double constant) {
    return GetFloat64Constant(
        Float64::FromBits(base::double_to_uint64(constant)));
  }

  Float64Constant* GetFloat64Constant(Float64 constant) {
    return GetOrAddNewConstantNode(float64_constants_, constant.get_bits());
  }

  HoleyFloat64Constant* GetHoleyFloat64Constant(Float64 constant) {
    return GetOrAddNewConstantNode(holey_float64_constants_,
                                   constant.get_bits());
  }

  Constant* GetHeapNumberConstant(double constant);

  RootConstant* GetRootConstant(RootIndex index) {
    return GetOrAddNewConstantNode(root_constants_, index);
  }

  RootConstant* GetBooleanConstant(bool value) {
    return GetRootConstant(value ? RootIndex::kTrueValue
                                 : RootIndex::kFalseValue);
  }

  ValueNode* GetConstant(compiler::ObjectRef ref);

  ValueNode* GetTrustedConstant(compiler::HeapObjectRef ref,
                                IndirectPointerTag tag);

  Zone* zone() const { return compilation_info_->zone(); }
  compiler::JSHeapBroker* broker() const { return compilation_info_->broker(); }

  BasicBlock::Id max_block_id() const { return max_block_id_; }

  bool is_tracing_enabled() const {
    return compilation_info_->is_tracing_enabled();
  }

  bool has_graph_labeller() const {
    return compilation_info_->has_graph_labeller();
  }
  MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }

  MaglevCompilationInfo* compilation_info() const { return compilation_info_; }

 private:
  MaglevCompilationInfo* compilation_info_;
  uint32_t tagged_stack_slots_ = kMaxUInt32;
  uint32_t untagged_stack_slots_ = kMaxUInt32;
  uint32_t max_call_stack_args_ = kMaxUInt32;
  uint32_t max_deopted_stack_size_ = kMaxUInt32;
  ZoneVector<BasicBlock*> blocks_;
  ZoneVector<InitialValue*> osr_values_;
  ZoneMap<RootIndex, RootConstant*> root_constants_;
  ZoneMap<int, SmiConstant*> smi_constants_;
  ZoneMap<int, TaggedIndexConstant*> tagged_index_constants_;
  ZoneMap<int32_t, Int32Constant*> int32_constants_;
  ZoneMap<uint32_t, Uint32Constant*> uint32_constants_;
  ZoneMap<ShiftedInt53, ShiftedInt53Constant*> shifted_int53_constants_;
  ZoneMap<intptr_t, IntPtrConstant*> intptr_constants_;
  // Use the bits of the float as the key.
  ZoneMap<uint64_t, Float64Constant*> float64_constants_;
  ZoneMap<uint64_t, HoleyFloat64Constant*> holey_float64_constants_;
  ZoneMap<uint64_t, Constant*> heap_number_constants_;
  ZoneVector<InitialValue*> parameters_;
  ZoneAbslFlatHashSet<DeoptFrame*> eager_deopt_top_frames_;
  ZoneAbslFlatHashMap<DeoptFrame*, std::pair<interpreter::Register, int>>
      lazy_deopt_top_frames_;
  MaglevCallSiteCandidates inlineable_calls_;
  ZoneMap<InlinedAllocation*, SmallAllocationVector> allocations_escape_map_;
  ZoneMap<InlinedAllocation*, SmallAllocationVector> allocations_elide_map_;
  RegList register_inputs_;
  compiler::ZoneRefMap<compiler::HeapObjectRef, Constant*> constants_;
  compiler::ZoneRefMap<compiler::HeapObjectRef, TrustedConstant*>
      trusted_constants_;
  ZoneVector<OptimizedCompilationInfo::InlinedFunctionHolder>
      inlined_functions_;

  bool has_recursive_calls_ = false;
  int total_inlined_bytecode_size_ = 0;
  int total_inlined_bytecode_size_small_ = 0;
  int total_peeled_bytecode_size_ = 0;
  uint32_t object_ids_ = 0;
  bool has_resumable_generator_ = false;
  bool may_have_unreachable_blocks_ = false;
  bool may_have_truncation_ = false;
  ZoneUnorderedMap<ValueNode*, compiler::OptionalScopeInfoRef> scope_infos_;
  BasicBlock::Id max_block_id_ = 0;
  std::unique_ptr<MaglevGraphLabeller> graph_labeller_ = {};

  template <typename NodeT, typename... Args>
  NodeT* CreateNewConstantNode(Args&&... args) const {
    static_assert(IsConstantNode(Node::opcode_of<NodeT>));
    NodeT* node = NodeBase::New<NodeT>(zone(), std::forward<Args>(args)...);
    static_assert(!NodeT::kProperties.can_eager_deopt());
    static_assert(!NodeT::kProperties.can_lazy_deopt());
    static_assert(!NodeT::kProperties.can_throw());
    static_assert(!NodeT::kProperties.can_write());
    if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
    if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building &&
                    is_tracing_enabled())) {
      std::cout << "  " << node << "  " << PrintNodeLabel(node) << ": "
                << PrintNode(node) << std::endl;
    }
    return node;
  }

  template <typename NodeT, typename T>
  NodeT* GetOrAddNewConstantNode(ZoneMap<T, NodeT*>& container, T constant) {
    auto it = container.find(constant);
    if (it == container.end()) {
      NodeT* node = CreateNewConstantNode<NodeT>(0, constant);
      container.emplace(constant, node);
      return node;
    }
    return it->second;
  }

  compiler::OptionalScopeInfoRef TryGetScopeInfoForContextLoad(
      ValueNode* context, int offset);

  template <typename Function>
  void IterateGraphAndSweepDeadBlocks(Function&& is_dead);
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_H_
