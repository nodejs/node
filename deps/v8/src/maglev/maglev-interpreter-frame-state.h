// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
#define V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_

#include "src/base/logging.h"
#include "src/base/threaded-list.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"
#include "src/maglev/maglev-register-frame-array.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

class BasicBlock;
class Graph;
class MaglevGraphBuilder;
class MergePointInterpreterFrameState;

// Destructively intersects the right map into the left map, such that the
// left map is mutated to become the result of the intersection. Values that
// are in both maps are passed to the merging function to be merged with each
// other -- again, the LHS here is expected to be mutated.
template <typename Key, typename Value,
          typename MergeFunc = std::equal_to<Value>>
void DestructivelyIntersect(ZoneMap<Key, Value>& lhs_map,
                            const ZoneMap<Key, Value>& rhs_map,
                            MergeFunc&& func = MergeFunc()) {
  // Walk the two maps in lock step. This relies on the fact that ZoneMaps are
  // sorted.
  typename ZoneMap<Key, Value>::iterator lhs_it = lhs_map.begin();
  typename ZoneMap<Key, Value>::const_iterator rhs_it = rhs_map.begin();
  while (lhs_it != lhs_map.end() && rhs_it != rhs_map.end()) {
    if (lhs_it->first < rhs_it->first) {
      // Remove from LHS elements that are not in RHS.
      lhs_it = lhs_map.erase(lhs_it);
    } else if (rhs_it->first < lhs_it->first) {
      // Skip over elements that are only in RHS.
      ++rhs_it;
    } else {
      // Apply the merge function to the values of the two iterators. If the
      // function returns false, remove the value.
      bool keep_value = func(lhs_it->second, rhs_it->second);
      if (keep_value) {
        ++lhs_it;
      } else {
        lhs_it = lhs_map.erase(lhs_it);
      }
      ++rhs_it;
    }
  }
  // If we haven't reached the end of LHS by now, then we have reached the end
  // of RHS, and the remaining items are therefore not in RHS. Remove them.
  if (lhs_it != lhs_map.end()) {
    lhs_map.erase(lhs_it, lhs_map.end());
  }
}

struct NodeInfo {
  NodeType type = NodeType::kUnknown;

  // Optional alternative nodes with the equivalent value but a different
  // representation.
  // TODO(leszeks): At least one of these is redundant for every node, consider
  // a more compressed form or even linked list.
  ValueNode* tagged_alternative = nullptr;
  ValueNode* int32_alternative = nullptr;
  ValueNode* float64_alternative = nullptr;
  ValueNode* constant_alternative = nullptr;
  // Alternative nodes with a value equivalent to the ToNumber of this node.
  ValueNode* truncated_int32_to_number = nullptr;

  bool is_empty() {
    return type == NodeType::kUnknown && tagged_alternative == nullptr &&
           int32_alternative == nullptr && float64_alternative == nullptr &&
           truncated_int32_to_number == nullptr &&
           constant_alternative == nullptr;
  }

  bool is_smi() const { return NodeTypeIsSmi(type); }
  bool is_any_heap_object() const { return NodeTypeIsAnyHeapObject(type); }
  bool is_string() const { return NodeTypeIsString(type); }
  bool is_internalized_string() const {
    return NodeTypeIsInternalizedString(type);
  }
  bool is_symbol() const { return NodeTypeIsSymbol(type); }
  bool is_constant() const { return constant_alternative != nullptr; }

  // Mutate this node info by merging in another node info, with the result
  // being a node info that is the subset of information valid in both inputs.
  void MergeWith(const NodeInfo& other) {
    type = IntersectType(type, other.type);
    tagged_alternative = tagged_alternative == other.tagged_alternative
                             ? tagged_alternative
                             : nullptr;
    int32_alternative = int32_alternative == other.int32_alternative
                            ? int32_alternative
                            : nullptr;
    float64_alternative = float64_alternative == other.float64_alternative
                              ? float64_alternative
                              : nullptr;
    constant_alternative = constant_alternative == other.constant_alternative
                               ? constant_alternative
                               : nullptr;
    truncated_int32_to_number =
        truncated_int32_to_number == other.truncated_int32_to_number
            ? truncated_int32_to_number
            : nullptr;
  }
};

