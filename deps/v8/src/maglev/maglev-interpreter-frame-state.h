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
#include "src/zone/zone-handle-set.h"
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

// The intersection (using `&`) of any two NodeTypes must be a valid NodeType
// (possibly "kUnknown").
// All heap object types include the heap object bit, so that they can be
// checked for AnyHeapObject with a single bit check.
#define NODE_TYPE_LIST(V)                                         \
  V(Unknown, 0)                                                   \
  V(NumberOrOddball, (1 << 1))                                    \
  V(Number, (1 << 2) | kNumberOrOddball)                          \
  V(Oddball, (1 << 3) | kNumberOrOddball)                         \
  V(ObjectWithKnownMap, (1 << 4))                                 \
  V(Smi, (1 << 5) | kObjectWithKnownMap | kNumber)                \
  V(AnyHeapObject, (1 << 6))                                      \
  V(Name, (1 << 7) | kAnyHeapObject)                              \
  V(String, (1 << 8) | kName)                                     \
  V(InternalizedString, (1 << 9) | kString)                       \
  V(Symbol, (1 << 10) | kName)                                    \
  V(JSReceiver, (1 << 11) | kAnyHeapObject)                       \
  V(HeapObjectWithKnownMap, kObjectWithKnownMap | kAnyHeapObject) \
  V(HeapNumber, kHeapObjectWithKnownMap | kNumber)                \
  V(JSReceiverWithKnownMap, kJSReceiver | kHeapObjectWithKnownMap)

enum class NodeType {
#define DEFINE_NODE_TYPE(Name, Value) k##Name = Value,
  NODE_TYPE_LIST(DEFINE_NODE_TYPE)
#undef DEFINE_NODE_TYPE
};

inline NodeType CombineType(NodeType left, NodeType right) {
  return static_cast<NodeType>(static_cast<int>(left) |
                               static_cast<int>(right));
}
inline NodeType IntersectType(NodeType left, NodeType right) {
  return static_cast<NodeType>(static_cast<int>(left) &
                               static_cast<int>(right));
}
inline bool NodeTypeIs(NodeType type, NodeType to_check) {
  int right = static_cast<int>(to_check);
  return (static_cast<int>(type) & right) == right;
}

#define DEFINE_NODE_TYPE_CHECK(Type, _)         \
  inline bool NodeTypeIs##Type(NodeType type) { \
    return NodeTypeIs(type, NodeType::k##Type); \
  }
NODE_TYPE_LIST(DEFINE_NODE_TYPE_CHECK)
#undef DEFINE_NODE_TYPE_CHECK

struct NodeInfo {
  NodeType type = NodeType::kUnknown;

  // Optional alternative nodes with the equivalent value but a different
  // representation.
  // TODO(leszeks): At least one of these is redundant for every node, consider
  // a more compressed form or even linked list.
  ValueNode* tagged_alternative = nullptr;
  ValueNode* int32_alternative = nullptr;
  ValueNode* float64_alternative = nullptr;
  ValueNode* truncated_int32_alternative = nullptr;

  bool is_empty() {
    return type == NodeType::kUnknown && tagged_alternative == nullptr &&
           int32_alternative == nullptr && float64_alternative == nullptr;
  }

  bool is_smi() const { return NodeTypeIsSmi(type); }
  bool is_any_heap_object() const { return NodeTypeIsAnyHeapObject(type); }
  bool is_string() const { return NodeTypeIsString(type); }
  bool is_internalized_string() const {
    return NodeTypeIsInternalizedString(type);
  }
  bool is_symbol() const { return NodeTypeIsSymbol(type); }

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
    truncated_int32_alternative =
        truncated_int32_alternative == other.truncated_int32_alternative
            ? truncated_int32_alternative
            : nullptr;
  }
};

struct KnownNodeAspects {
  explicit KnownNodeAspects(Zone* zone)
      : node_infos(zone),
        stable_maps(zone),
        unstable_maps(zone),
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
    clone->stable_maps = stable_maps;
    clone->loaded_constant_properties = loaded_constant_properties;
    clone->loaded_context_constants = loaded_context_constants;
    return clone;
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
  // TODO(v8:7700): Investigate a better data structure to use than
  // ZoneHandleSet.
  // Valid across side-effecting calls, as long as we install a dependency.
  ZoneMap<ValueNode*, ZoneHandleSet<Map>> stable_maps;
  // Flushed after side-effecting calls.
  ZoneMap<ValueNode*, ZoneHandleSet<Map>> unstable_maps;

