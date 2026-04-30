// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
#define V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_

#include <optional>

#include "src/base/logging.h"
#include "src/base/threaded-list.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-known-node-aspects.h"
#ifdef V8_ENABLE_MAGLEV
#include "src/maglev/maglev-regalloc-data.h"
#endif
#include "src/maglev/maglev-register-frame-array.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

class BasicBlock;
class Graph;
class MaglevGraphBuilder;
class MergePointInterpreterFrameState;
struct LoopEffects;

class InterpreterFrameState {
 public:
  InterpreterFrameState(const MaglevCompilationUnit& info,
                        KnownNodeAspects* known_node_aspects)
      : frame_(info), known_node_aspects_(known_node_aspects) {
    frame_[interpreter::Register::virtual_accumulator()] = nullptr;
  }

  explicit InterpreterFrameState(const MaglevCompilationUnit& info)
      : InterpreterFrameState(
            info, info.zone()->New<KnownNodeAspects>(info.zone())) {}

  inline void CopyFrom(const MaglevCompilationUnit& info,
                       MergePointInterpreterFrameState& state,
                       bool preserve_known_node_aspects, Zone* zone);

  void set_accumulator(ValueNode* value) {
    // Conversions should be stored in known_node_aspects/NodeInfo.
    DCHECK(!value->is_conversion());
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
    DCHECK(!value->is_conversion());
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

  void add_object(VirtualObject* vobject) {
    known_node_aspects_->virtual_objects().Add(vobject);
  }
  const VirtualObjectList& virtual_objects() const {
    DCHECK_NOT_NULL(known_node_aspects_);
    return known_node_aspects_->virtual_objects();
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
            info.zone()->AllocateArray<ValueNode*>(SizeFor(info, liveness))),
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
#ifdef V8_ENABLE_MAGLEV

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
#endif  // V8_ENABLE_MAGLEV
};

class MergePointInterpreterFrameState {
 public:
  enum class BasicBlockType {
    kDefault,
    kLoopHeader,
    kExceptionHandlerStart,
    kUnusedExceptionHandlerStart,
  };

  static MergePointInterpreterFrameState* New(
      const MaglevCompilationUnit& info, const InterpreterFrameState& state,
      int merge_offset, int predecessor_count, BasicBlock* predecessor,
      const compiler::BytecodeLivenessState* liveness);

  static MergePointInterpreterFrameState* NewForLoop(
      const InterpreterFrameState& start_state,
      const MaglevGraphBuilder* builder, const MaglevCompilationUnit& info,
      int merge_offset, int predecessor_count,
      const compiler::BytecodeLivenessState* liveness,
      const compiler::LoopInfo* loop_info, bool has_been_peeled = false);

  static MergePointInterpreterFrameState* NewForCatchBlock(
      const MaglevCompilationUnit& unit,
      const compiler::BytecodeLivenessState* liveness, int handler_offset,
      bool was_used, interpreter::Register context_register, Graph* graph);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void Merge(MaglevGraphBuilder* graph_builder, InterpreterFrameState& unmerged,
             BasicBlock* predecessor);
  void Merge(MaglevGraphBuilder* graph_builder,
             MaglevCompilationUnit& compilation_unit,
             InterpreterFrameState& unmerged, BasicBlock* predecessor);
  void InitializeLoop(MaglevGraphBuilder* graph_builder,
                      MaglevCompilationUnit& compilation_unit,
                      InterpreterFrameState& unmerged, BasicBlock* predecessor,
                      bool optimistic_initial_state = false,
                      LoopEffects* loop_effects = nullptr);
  void InitializeWithBasicBlock(BasicBlock* current_block);

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeLoop(MaglevGraphBuilder* graph_builder,
                 InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block);
  void MergeLoop(MaglevGraphBuilder* graph_builder,
                 MaglevCompilationUnit& compilation_unit,
                 InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block);
  void set_loop_effects(LoopEffects* loop_effects);
  const LoopEffects* loop_effects();
  // Merges a frame-state that might not be mergable, in which case we need to
  // re-compile the loop again. Calls FinishBlock only if the merge succeeded.
  bool TryMergeLoop(MaglevGraphBuilder* graph_builder,
                    InterpreterFrameState& loop_end_state,
                    const std::function<BasicBlock*()>& FinishBlock);