struct KnownNodeAspects {
  explicit KnownNodeAspects(Zone* zone)
      : node_infos(zone),
        possible_maps(zone),
        any_map_for_any_node_is_unstable(false),
        loaded_constant_properties(zone),
        loaded_properties(zone),
        loaded_context_constants(zone),
        loaded_context_slots(zone) {}

  // Copy constructor is defaulted but private so that we explicitly call the
  // Clone method.
  KnownNodeAspects& operator=(const KnownNodeAspects& other) = delete;
  KnownNodeAspects(KnownNodeAspects&& other) = delete;
  KnownNodeAspects& operator=(KnownNodeAspects&& other) = delete;

  KnownNodeAspects* Clone(Zone* zone) const {
    return zone->New<KnownNodeAspects>(*this);
  }

  // Loop headers can safely clone the node types, since those won't be
  // invalidated in the loop body, and similarly stable maps will have
  // dependencies installed. Unstable maps however might be invalidated by
  // calls, and we don't know about these until it's too late.
  KnownNodeAspects* CloneForLoopHeader(Zone* zone) const {
    KnownNodeAspects* clone = zone->New<KnownNodeAspects>(zone);
    clone->node_infos = node_infos;
    clone->possible_maps = possible_maps;
    if (any_map_for_any_node_is_unstable) {
      clone->any_map_for_any_node_is_unstable = true;
      clone->ClearUnstableMaps();
      DCHECK(!clone->any_map_for_any_node_is_unstable);
    }
    clone->loaded_constant_properties = loaded_constant_properties;
    clone->loaded_context_constants = loaded_context_constants;
    return clone;
  }

  void ClearUnstableMaps() {
    // A side effect could change existing objects' maps. For stable maps we
    // know this hasn't happened (because we added a dependency on the maps
    // staying stable and therefore not possible to transition away from), but
    // we can no longer assume that objects with unstable maps still have the
    // same map. Unstable maps can also transition to stable ones, so we have to
    // clear _all_ maps for a node if it had _any_ unstable map.
    if (!any_map_for_any_node_is_unstable) return;
    for (auto it = possible_maps.begin(); it != possible_maps.end();) {
      if (it->second.any_map_is_unstable) {
        it = possible_maps.erase(it);
      } else {
        ++it;
      }
    }
    any_map_for_any_node_is_unstable = false;
  }

  ZoneMap<ValueNode*, NodeInfo>::iterator FindInfo(ValueNode* node) {
    return node_infos.find(node);
  }
  ZoneMap<ValueNode*, NodeInfo>::const_iterator FindInfo(
      ValueNode* node) const {
    return node_infos.find(node);
  }
  bool IsValid(ZoneMap<ValueNode*, NodeInfo>::iterator& it) {
    return it != node_infos.end();
  }
  bool IsValid(ZoneMap<ValueNode*, NodeInfo>::const_iterator& it) const {
    return it != node_infos.end();
  }

  const NodeInfo* TryGetInfoFor(ValueNode* node) const {
    auto info_it = FindInfo(node);
    if (!IsValid(info_it)) return nullptr;
    return &info_it->second;
  }
  NodeInfo* GetOrCreateInfoFor(ValueNode* node) { return &node_infos[node]; }

  void Merge(const KnownNodeAspects& other, Zone* zone);

  // TODO(leszeks): Store these more efficiently than with std::map -- in
  // particular, clear out entries that are no longer reachable, perhaps also
  // allow lookup by interpreter register rather than by node pointer.

  // Permanently valid if checked in a dominator.
  ZoneMap<ValueNode*, NodeInfo> node_infos;