  // Valid across side-effecting calls, as long as we install a dependency.
  ZoneMap<std::pair<ValueNode*, compiler::NameRef>, ValueNode*>
      loaded_constant_properties;
  // Flushed after side-effecting calls.
  ZoneMap<std::pair<ValueNode*, compiler::NameRef>, ValueNode*>
      loaded_properties;

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
                       const MergePointInterpreterFrameState& state);

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

  static MergePointInterpreterFrameState* New(
      const MaglevCompilationUnit& info, const InterpreterFrameState& state,
      int merge_offset, int predecessor_count, BasicBlock* predecessor,
      const compiler::BytecodeLivenessState* liveness);

  static MergePointInterpreterFrameState* NewForLoop(
      const InterpreterFrameState& start_state,
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, const compiler::BytecodeLivenessState* liveness,
      const compiler::LoopInfo* loop_info);

  static MergePointInterpreterFrameState* NewForCatchBlock(
      const MaglevCompilationUnit& unit,
      const compiler::BytecodeLivenessState* liveness, int handler_offset,
      interpreter::Register context_register, Graph* graph, bool is_inline);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void Merge(MaglevCompilationUnit& compilation_unit,
             ZoneMap<int, SmiConstant*>& smi_constants,
             InterpreterFrameState& unmerged, BasicBlock* predecessor);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeLoop(MaglevCompilationUnit& compilation_unit,
                 ZoneMap<int, SmiConstant*>& smi_constants,
                 InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block);

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
    return is_loop() && !is_resumable_loop() && predecessor_count_ == 1 &&
           predecessors_so_far_ == 0;
  }

  bool is_resumable_loop() const { return is_resumable_loop_; }

  int merge_offset() const { return merge_offset_; }

 private:
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

  friend void InterpreterFrameState::CopyFrom(
      const MaglevCompilationUnit& info,
      const MergePointInterpreterFrameState& state);

  template <typename T, typename... Args>
  friend T* Zone::New(Args&&... args);

  MergePointInterpreterFrameState(
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, int predecessors_so_far, BasicBlock** predecessors,
      BasicBlockType type, const compiler::BytecodeLivenessState* liveness);

  ValueNode* MergeValue(MaglevCompilationUnit& compilation_unit,
                        ZoneMap<int, SmiConstant*>& smi_constants,
                        interpreter::Register owner,
                        const KnownNodeAspects& unmerged_aspects,
                        ValueNode* merged, ValueNode* unmerged,
                        Alternatives::List& per_predecessor_alternatives);

  void ReducePhiPredecessorCount(interpreter::Register owner,
                                 ValueNode* merged);

  void MergeLoopValue(MaglevCompilationUnit& compilation_unit,
                      ZoneMap<int, SmiConstant*>& smi_constants,
                      interpreter::Register owner,
                      KnownNodeAspects& unmerged_aspects, ValueNode* merged,
                      ValueNode* unmerged);

  ValueNode* NewLoopPhi(Zone* zone, interpreter::Register reg);

  ValueNode* NewExceptionPhi(Zone* zone, interpreter::Register reg) {
    DCHECK_EQ(predecessors_so_far_, 0);
    DCHECK_EQ(predecessor_count_, 0);
    DCHECK_NULL(predecessors_);
    Phi* result = Node::New<Phi>(zone, 0, this, reg);
    phis_.Add(result);
    return result;
  }

  int merge_offset_;

  int predecessor_count_;
  int predecessors_so_far_;
  bool is_resumable_loop_ = false;
  BasicBlock** predecessors_;

  BasicBlockType basic_block_type_;
  Phi::List phis_;

  CompactInterpreterFrameState frame_state_;
  MergePointRegisterState register_state_;
  KnownNodeAspects* known_node_aspects_ = nullptr;
  Alternatives::List* per_predecessor_alternatives_;
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