  // Merges an unmerged framestate into a possibly merged framestate at the
  // start of the target catchblock.
  void MergeThrow(MaglevGraphBuilder* handler_builder,
                  const MaglevCompilationUnit* handler_unit,
                  const KnownNodeAspects& known_node_aspects);

  // Merges a dead framestate (e.g. one which has been early terminated with a
  // deopt).
  void MergeDead(const MaglevCompilationUnit& compilation_unit,
                 unsigned num = 1) {
    DCHECK_GE(predecessor_count_, num);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    ReducePhiPredecessorCount(num);
    predecessor_count_ -= num;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
  }

  void clear_is_loop() {
    bitfield_ =
        kBasicBlockTypeBits::update(bitfield_, BasicBlockType::kDefault);
    bitfield_ = kIsResumableLoopBit::update(bitfield_, false);
  }

  // Merges a dead loop framestate (e.g. one where the block containing the
  // JumpLoop has been early terminated with a deopt).
  void MergeDeadLoop(const MaglevCompilationUnit& compilation_unit) {
    // This should be the last predecessor we try to merge.
    DCHECK_EQ(predecessors_so_far_, predecessor_count_ - 1);
    DCHECK(is_unmerged_loop());
    MergeDead(compilation_unit);
    // This means that this is no longer a loop.
    clear_is_loop();
  }

  // Clears dead loop state, after all merges have already be done.
  void TurnLoopIntoRegularBlock() {
    DCHECK(is_loop());
    predecessor_count_--;
    predecessors_so_far_--;
    ReducePhiPredecessorCount(1);
    clear_is_loop();
  }

  void RemovePredecessorAt(int predecessor_id);

  // Returns and clears the known node aspects on this state. Expects to only
  // ever be called once, when starting a basic block with this state.
  KnownNodeAspects* TakeKnownNodeAspects() {
    DCHECK_NOT_NULL(known_node_aspects_);
    return std::exchange(known_node_aspects_, nullptr);
  }

  KnownNodeAspects* CloneKnownNodeAspects(Zone* zone) {
    return known_node_aspects_->Clone(zone);
  }

  void ClearKnownNodeAspects() { known_node_aspects_ = nullptr; }

  void MergeNodeAspects(Zone* zone,
                        const KnownNodeAspects& known_node_aspects) {
    if (!known_node_aspects_) {
      known_node_aspects_ = known_node_aspects.Clone(zone);
    } else {
      known_node_aspects_->Merge(known_node_aspects, zone);
    }
  }

  const CompactInterpreterFrameState& frame_state() const {
    return frame_state_;
  }
  MergePointRegisterState& register_state() { return register_state_; }

  bool has_phi() const { return !phis_.is_empty(); }
  Phi::List* phis() { return &phis_; }

  uint32_t predecessor_count() const { return predecessor_count_; }

  uint32_t predecessors_so_far() const { return predecessors_so_far_; }

  BasicBlock* predecessor_at(int i) const {
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
    DCHECK_LT(i, predecessors_so_far_);
    return predecessors_[i];
  }
  void set_predecessor_at(int i, BasicBlock* val) {
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
    DCHECK_LT(i, predecessors_so_far_);
    predecessors_[i] = val;
  }

  void PrintVirtualObjects(const MaglevCompilationUnit& unit,
                           VirtualObjectList from_ifs,
                           const char* prelude = nullptr) {
    if (V8_LIKELY(!v8_flags.trace_maglev_graph_building ||
                  !unit.is_tracing_enabled())) {
      return;
    }
    if (prelude) {
      std::cout << prelude << std::endl;
    }
    from_ifs.Print(std::cout, "* VOs (Interpreter Frame State): ");
    if (known_node_aspects_) {
      known_node_aspects_->virtual_objects().Print(
          std::cout, "* VOs (Merge Frame State): ");
    }
  }

  bool is_loop() const {
    return basic_block_type() == BasicBlockType::kLoopHeader;
  }

