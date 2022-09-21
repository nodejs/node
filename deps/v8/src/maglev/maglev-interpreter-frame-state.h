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
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"
#include "src/maglev/maglev-register-frame-array.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

class BasicBlock;
class MergePointInterpreterFrameState;

// Destructively intersects the right map into the left map, such that the
// left map is mutated to become the result of the intersection. Values that
// are in both maps are passed to the merging function to be merged with each
// other -- again, the LHS here is expected to be mutated.
template <typename Value, typename MergeFunc>
void DestructivelyIntersect(ZoneMap<ValueNode*, Value>& lhs_map,
                            const ZoneMap<ValueNode*, Value>& rhs_map,
                            MergeFunc&& func) {
  // Walk the two maps in lock step. This relies on the fact that ZoneMaps are
  // sorted.
  typename ZoneMap<ValueNode*, Value>::iterator lhs_it = lhs_map.begin();
  typename ZoneMap<ValueNode*, Value>::const_iterator rhs_it = rhs_map.begin();
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
}

// The intersection (using `&`) of any two NodeTypes must be a valid NodeType
// (possibly "kUnknown").
// TODO(leszeks): Figure out how to represent Number/Numeric with this encoding.
enum class NodeType {
  kUnknown = 0,
  kSmi = (1 << 0),
  kAnyHeapObject = (1 << 1),
  // All heap object types include the heap object bit, so that they can be
  // checked for AnyHeapObject with a single bit check.
  kString = (1 << 2) | kAnyHeapObject,
  kSymbol = (1 << 3) | kAnyHeapObject,
  kHeapNumber = (1 << 4) | kAnyHeapObject,
  kHeapObjectWithKnownMap = (1 << 5) | kAnyHeapObject,
};

struct NodeInfo {
  NodeType type;
  // TODO(leszeks): Consider adding more info for nodes here, e.g. alternative
  // representations or previously loaded fields.

  static bool IsSmi(const NodeInfo* info) {
    if (!info) return false;
    return info->type == NodeType::kSmi;
  }
  static bool IsAnyHeapObject(const NodeInfo* info) {
    if (!info) return false;
    return static_cast<int>(info->type) &
           static_cast<int>(NodeType::kAnyHeapObject);
  }
  static bool IsString(const NodeInfo* info) {
    if (!info) return false;
    return info->type == NodeType::kString;
  }
  static bool IsSymbol(const NodeInfo* info) {
    if (!info) return false;
    return info->type == NodeType::kSymbol;
  }

  // Mutate this node info by merging in another node info, with the result
  // being a node info that is the subset of information valid in both inputs.
  void MergeWith(const NodeInfo& other) {
    type = static_cast<NodeType>(static_cast<int>(type) &
                                 static_cast<int>(other.type));
  }
};

struct KnownNodeAspects {
  explicit KnownNodeAspects(Zone* zone)
      : node_infos(zone), stable_maps(zone), unstable_maps(zone) {}

  KnownNodeAspects(const KnownNodeAspects& other) = delete;
  KnownNodeAspects& operator=(const KnownNodeAspects& other) = delete;
  KnownNodeAspects(KnownNodeAspects&& other) = delete;
  KnownNodeAspects& operator=(KnownNodeAspects&& other) = delete;

  KnownNodeAspects* Clone(Zone* zone) const {
    KnownNodeAspects* clone = zone->New<KnownNodeAspects>(zone);
    clone->node_infos = node_infos;
    clone->stable_maps = stable_maps;
    clone->unstable_maps = unstable_maps;
    return clone;
  }

  // Loop headers can safely clone the node types, since those won't be
  // invalidated in the loop body, and similarly stable maps will have
  // dependencies installed. Unstable maps however might be invalidated by
  // calls, and we don't know about these until it's too late.
  KnownNodeAspects* CloneWithoutUnstableMaps(Zone* zone) const {
    KnownNodeAspects* clone = zone->New<KnownNodeAspects>(zone);
    clone->node_infos = node_infos;
    clone->stable_maps = stable_maps;
    return clone;
  }