  struct PossibleMaps {
    // TODO(v8:7700): Investigate a better data structure to use than
    // compiler::ZoneRefSet.
    compiler::ZoneRefSet<Map> possible_maps;
    // TODO(leszeks): Consider storing this in a more compact way.
    bool any_map_is_unstable;
  };
  // Maps for a node. Sets of maps that only contain stable maps are valid
  // across side-effecting calls, as long as we install a dependency, otherwise
  // they are cleared on side-effects.
  // TODO(v8:7700): Investigate a better data structure to use than ZoneMap.
  ZoneMap<ValueNode*, PossibleMaps> possible_maps;
  bool any_map_for_any_node_is_unstable;

  // Cached property loads.

  // Maps name->object->value, so that stores to a name can invalidate all loads
  // of that name (in case the objects are aliasing).
  using LoadedPropertyMap =
      ZoneMap<compiler::NameRef, ZoneMap<ValueNode*, ValueNode*>>;

  // Valid across side-effecting calls, as long as we install a dependency.
  LoadedPropertyMap loaded_constant_properties;
  // Flushed after side-effecting calls.
  LoadedPropertyMap loaded_properties;

  // Unconditionally valid across side-effecting calls.
  ZoneMap<std::tuple<ValueNode*, int>, ValueNode*> loaded_context_constants;
  // Flushed after side-effecting calls.
  ZoneMap<std::tuple<ValueNode*, int>, ValueNode*> loaded_context_slots;

 private:
  friend KnownNodeAspects* Zone::New<KnownNodeAspects, const KnownNodeAspects&>(
      const KnownNodeAspects&);
  KnownNodeAspects(const KnownNodeAspects& other) V8_NOEXCEPT = default;
};

class InterpreterFrameState {
 public:
  InterpreterFrameState(const MaglevCompilationUnit& info,
                        KnownNodeAspects* known_node_aspects)
      : frame_(info), known_node_aspects_(known_node_aspects) {}

  explicit InterpreterFrameState(const MaglevCompilationUnit& info)
      : InterpreterFrameState(
            info, info.zone()->New<KnownNodeAspects>(info.zone())) {}

  inline void CopyFrom(const MaglevCompilationUnit& info,
                       MergePointInterpreterFrameState& state);

  void set_accumulator(ValueNode* value) {
    // Conversions should be stored in known_node_aspects/NodeInfo.
    DCHECK(!value->properties().is_conversion());
    frame_[interpreter::Register::virtual_accumulator()] = value;
  }
  ValueNode* accumulator() const {
    return frame_[interpreter::Register::virtual_accumulator()];
  }

  void set(interpreter::Register reg, ValueNode* value) {
    DCHECK_IMPLIES(reg.is_parameter(),
                   reg == interpreter::Register::current_context() ||
                       reg == interpreter::Register::function_closure() ||
                       reg == interpreter::Register::virtual_accumulator() ||
                       reg.ToParameterIndex() >= 0);
    // Conversions should be stored in known_node_aspects/NodeInfo.
    DCHECK(!value->properties().is_conversion());
    frame_[reg] = value;
  }
  ValueNode* get(interpreter::Register reg) const {
    DCHECK_IMPLIES(reg.is_parameter(),
                   reg == interpreter::Register::current_context() ||
                       reg == interpreter::Register::function_closure() ||
                       reg == interpreter::Register::virtual_accumulator() ||
                       reg.ToParameterIndex() >= 0);
    return frame_[reg];
  }

  const RegisterFrameArray<ValueNode*>& frame() const { return frame_; }

  KnownNodeAspects* known_node_aspects() { return known_node_aspects_; }
  const KnownNodeAspects* known_node_aspects() const {
    return known_node_aspects_;
  }

  void set_known_node_aspects(KnownNodeAspects* known_node_aspects) {
    DCHECK_NOT_NULL(known_node_aspects);
    known_node_aspects_ = known_node_aspects;
  }