  bool exception_handler_was_used() const {
    DCHECK(is_exception_handler());
    return basic_block_type() == BasicBlockType::kExceptionHandlerStart;
  }

  bool is_exception_handler() const {
    return basic_block_type() == BasicBlockType::kExceptionHandlerStart ||
           basic_block_type() == BasicBlockType::kUnusedExceptionHandlerStart;
  }

  bool is_inline() const { return kIsInline::decode(bitfield_); }

  bool is_unmerged_loop() const {
    // If this is a loop and not all predecessors are set, then the loop isn't
    // merged yet.
    DCHECK_IMPLIES(is_loop(), predecessor_count_ > 0);
    return is_loop() && predecessors_so_far_ < predecessor_count_;
  }

  bool is_unmerged_unreachable_loop() const {
    // If there is only one predecessor, and it's not set, then this is a loop
    // merge with no forward control flow entering it.
    return is_unmerged_loop() && !is_resumable_loop() &&
           predecessor_count_ == 1 && predecessors_so_far_ == 0;
  }

  bool IsUnreachableByForwardEdge() const;
  bool IsUnreachable() const;

  BasicBlockType basic_block_type() const {
    return kBasicBlockTypeBits::decode(bitfield_);
  }
  bool is_resumable_loop() const {
    bool res = kIsResumableLoopBit::decode(bitfield_);
    DCHECK_IMPLIES(res, is_loop());
    return res;
  }
  void set_is_resumable_loop(Graph* graph);
  bool is_loop_with_peeled_iteration() const {
    return kIsLoopWithPeeledIterationBit::decode(bitfield_);
  }

  int merge_offset() const { return merge_offset_; }

  DeoptFrame* backedge_deopt_frame() const { return backedge_deopt_frame_; }

  const compiler::LoopInfo* loop_info() const {
    DCHECK(loop_metadata_.has_value());
    DCHECK_NOT_NULL(loop_metadata_->loop_info);
    return loop_metadata_->loop_info;
  }
  void ClearLoopInfo() { loop_metadata_->loop_info = nullptr; }
  bool HasLoopInfo() const {
    return loop_metadata_.has_value() && loop_metadata_->loop_info;
  }

  interpreter::Register catch_block_context_register() const {
    DCHECK(is_exception_handler());
    return catch_block_context_register_;
  }

 private:
  using kBasicBlockTypeBits = base::BitField<BasicBlockType, 0, 2>;
  using kIsResumableLoopBit = kBasicBlockTypeBits::Next<bool, 1>;
  using kIsLoopWithPeeledIterationBit = kIsResumableLoopBit::Next<bool, 1>;
  using kIsInline = kIsLoopWithPeeledIterationBit::Next<bool, 1>;

  // For each non-Phi value in the frame state, store its alternative
  // representations to avoid re-converting on Phi creation.
  class Alternatives {
   public:
    using List = base::ThreadedList<Alternatives>;

    explicit Alternatives(const NodeInfo* node_info)
        : node_type_(node_info ? node_info->type() : NodeType::kUnknown),
          tagged_alternative_(node_info ? node_info->alternative().tagged()
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

  void MergePhis(MaglevGraphBuilder* builder,
                 MaglevCompilationUnit& compilation_unit,
                 InterpreterFrameState& unmerged, BasicBlock* predecessor,
                 bool optimistic_loop_phis);

  ValueNode* MergeValue(const MaglevGraphBuilder* graph_builder,
                        interpreter::Register owner,
                        const KnownNodeAspects& unmerged_aspects,
                        ValueNode* merged, ValueNode* unmerged,
                        Alternatives::List* per_predecessor_alternatives,
                        bool optimistic_loop_phis = false);

  void ReducePhiPredecessorCount(unsigned num);

  void MergeVirtualObjects(MaglevGraphBuilder* builder,
                           MaglevCompilationUnit& compilation_unit,
                           const KnownNodeAspects& unmerged_aspects);

  void MergeVirtualObject(MaglevGraphBuilder* builder,
                          const VirtualObjectList unmerged_vos,
                          const KnownNodeAspects& unmerged_aspects,
                          VirtualObject* merged, VirtualObject* unmerged);

  std::optional<ValueNode*> MergeVirtualObjectValue(
      const MaglevGraphBuilder* graph_builder,
      const KnownNodeAspects& unmerged_aspects, ValueNode* merged,
      ValueNode* unmerged);

  void MergeLoopValue(MaglevGraphBuilder* graph_builder,
                      interpreter::Register owner,
                      const KnownNodeAspects& unmerged_aspects,
                      ValueNode* merged, ValueNode* unmerged);

  ValueNode* NewLoopPhi(Zone* zone, interpreter::Register reg);

  ValueNode* NewExceptionPhi(Zone* zone, interpreter::Register reg) {
    DCHECK_EQ(predecessor_count_, 0);
    DCHECK_NULL(predecessors_);
    Phi* result = Node::New<Phi>(zone, 0, this, reg);
    phis_.Add(result);
    return result;
  }

  int merge_offset_;

  uint32_t predecessor_count_;
  uint32_t predecessors_so_far_;

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
    // For catch blocks, store the interpreter register holding the context.
    // This will be the same value for all incoming merges.
    interpreter::Register catch_block_context_register_;
  };