  NodeInfo* GetInfoFor(ValueNode* node) {
    auto it = node_infos.find(node);
    if (it == node_infos.end()) return nullptr;
    return &it->second;
  }

  void InsertOrUpdateNodeType(ValueNode* node, NodeInfo* existing_info,
                              NodeType new_type) {
    if (existing_info == nullptr) {
      DCHECK_EQ(node_infos.find(node), node_infos.end());
      node_infos.emplace(node, NodeInfo{new_type});
    } else {
      DCHECK_EQ(&node_infos.find(node)->second, existing_info);
      existing_info->type = new_type;
    }
  }

  void Merge(const KnownNodeAspects& other) {
    DestructivelyIntersect(node_infos, other.node_infos,
                           [](NodeInfo& lhs, const NodeInfo& rhs) {
                             lhs.MergeWith(rhs);
                             return lhs.type != NodeType::kUnknown;
                           });
    DestructivelyIntersect(stable_maps, other.stable_maps,
                           [](compiler::MapRef lhs, compiler::MapRef rhs) {
                             return lhs.equals(rhs);
                           });
    DestructivelyIntersect(unstable_maps, other.unstable_maps,
                           [](compiler::MapRef lhs, compiler::MapRef rhs) {
                             return lhs.equals(rhs);
                           });
  }

  // TODO(leszeks): Store these more efficiently than with std::map -- in
  // particular, clear out entries that are no longer reachable, perhaps also
  // allow lookup by interpreter register rather than by node pointer.

  // Permanently valid if checked in a dominator.
  ZoneMap<ValueNode*, NodeInfo> node_infos;
  // Valid across side-effecting calls, as long as we install a dependency.
  ZoneMap<ValueNode*, compiler::MapRef> stable_maps;
  // Flushed after side-effecting calls.
  ZoneMap<ValueNode*, compiler::MapRef> unstable_maps;
};

class InterpreterFrameState {
 public:
  explicit InterpreterFrameState(const MaglevCompilationUnit& info)
      : frame_(info),
        known_node_aspects_(info.zone()->New<KnownNodeAspects>(info.zone())) {}

  inline void CopyFrom(const MaglevCompilationUnit& info,
                       const MergePointInterpreterFrameState& state);