  void clear_known_node_aspects() { known_node_aspects_ = nullptr; }

 private:
  RegisterFrameArray<ValueNode*> frame_;
  KnownNodeAspects* known_node_aspects_;
};

class CompactInterpreterFrameState {
 public:
  CompactInterpreterFrameState(const MaglevCompilationUnit& info,
                               const compiler::BytecodeLivenessState* liveness)
      : live_registers_and_accumulator_(
            info.zone()->NewArray<ValueNode*>(SizeFor(info, liveness))),
        liveness_(liveness) {}

  CompactInterpreterFrameState(const MaglevCompilationUnit& info,
                               const compiler::BytecodeLivenessState* liveness,
                               const InterpreterFrameState& state)
      : CompactInterpreterFrameState(info, liveness) {
    ForEachValue(info, [&](ValueNode*& entry, interpreter::Register reg) {
      entry = state.get(reg);
    });
  }

  CompactInterpreterFrameState(const CompactInterpreterFrameState&) = delete;
  CompactInterpreterFrameState(CompactInterpreterFrameState&&) = delete;
  CompactInterpreterFrameState& operator=(const CompactInterpreterFrameState&) =
      delete;
  CompactInterpreterFrameState& operator=(CompactInterpreterFrameState&&) =
      delete;

  template <typename Function>
  void ForEachParameter(const MaglevCompilationUnit& info, Function&& f) const {
    for (int i = 0; i < info.parameter_count(); i++) {
      interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
      f(live_registers_and_accumulator_[i], reg);
    }
  }

  template <typename Function>
  void ForEachParameter(const MaglevCompilationUnit& info, Function&& f) {
    for (int i = 0; i < info.parameter_count(); i++) {
      interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
      f(live_registers_and_accumulator_[i], reg);
    }
  }

  template <typename Function>
  void ForEachLocal(const MaglevCompilationUnit& info, Function&& f) const {
    int live_reg = 0;
    for (int register_index : *liveness_) {
      interpreter::Register reg = interpreter::Register(register_index);
      f(live_registers_and_accumulator_[info.parameter_count() +
                                        context_register_count_ + live_reg++],
        reg);
    }
  }

  template <typename Function>
  void ForEachLocal(const MaglevCompilationUnit& info, Function&& f) {
    int live_reg = 0;
    for (int register_index : *liveness_) {
      interpreter::Register reg = interpreter::Register(register_index);
      f(live_registers_and_accumulator_[info.parameter_count() +
                                        context_register_count_ + live_reg++],
        reg);
    }
  }

  template <typename Function>
  void ForEachRegister(const MaglevCompilationUnit& info, Function&& f) {
    ForEachParameter(info, f);
    f(context(info), interpreter::Register::current_context());
    ForEachLocal(info, f);
  }

  template <typename Function>
  void ForEachRegister(const MaglevCompilationUnit& info, Function&& f) const {
    ForEachParameter(info, f);
    f(context(info), interpreter::Register::current_context());
    ForEachLocal(info, f);
  }

  template <typename Function>
  void ForEachValue(const MaglevCompilationUnit& info, Function&& f) {
    ForEachRegister(info, f);
    if (liveness_->AccumulatorIsLive()) {
      f(accumulator(info), interpreter::Register::virtual_accumulator());
    }
  }

  template <typename Function>
  void ForEachValue(const MaglevCompilationUnit& info, Function&& f) const {
    ForEachRegister(info, f);
    if (liveness_->AccumulatorIsLive()) {
      f(accumulator(info), interpreter::Register::virtual_accumulator());
    }
  }

  const compiler::BytecodeLivenessState* liveness() const { return liveness_; }

  ValueNode*& accumulator(const MaglevCompilationUnit& info) {
    DCHECK(liveness_->AccumulatorIsLive());
    return live_registers_and_accumulator_[size(info) - 1];
  }
  ValueNode*& accumulator(const MaglevCompilationUnit& info) const {
    DCHECK(liveness_->AccumulatorIsLive());
    return live_registers_and_accumulator_[size(info) - 1];
  }