  struct LoopMetadata {
    const compiler::LoopInfo* loop_info;
    const LoopEffects* loop_effects;
  };
  std::optional<LoopMetadata> loop_metadata_ = std::nullopt;
};

struct LoopEffects {
  explicit LoopEffects(int loop_header, Zone* zone)
      :
#ifdef DEBUG
        loop_header(loop_header),
#endif
        context_slot_written(zone),
        objects_written(zone),
        keys_cleared(zone),
        allocations(zone) {
  }
#ifdef DEBUG
  int loop_header;
#endif
  ZoneSet<KnownNodeAspects::LoadedContextSlotsKey> context_slot_written;
  ZoneSet<ValueNode*> objects_written;
  ZoneSet<PropertyKey> keys_cleared;
  ZoneSet<InlinedAllocation*> allocations;
  bool unstable_aspects_cleared = false;
  bool may_have_aliasing_contexts = false;
  void Merge(const LoopEffects* other) {
    if (!unstable_aspects_cleared) {
      unstable_aspects_cleared = other->unstable_aspects_cleared;
    }
    if (!may_have_aliasing_contexts) {
      may_have_aliasing_contexts = other->may_have_aliasing_contexts;
    }
    context_slot_written.insert(other->context_slot_written.begin(),
                                other->context_slot_written.end());
    objects_written.insert(other->objects_written.begin(),
                           other->objects_written.end());
    keys_cleared.insert(other->keys_cleared.begin(), other->keys_cleared.end());
    allocations.insert(other->allocations.begin(), other->allocations.end());
  }
};

void InterpreterFrameState::CopyFrom(const MaglevCompilationUnit& unit,
                                     MergePointInterpreterFrameState& state,
                                     bool preserve_known_node_aspects = false,
                                     Zone* zone = nullptr) {
  DCHECK_IMPLIES(preserve_known_node_aspects, zone);
  if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building &&
                  unit.is_tracing_enabled())) {
    std::cout << "- Copying frame state from merge @" << &state << std::endl;
    if (known_node_aspects_) {
      state.PrintVirtualObjects(unit, virtual_objects());
    }
  }
  if (known_node_aspects_) {
    known_node_aspects_->virtual_objects().Snapshot();
  }
  state.frame_state().ForEachValue(
      unit, [&](ValueNode* value, interpreter::Register reg) {
        frame_[reg] = value;
      });
  if (preserve_known_node_aspects) {
    known_node_aspects_ = state.CloneKnownNodeAspects(zone);
  } else {
    // Move "what we know" across without copying -- we can safely mutate it
    // now, as we won't be entering this merge point again.
    known_node_aspects_ = state.TakeKnownNodeAspects();
  }
}

inline VirtualObjectList DeoptFrame::GetVirtualObjects() const {
  if (type() == DeoptFrame::FrameType::kInterpretedFrame) {
    // Recover virtual object list using the last object before the
    // deopt frame creation.
    return VirtualObjectList(as_interpreted().last_virtual_object());
  }
  DCHECK_NOT_NULL(parent());
  return parent()->GetVirtualObjects();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