  void set_accumulator(ValueNode* value) {
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

  KnownNodeAspects& known_node_aspects() { return *known_node_aspects_; }
  const KnownNodeAspects& known_node_aspects() const {
    return *known_node_aspects_;
  }

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
  ValueNode* accumulator(const MaglevCompilationUnit& info) const {
    DCHECK(liveness_->AccumulatorIsLive());
    return live_registers_and_accumulator_[size(info) - 1];
  }

  ValueNode*& context(const MaglevCompilationUnit& info) {
    return live_registers_and_accumulator_[info.parameter_count()];
  }
  ValueNode* context(const MaglevCompilationUnit& info) const {
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
    for (Register reg : kAllocatableGeneralRegisters) {
      f(reg, *current_value);
      ++current_value;
    }
  }

  template <typename Function>
  void ForEachDoubleRegister(Function&& f) {
    RegisterState* current_value = &double_values_[0];
    for (DoubleRegister reg : kAllocatableDoubleRegisters) {
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
  void CheckIsLoopPhiIfNeeded(const MaglevCompilationUnit& compilation_unit,
                              int merge_offset, interpreter::Register reg,
                              ValueNode* value) {
#ifdef DEBUG
    const auto& analysis = compilation_unit.bytecode_analysis();
    if (!analysis.IsLoopHeader(merge_offset)) return;
    auto& assignments = analysis.GetLoopInfoFor(merge_offset).assignments();
    if (reg.is_parameter()) {
      if (reg.is_current_context()) return;
      if (!assignments.ContainsParameter(reg.ToParameterIndex())) return;
    } else {
      DCHECK(
          analysis.GetInLivenessFor(merge_offset)->RegisterIsLive(reg.index()));
      if (!assignments.ContainsLocal(reg.index())) return;
    }
    DCHECK(value->Is<Phi>());
#endif
  }

  static MergePointInterpreterFrameState* New(
      const MaglevCompilationUnit& info, const InterpreterFrameState& state,
      int merge_offset, int predecessor_count, BasicBlock* predecessor,
      const compiler::BytecodeLivenessState* liveness) {
    MergePointInterpreterFrameState* merge_state =
        info.zone()->New<MergePointInterpreterFrameState>(
            info, predecessor_count, 1,
            info.zone()->NewArray<BasicBlock*>(predecessor_count),
            BasicBlockType::kDefault, liveness);
    merge_state->frame_state_.ForEachValue(
        info, [&](ValueNode*& entry, interpreter::Register reg) {
          entry = state.get(reg);
        });
    merge_state->predecessors_[0] = predecessor;
    merge_state->known_node_aspects_ =
        info.zone()->New<KnownNodeAspects>(info.zone());
    return merge_state;
  }

  static MergePointInterpreterFrameState* NewForLoop(
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, const compiler::BytecodeLivenessState* liveness,
      const compiler::LoopInfo* loop_info) {
    MergePointInterpreterFrameState* state =
        info.zone()->New<MergePointInterpreterFrameState>(
            info, predecessor_count, 0,
            info.zone()->NewArray<BasicBlock*>(predecessor_count),
            BasicBlockType::kLoopHeader, liveness);
    auto& assignments = loop_info->assignments();
    auto& frame_state = state->frame_state_;
    frame_state.ForEachParameter(
        info, [&](ValueNode*& entry, interpreter::Register reg) {
          entry = nullptr;
          if (assignments.ContainsParameter(reg.ToParameterIndex())) {
            entry = state->NewLoopPhi(info.zone(), reg, merge_offset);
          }
        });
    // TODO(v8:7700): Add contexts into assignment analysis.
    frame_state.context(info) = state->NewLoopPhi(
        info.zone(), interpreter::Register::current_context(), merge_offset);
    frame_state.ForEachLocal(
        info, [&](ValueNode*& entry, interpreter::Register reg) {
          entry = nullptr;
          if (assignments.ContainsLocal(reg.index())) {
            entry = state->NewLoopPhi(info.zone(), reg, merge_offset);
          }
        });
    DCHECK(!frame_state.liveness()->AccumulatorIsLive());
    return state;
  }

  static MergePointInterpreterFrameState* NewForCatchBlock(
      const MaglevCompilationUnit& unit,
      const compiler::BytecodeLivenessState* liveness, int handler_offset,
      interpreter::Register context_register, Graph* graph, bool is_inline);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void Merge(MaglevCompilationUnit& compilation_unit,
             const InterpreterFrameState& unmerged, BasicBlock* predecessor,
             int merge_offset) {
    DCHECK_GT(predecessor_count_, 1);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    predecessors_[predecessors_so_far_] = predecessor;

    if (known_node_aspects_ == nullptr) {
      DCHECK(is_unmerged_loop());
      DCHECK_EQ(predecessors_so_far_, 0);
      known_node_aspects_ =
          unmerged.known_node_aspects().CloneWithoutUnstableMaps(
              compilation_unit.zone());
    } else {
      known_node_aspects_->Merge(unmerged.known_node_aspects());
    }

    if (FLAG_trace_maglev_graph_building) {
      std::cout << "Merging..." << std::endl;
    }
    frame_state_.ForEachValue(compilation_unit, [&](ValueNode*& value,
                                                    interpreter::Register reg) {
      CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);

      if (FLAG_trace_maglev_graph_building) {
        std::cout << "  " << reg.ToString() << ": "
                  << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                  << " <- "
                  << PrintNodeLabel(compilation_unit.graph_labeller(),
                                    unmerged.get(reg));
      }
      value = MergeValue(compilation_unit, reg, value, unmerged.get(reg),
                         merge_offset);
      if (FLAG_trace_maglev_graph_building) {
        std::cout << " => "
                  << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                  << ": " << PrintNode(compilation_unit.graph_labeller(), value)
                  << std::endl;
      }
    });
    predecessors_so_far_++;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
  }

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeLoop(MaglevCompilationUnit& compilation_unit,
                 const InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block, int merge_offset) {
    // This should be the last predecessor we try to merge.
    DCHECK_EQ(predecessors_so_far_, predecessor_count_ - 1);
    DCHECK(is_unmerged_loop());
    predecessors_[predecessor_count_ - 1] = loop_end_block;

    if (FLAG_trace_maglev_graph_building) {
      std::cout << "Merging loop backedge..." << std::endl;
    }
    frame_state_.ForEachValue(compilation_unit, [&](ValueNode* value,
                                                    interpreter::Register reg) {
      CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);

      if (FLAG_trace_maglev_graph_building) {
        std::cout << "  " << reg.ToString() << ": "
                  << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                  << " <- "
                  << PrintNodeLabel(compilation_unit.graph_labeller(),
                                    loop_end_state.get(reg));
      }
      MergeLoopValue(compilation_unit, reg, value, loop_end_state.get(reg),
                     merge_offset);
      if (FLAG_trace_maglev_graph_building) {
        std::cout << " => "
                  << PrintNodeLabel(compilation_unit.graph_labeller(), value)
                  << ": " << PrintNode(compilation_unit.graph_labeller(), value)
                  << std::endl;
      }
    });
    predecessors_so_far_++;
    DCHECK_EQ(predecessors_so_far_, predecessor_count_);
  }

  // Merges a dead framestate (e.g. one which has been early terminated with a
  // deopt).
  void MergeDead(const MaglevCompilationUnit& compilation_unit,
                 int merge_offset) {
    DCHECK_GT(predecessor_count_, 1);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    predecessor_count_--;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);

    frame_state_.ForEachValue(
        compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
          CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);
          ReducePhiPredecessorCount(reg, value, merge_offset);
        });
  }

  // Merges a dead loop framestate (e.g. one where the block containing the
  // JumpLoop has been early terminated with a deopt).
  void MergeDeadLoop(const MaglevCompilationUnit& compilation_unit,
                     int merge_offset) {
    // This should be the last predecessor we try to merge.
    DCHECK_EQ(predecessors_so_far_, predecessor_count_ - 1);
    DCHECK(is_unmerged_loop());
    MergeDead(compilation_unit, merge_offset);
    // This means that this is no longer a loop.
    basic_block_type_ = BasicBlockType::kDefault;
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
    return basic_block_type_ == BasicBlockType::kLoopHeader;
  }

  bool is_exception_handler() const {
    return basic_block_type_ == BasicBlockType::kExceptionHandlerStart;
  }

  bool is_unmerged_loop() const {
    // If this is a loop and not all predecessors are set, then the loop isn't
    // merged yet.
    DCHECK_GT(predecessor_count_, 0);
    return is_loop() && predecessors_so_far_ < predecessor_count_;
  }

  bool is_unreachable_loop() const {
    // If there is only one predecessor, and it's not set, then this is a loop
    // merge with no forward control flow entering it.
    return is_loop() && predecessor_count_ == 1 && predecessors_so_far_ == 0;
  }

 private:
  friend void InterpreterFrameState::CopyFrom(
      const MaglevCompilationUnit& info,
      const MergePointInterpreterFrameState& state);

  template <typename T, typename... Args>
  friend T* Zone::New(Args&&... args);

  MergePointInterpreterFrameState(
      const MaglevCompilationUnit& info, int predecessor_count,
      int predecessors_so_far, BasicBlock** predecessors, BasicBlockType type,
      const compiler::BytecodeLivenessState* liveness)
      : predecessor_count_(predecessor_count),
        predecessors_so_far_(predecessors_so_far),
        predecessors_(predecessors),
        basic_block_type_(type),
        frame_state_(info, liveness) {}

  ValueNode* FromInt32ToTagged(MaglevCompilationUnit& compilation_unit,
                               ValueNode* value) {
    DCHECK_EQ(value->properties().value_representation(),
              ValueRepresentation::kInt32);
    if (value->Is<CheckedSmiUntag>()) {
      return value->input(0).node();
    }
#define IS_INT32_OP_NODE(Name) || value->Is<Name>()
    DCHECK(value->Is<Int32Constant>()
               INT32_OPERATIONS_NODE_LIST(IS_INT32_OP_NODE));
#undef IS_INT32_OP_NODE
    // Check if the next Node in the block after value is its CheckedSmiTag
    // version and reuse it.
    if (value->NextNode()) {
      CheckedSmiTag* tagged = value->NextNode()->TryCast<CheckedSmiTag>();
      if (tagged != nullptr && value == tagged->input().node()) {
        return tagged;
      }
    }
    // Otherwise create a tagged version.
    ValueNode* tagged =
        Node::New<CheckedSmiTag, std::initializer_list<ValueNode*>>(
            compilation_unit.zone(), compilation_unit,
            value->eager_deopt_info()->state, {value});
    Node::List::AddAfter(value, tagged);
    compilation_unit.RegisterNodeInGraphLabeller(tagged);
    return tagged;
  }

  ValueNode* FromFloat64ToTagged(MaglevCompilationUnit& compilation_unit,
                                 ValueNode* value) {
    DCHECK_EQ(value->properties().value_representation(),
              ValueRepresentation::kFloat64);
    if (value->Is<CheckedFloat64Unbox>()) {
      return value->input(0).node();
    }
    if (value->Is<ChangeInt32ToFloat64>()) {
      return FromInt32ToTagged(compilation_unit, value->input(0).node());
    }
    // Check if the next Node in the block after value is its Float64Box
    // version and reuse it.
    if (value->NextNode()) {
      Float64Box* tagged = value->NextNode()->TryCast<Float64Box>();
      if (tagged != nullptr && value == tagged->input().node()) {
        return tagged;
      }
    }
    // Otherwise create a tagged version.
    ValueNode* tagged = Node::New<Float64Box>(compilation_unit.zone(), {value});
    Node::List::AddAfter(value, tagged);
    compilation_unit.RegisterNodeInGraphLabeller(tagged);
    return tagged;
  }

  // TODO(victorgomes): Consider refactor this function to share code with
  // MaglevGraphBuilder::GetTagged.
  ValueNode* EnsureTagged(MaglevCompilationUnit& compilation_unit,
                          ValueNode* value) {
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kTagged:
        return value;
      case ValueRepresentation::kInt32:
        return FromInt32ToTagged(compilation_unit, value);
      case ValueRepresentation::kFloat64:
        return FromFloat64ToTagged(compilation_unit, value);
    }
  }

  ValueNode* MergeValue(MaglevCompilationUnit& compilation_unit,
                        interpreter::Register owner, ValueNode* merged,
                        ValueNode* unmerged, int merge_offset) {
    // If the merged node is null, this is a pre-created loop header merge
    // frame will null values for anything that isn't a loop Phi.
    if (merged == nullptr) {
      DCHECK(is_unmerged_loop());
      DCHECK_EQ(predecessors_so_far_, 0);
      return unmerged;
    }

    Phi* result = merged->TryCast<Phi>();
    if (result != nullptr && result->merge_offset() == merge_offset) {
      // It's possible that merged == unmerged at this point since loop-phis are
      // not dropped if they are only assigned to themselves in the loop.
      DCHECK_EQ(result->owner(), owner);
      unmerged = EnsureTagged(compilation_unit, unmerged);
      result->set_input(predecessors_so_far_, unmerged);
      return result;
    }

    if (merged == unmerged) return merged;

    // We guarantee that the values are tagged.
    // TODO(victorgomes): Support Phi nodes of untagged values.
    merged = EnsureTagged(compilation_unit, merged);
    unmerged = EnsureTagged(compilation_unit, unmerged);

    // Tagged versions could point to the same value, avoid Phi nodes in this
    // case.
    if (merged == unmerged) return merged;

    // Up to this point all predecessors had the same value for this interpreter
    // frame slot. Now that we find a distinct value, insert a copy of the first
    // value for each predecessor seen so far, in addition to the new value.
    // TODO(verwaest): Unclear whether we want this for Maglev: Instead of
    // letting the register allocator remove phis, we could always merge through
    // the frame slot. In that case we only need the inputs for representation
    // selection, and hence could remove duplicate inputs. We'd likely need to
    // attach the interpreter register to the phi in that case?
    result = Node::New<Phi>(compilation_unit.zone(), predecessor_count_, owner,
                            merge_offset);

    for (int i = 0; i < predecessors_so_far_; i++) result->set_input(i, merged);
    result->set_input(predecessors_so_far_, unmerged);
    if (FLAG_trace_maglev_graph_building) {
      for (int i = predecessors_so_far_ + 1; i < predecessor_count_; i++) {
        result->set_input(i, nullptr);
      }
    }

    phis_.Add(result);
    return result;
  }

  void ReducePhiPredecessorCount(interpreter::Register owner, ValueNode* merged,
                                 int merge_offset) {
    // If the merged node is null, this is a pre-created loop header merge
    // frame with null values for anything that isn't a loop Phi.
    if (merged == nullptr) {
      DCHECK(is_unmerged_loop());
      DCHECK_EQ(predecessors_so_far_, 0);
      return;
    }

    Phi* result = merged->TryCast<Phi>();
    if (result != nullptr && result->merge_offset() == merge_offset) {
      // It's possible that merged == unmerged at this point since loop-phis are
      // not dropped if they are only assigned to themselves in the loop.
      DCHECK_EQ(result->owner(), owner);
      result->reduce_input_count();
    }
  }

  void MergeLoopValue(MaglevCompilationUnit& compilation_unit,
                      interpreter::Register owner, ValueNode* merged,
                      ValueNode* unmerged, int merge_offset) {
    Phi* result = merged->TryCast<Phi>();
    if (result == nullptr || result->merge_offset() != merge_offset) {
      if (merged != unmerged) {
        // TODO(leszeks): These DCHECKs are too restrictive, investigate making
        // them looser.
        // DCHECK(unmerged->Is<CheckedSmiUntag>() ||
        //        unmerged->Is<CheckedFloat64Unbox>());
        // DCHECK_EQ(merged, unmerged->input(0).node());
      }
      return;
    }
    DCHECK_EQ(result->owner(), owner);
    unmerged = EnsureTagged(compilation_unit, unmerged);
    result->set_input(predecessor_count_ - 1, unmerged);
  }

  ValueNode* NewLoopPhi(Zone* zone, interpreter::Register reg,
                        int merge_offset) {
    DCHECK_EQ(predecessors_so_far_, 0);
    // Create a new loop phi, which for now is empty.
    Phi* result = Node::New<Phi>(zone, predecessor_count_, reg, merge_offset);
    if (FLAG_trace_maglev_graph_building) {
      for (int i = 0; i < predecessor_count_; i++) {
        result->set_input(i, nullptr);
      }
    }
    phis_.Add(result);
    return result;
  }

  ValueNode* NewExceptionPhi(Zone* zone, interpreter::Register reg,
                             int handler_offset) {
    DCHECK_EQ(predecessors_so_far_, 0);
    DCHECK_EQ(predecessor_count_, 0);
    DCHECK_NULL(predecessors_);
    Phi* result = Node::New<Phi>(zone, 0, reg, handler_offset);
    phis_.Add(result);
    return result;
  }

  int predecessor_count_;
  int predecessors_so_far_;
  BasicBlock** predecessors_;

  BasicBlockType basic_block_type_;
  Phi::List phis_;

  CompactInterpreterFrameState frame_state_;
  MergePointRegisterState register_state_;
  KnownNodeAspects* known_node_aspects_ = nullptr;
};

void InterpreterFrameState::CopyFrom(
    const MaglevCompilationUnit& info,
    const MergePointInterpreterFrameState& state) {
  state.frame_state().ForEachValue(
      info, [&](ValueNode* value, interpreter::Register reg) {
        frame_[reg] = value;
      });
  // Move "what we know" across without copying -- we can safely mutate it
  // now, as we won't be entering this merge point again.
  known_node_aspects_ = state.known_node_aspects_;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