  ValueNode*& context(const MaglevCompilationUnit& info) {
    return live_registers_and_accumulator_[info.parameter_count()];
  }
  ValueNode*& context(const MaglevCompilationUnit& info) const {
    return live_registers_and_accumulator_[info.parameter_count()];
  }

  ValueNode* GetValueOf(interpreter::Register reg,
                        const MaglevCompilationUnit& info) const {
    DCHECK(reg.is_valid());
    if (reg == interpreter::Register::current_context()) {
      return context(info);
    }
    if (reg == interpreter::Register::virtual_accumulator()) {
      return accumulator(info);
    }
    if (reg.is_parameter()) {
      DCHECK_LT(reg.ToParameterIndex(), info.parameter_count());
      return live_registers_and_accumulator_[reg.ToParameterIndex()];
    }
    int live_reg = 0;
    // TODO(victorgomes): See if we can do better than a linear search here.
    for (int register_index : *liveness_) {
      if (reg == interpreter::Register(register_index)) {
        return live_registers_and_accumulator_[info.parameter_count() +
                                               context_register_count_ +
                                               live_reg];
      }
      live_reg++;
    }
    // No value in this frame state.
    return nullptr;
  }

  size_t size(const MaglevCompilationUnit& info) const {
    return SizeFor(info, liveness_);
  }

 private:
  static size_t SizeFor(const MaglevCompilationUnit& info,
                        const compiler::BytecodeLivenessState* liveness) {
    return info.parameter_count() + context_register_count_ +
           liveness->live_value_count();
  }

  // TODO(leszeks): Only include the context register if there are any
  // Push/PopContext calls.
  static const int context_register_count_ = 1;
  ValueNode** const live_registers_and_accumulator_;
  const compiler::BytecodeLivenessState* const liveness_;
};

class MergePointRegisterState {
 public:
  bool is_initialized() const { return values_[0].GetPayload().is_initialized; }

  template <typename Function>
  void ForEachGeneralRegister(Function&& f) {
    RegisterState* current_value = &values_[0];
    for (Register reg : MaglevAssembler::GetAllocatableRegisters()) {
      f(reg, *current_value);
      ++current_value;
    }
  }

  template <typename Function>
  void ForEachDoubleRegister(Function&& f) {
    RegisterState* current_value = &double_values_[0];
    for (DoubleRegister reg :
         MaglevAssembler::GetAllocatableDoubleRegisters()) {
      f(reg, *current_value);
      ++current_value;
    }
  }

 private:
  RegisterState values_[kAllocatableGeneralRegisterCount] = {{}};
  RegisterState double_values_[kAllocatableDoubleRegisterCount] = {{}};
};

class MergePointInterpreterFrameState {
 public:
  enum class BasicBlockType {
    kDefault,
    kLoopHeader,
    kExceptionHandlerStart,
  };

  static MergePointInterpreterFrameState* New(
      const MaglevCompilationUnit& info, const InterpreterFrameState& state,
      int merge_offset, int predecessor_count, BasicBlock* predecessor,
      const compiler::BytecodeLivenessState* liveness);

  static MergePointInterpreterFrameState* NewForLoop(
      const InterpreterFrameState& start_state,
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, const compiler::BytecodeLivenessState* liveness,
      const compiler::LoopInfo* loop_info, bool has_been_peeled = false);

  static MergePointInterpreterFrameState* NewForCatchBlock(
      const MaglevCompilationUnit& unit,
      const compiler::BytecodeLivenessState* liveness, int handler_offset,
      interpreter::Register context_register, Graph* graph);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void Merge(MaglevGraphBuilder* graph_builder, InterpreterFrameState& unmerged,
             BasicBlock* predecessor);
  void Merge(MaglevGraphBuilder* graph_builder,
             MaglevCompilationUnit& compilation_unit,
             InterpreterFrameState& unmerged, BasicBlock* predecessor);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeLoop(MaglevGraphBuilder* graph_builder,
                 InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block);
  void MergeLoop(MaglevGraphBuilder* graph_builder,
                 MaglevCompilationUnit& compilation_unit,
                 InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeThrow(MaglevGraphBuilder* builder,
                  const MaglevCompilationUnit* handler_unit,
                  InterpreterFrameState& unmerged);

  // Merges a dead framestate (e.g. one which has been early terminated with a
  // deopt).
  void MergeDead(const MaglevCompilationUnit& compilation_unit) {
    DCHECK_GE(predecessor_count_, 1);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    predecessor_count_--;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);

    frame_state_.ForEachValue(compilation_unit,
                              [&](ValueNode* value, interpreter::Register reg) {
                                ReducePhiPredecessorCount(reg, value);
                              });
  }

  // Merges a dead loop framestate (e.g. one where the block containing the
  // JumpLoop has been early terminated with a deopt).
  void MergeDeadLoop(const MaglevCompilationUnit& compilation_unit) {
    // This should be the last predecessor we try to merge.
    DCHECK_EQ(predecessors_so_far_, predecessor_count_ - 1);
    DCHECK(is_unmerged_loop());
    MergeDead(compilation_unit);
    // This means that this is no longer a loop.
    bitfield_ =
        kBasicBlockTypeBits::update(bitfield_, BasicBlockType::kDefault);
  }

  // Returns and clears the known node aspects on this state. Expects to only
  // ever be called once, when starting a basic block with this state.
  KnownNodeAspects* TakeKnownNodeAspects() {
    DCHECK_NOT_NULL(known_node_aspects_);
    return std::exchange(known_node_aspects_, nullptr);
  }

  const CompactInterpreterFrameState& frame_state() const {
    return frame_state_;
  }
  MergePointRegisterState& register_state() { return register_state_; }

  bool has_phi() const { return !phis_.is_empty(); }
  Phi::List* phis() { return &phis_; }

  void SetPhis(Phi::List&& phis) {
    // Move the collected phis to the live interpreter frame.
    DCHECK(phis_.is_empty());
    phis_.MoveTail(&phis, phis.begin());
  }

  int predecessor_count() const { return predecessor_count_; }

  BasicBlock* predecessor_at(int i) const {
    // DCHECK_EQ(predecessors_so_far_, predecessor_count_);
    DCHECK_LT(i, predecessor_count_);
    return predecessors_[i];
  }

  bool is_loop() const {
    return basic_block_type() == BasicBlockType::kLoopHeader;
  }

  bool is_exception_handler() const {
    return basic_block_type() == BasicBlockType::kExceptionHandlerStart;
  }

  bool is_unmerged_loop() const {
    // If this is a loop and not all predecessors are set, then the loop isn't
    // merged yet.
    DCHECK_IMPLIES(is_loop(), predecessor_count_ > 0);
    return is_loop() && predecessors_so_far_ < predecessor_count_;
  }

  bool is_unreachable_loop() const {
    // If there is only one predecessor, and it's not set, then this is a loop
    // merge with no forward control flow entering it.
    return is_loop() && !is_resumable_loop() && predecessor_count_ == 1 &&
           predecessors_so_far_ == 0;
  }

  BasicBlockType basic_block_type() const {
    return kBasicBlockTypeBits::decode(bitfield_);
  }
  bool is_resumable_loop() const {
    return kIsResumableLoopBit::decode(bitfield_);
  }
  bool is_loop_with_peeled_iteration() const {
    return kIsLoopWithPeeledIterationBit::decode(bitfield_);
  }

  int merge_offset() const { return merge_offset_; }

  DeoptFrame* backedge_deopt_frame() const { return backedge_deopt_frame_; }

  const compiler::LoopInfo* loop_info() const {
    DCHECK(loop_info_.has_value());
    return loop_info_.value();
  }

 private:
  using kBasicBlockTypeBits = base::BitField<BasicBlockType, 0, 2>;
  using kIsResumableLoopBit = kBasicBlockTypeBits::Next<bool, 1>;
  using kIsLoopWithPeeledIterationBit = kIsResumableLoopBit::Next<bool, 1>;

  // For each non-Phi value in the frame state, store its alternative
  // representations to avoid re-converting on Phi creation.
  class Alternatives {
   public:
    using List = base::ThreadedList<Alternatives>;

    explicit Alternatives(const NodeInfo* node_info)
        : node_type_(node_info ? node_info->type : NodeType::kUnknown),
          tagged_alternative_(node_info ? node_info->tagged_alternative
                                        : nullptr) {}

    NodeType node_type() const { return node_type_; }
    ValueNode* tagged_alternative() const { return tagged_alternative_; }

   private:
    Alternatives** next() { return &next_; }

    // For now, Phis are tagged, so only store the tagged alternative.
    NodeType node_type_;
    ValueNode* tagged_alternative_;
    Alternatives* next_ = nullptr;
    friend base::ThreadedListTraits<Alternatives>;
  };
  NodeType AlternativeType(const Alternatives* alt);

  template <typename T, typename... Args>
  friend T* Zone::New(Args&&... args);

  MergePointInterpreterFrameState(
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, int predecessors_so_far, BasicBlock** predecessors,
      BasicBlockType type, const compiler::BytecodeLivenessState* liveness);

  ValueNode* MergeValue(MaglevGraphBuilder* graph_builder,
                        interpreter::Register owner,
                        const KnownNodeAspects& unmerged_aspects,
                        ValueNode* merged, ValueNode* unmerged,
                        Alternatives::List* per_predecessor_alternatives);

  void ReducePhiPredecessorCount(interpreter::Register owner,
                                 ValueNode* merged);

  void MergeLoopValue(MaglevGraphBuilder* graph_builder,
                      interpreter::Register owner,
                      KnownNodeAspects& unmerged_aspects, ValueNode* merged,
                      ValueNode* unmerged);

  ValueNode* NewLoopPhi(Zone* zone, interpreter::Register reg);

  ValueNode* NewExceptionPhi(Zone* zone, interpreter::Register reg) {
    DCHECK_EQ(predecessor_count_, 0);
    DCHECK_NULL(predecessors_);
    Phi* result = Node::New<Phi>(zone, 0, this, reg);
    phis_.Add(result);
    return result;
  }

  int merge_offset_;

  int predecessor_count_;
  int predecessors_so_far_;

  uint32_t bitfield_;

  BasicBlock** predecessors_;

  Phi::List phis_;

  CompactInterpreterFrameState frame_state_;
  MergePointRegisterState register_state_;
  KnownNodeAspects* known_node_aspects_ = nullptr;

  union {
    // {pre_predecessor_alternatives_} is used to keep track of the alternatives
    // of Phi inputs. Once the block has been merged, it's not used anymore.
    Alternatives::List* per_predecessor_alternatives_;
    // {backedge_deopt_frame_} is used to record the deopt frame for the
    // backedge, in case we want to insert a deopting conversion during phi
    // untagging. It is set when visiting the JumpLoop (and will only be set for
    // loop headers), when the header has already been merged and
    // {per_predecessor_alternatives_} is thus not used anymore.
    DeoptFrame* backedge_deopt_frame_;
  };

  base::Optional<const compiler::LoopInfo*> loop_info_ = base::nullopt;
};

void InterpreterFrameState::CopyFrom(const MaglevCompilationUnit& info,
                                     MergePointInterpreterFrameState& state) {
  state.frame_state().ForEachValue(
      info, [&](ValueNode* value, interpreter::Register reg) {
        frame_[reg] = value;
      });
  // Move "what we know" across without copying -- we can safely mutate it
  // now, as we won't be entering this merge point again.
  known_node_aspects_ = state.TakeKnownNodeAspects();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
