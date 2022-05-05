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
class MergePointInterpreterFrameState;

class InterpreterFrameState {
 public:
  explicit InterpreterFrameState(const MaglevCompilationUnit& info)
      : frame_(info) {}

  InterpreterFrameState(const MaglevCompilationUnit& info,
                        const InterpreterFrameState& state)
      : frame_(info) {
    frame_.CopyFrom(info, state.frame_, nullptr);
  }

  void CopyFrom(const MaglevCompilationUnit& info,
                const InterpreterFrameState& state) {
    frame_.CopyFrom(info, state.frame_, nullptr);
  }

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

 private:
  RegisterFrameArray<ValueNode*> frame_;
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
      f(live_registers_and_accumulator_[info.parameter_count() + live_reg++],
        reg);
    }
  }

  template <typename Function>
  void ForEachLocal(const MaglevCompilationUnit& info, Function&& f) {
    int live_reg = 0;
    for (int register_index : *liveness_) {
      interpreter::Register reg = interpreter::Register(register_index);
      f(live_registers_and_accumulator_[info.parameter_count() + live_reg++],
        reg);
    }
  }

  template <typename Function>
  void ForEachRegister(const MaglevCompilationUnit& info, Function&& f) {
    ForEachParameter(info, f);
    ForEachLocal(info, f);
  }

  template <typename Function>
  void ForEachRegister(const MaglevCompilationUnit& info, Function&& f) const {
    ForEachParameter(info, f);
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
    return live_registers_and_accumulator_[size(info) - 1];
  }
  ValueNode* accumulator(const MaglevCompilationUnit& info) const {
    return live_registers_and_accumulator_[size(info) - 1];
  }

  size_t size(const MaglevCompilationUnit& info) const {
    return SizeFor(info, liveness_);
  }

 private:
  static size_t SizeFor(const MaglevCompilationUnit& info,
                        const compiler::BytecodeLivenessState* liveness) {
    return info.parameter_count() + liveness->live_value_count();
  }

  ValueNode** const live_registers_and_accumulator_;
  const compiler::BytecodeLivenessState* const liveness_;
};

class MergePointRegisterState {
 public:
  class Iterator {
   public:
    struct Entry {
      RegisterState& state;
      Register reg;
    };
    explicit Iterator(RegisterState* value_pointer,
                      RegList::Iterator reg_iterator)
        : current_value_(value_pointer), reg_iterator_(reg_iterator) {}
    Entry operator*() { return {*current_value_, *reg_iterator_}; }
    void operator++() {
      ++current_value_;
      ++reg_iterator_;
    }
    bool operator!=(const Iterator& other) const {
      return current_value_ != other.current_value_;
    }

   private:
    RegisterState* current_value_;
    RegList::Iterator reg_iterator_;
  };

  bool is_initialized() const { return values_[0].GetPayload().is_initialized; }

  Iterator begin() {
    return Iterator(values_, kAllocatableGeneralRegisters.begin());
  }
  Iterator end() {
    return Iterator(values_ + kAllocatableGeneralRegisterCount,
                    kAllocatableGeneralRegisters.end());
  }

 private:
  RegisterState values_[kAllocatableGeneralRegisterCount] = {{}};
};

class MergePointInterpreterFrameState {
 public:
  static constexpr BasicBlock* kDeadPredecessor = nullptr;

  void CheckIsLoopPhiIfNeeded(const MaglevCompilationUnit& compilation_unit,
                              int merge_offset, interpreter::Register reg,
                              ValueNode* value) {
#ifdef DEBUG
    const auto& analysis = compilation_unit.bytecode_analysis();
    if (!analysis.IsLoopHeader(merge_offset)) return;
    auto& assignments = analysis.GetLoopInfoFor(merge_offset).assignments();
    if (reg.is_parameter()) {
      if (!assignments.ContainsParameter(reg.ToParameterIndex())) return;
    } else {
      DCHECK(
          analysis.GetInLivenessFor(merge_offset)->RegisterIsLive(reg.index()));
      if (!assignments.ContainsLocal(reg.index())) return;
    }
    DCHECK(value->Is<Phi>());
#endif
  }

  MergePointInterpreterFrameState(
      const MaglevCompilationUnit& info, const InterpreterFrameState& state,
      int merge_offset, int predecessor_count, BasicBlock* predecessor,
      const compiler::BytecodeLivenessState* liveness)
      : predecessor_count_(predecessor_count),
        predecessors_so_far_(1),
        predecessors_(info.zone()->NewArray<BasicBlock*>(predecessor_count)),
        frame_state_(info, liveness, state) {
    predecessors_[0] = predecessor;
  }

  MergePointInterpreterFrameState(
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, const compiler::BytecodeLivenessState* liveness,
      const compiler::LoopInfo* loop_info)
      : predecessor_count_(predecessor_count),
        predecessors_so_far_(1),
        predecessors_(info.zone()->NewArray<BasicBlock*>(predecessor_count)),
        frame_state_(info, liveness) {
    auto& assignments = loop_info->assignments();
    frame_state_.ForEachParameter(
        info, [&](ValueNode*& entry, interpreter::Register reg) {
          entry = nullptr;
          if (assignments.ContainsParameter(reg.ToParameterIndex())) {
            entry = NewLoopPhi(info.zone(), reg, merge_offset);
          }
        });
    frame_state_.ForEachLocal(
        info, [&](ValueNode*& entry, interpreter::Register reg) {
          entry = nullptr;
          if (assignments.ContainsLocal(reg.index())) {
            entry = NewLoopPhi(info.zone(), reg, merge_offset);
          }
        });
    DCHECK(!frame_state_.liveness()->AccumulatorIsLive());

#ifdef DEBUG
    predecessors_[0] = nullptr;
#endif
  }

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void Merge(MaglevCompilationUnit& compilation_unit,
             const InterpreterFrameState& unmerged, BasicBlock* predecessor,
             int merge_offset) {
    DCHECK_GT(predecessor_count_, 1);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    predecessors_[predecessors_so_far_] = predecessor;

    frame_state_.ForEachValue(
        compilation_unit, [&](ValueNode*& value, interpreter::Register reg) {
          CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);

          value = MergeValue(compilation_unit, reg, value, unmerged.get(reg),
                             merge_offset);
        });
    predecessors_so_far_++;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
  }

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeLoop(const MaglevCompilationUnit& compilation_unit,
                 const InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block, int merge_offset) {
    DCHECK_EQ(predecessors_so_far_, predecessor_count_);
    DCHECK_NULL(predecessors_[0]);
    predecessors_[0] = loop_end_block;

    frame_state_.ForEachValue(
        compilation_unit, [&](ValueNode* value, interpreter::Register reg) {
          CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);

          MergeLoopValue(compilation_unit.zone(), reg, value,
                         loop_end_state.get(reg), merge_offset);
        });
  }

  // Merges a dead framestate (e.g. one which has been early terminated with a
  // deopt).
  void MergeDead() {
    DCHECK_GT(predecessor_count_, 1);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    predecessors_[predecessors_so_far_] = kDeadPredecessor;
    predecessors_so_far_++;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
  }

  // Merges a dead loop framestate (e.g. one where the block containing the
  // JumpLoop has been early terminated with a deopt).
  void MergeDeadLoop() {
    DCHECK_EQ(predecessors_so_far_, predecessor_count_);
    DCHECK_NULL(predecessors_[0]);
    predecessors_[0] = kDeadPredecessor;
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
    DCHECK_EQ(predecessors_so_far_, predecessor_count_);
    DCHECK_LT(i, predecessor_count_);
    return predecessors_[i];
  }

 private:
  friend void InterpreterFrameState::CopyFrom(
      const MaglevCompilationUnit& info,
      const MergePointInterpreterFrameState& state);

  ValueNode* TagValue(MaglevCompilationUnit& compilation_unit,
                      ValueNode* value) {
    DCHECK(value->is_untagged_value());
    if (value->Is<CheckedSmiUntag>()) {
      return value->input(0).node();
    }
    DCHECK(value->Is<Int32AddWithOverflow>() || value->Is<Int32Constant>());
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
    value->AddNodeAfter(tagged);
    compilation_unit.RegisterNodeInGraphLabeller(tagged);
    return tagged;
  }

  ValueNode* EnsureTagged(MaglevCompilationUnit& compilation_unit,
                          ValueNode* value) {
    if (value->is_untagged_value()) return TagValue(compilation_unit, value);
    return value;
  }

  ValueNode* MergeValue(MaglevCompilationUnit& compilation_unit,
                        interpreter::Register owner, ValueNode* merged,
                        ValueNode* unmerged, int merge_offset) {
    // If the merged node is null, this is a pre-created loop header merge
    // frame will null values for anything that isn't a loop Phi.
    if (merged == nullptr) {
      DCHECK_NULL(predecessors_[0]);
      DCHECK_EQ(predecessors_so_far_, 1);
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

    phis_.Add(result);
    return result;
  }

  void MergeLoopValue(Zone* zone, interpreter::Register owner,
                      ValueNode* merged, ValueNode* unmerged,
                      int merge_offset) {
    Phi* result = merged->TryCast<Phi>();
    if (result == nullptr || result->merge_offset() != merge_offset) {
      DCHECK_EQ(merged, unmerged);
      return;
    }
    DCHECK_EQ(result->owner(), owner);
    // The loop jump is defined to unconditionally be index 0.
#ifdef DEBUG
    DCHECK_NULL(result->input(0).node());
#endif
    result->set_input(0, unmerged);
  }

  ValueNode* NewLoopPhi(Zone* zone, interpreter::Register reg,
                        int merge_offset) {
    DCHECK_EQ(predecessors_so_far_, 1);
    // Create a new loop phi, which for now is empty.
    Phi* result = Node::New<Phi>(zone, predecessor_count_, reg, merge_offset);
#ifdef DEBUG
    result->set_input(0, nullptr);
#endif
    phis_.Add(result);
    return result;
  }

  int predecessor_count_;
  int predecessors_so_far_;
  Phi::List phis_;
  BasicBlock** predecessors_;

  CompactInterpreterFrameState frame_state_;
  MergePointRegisterState register_state_;
};

void InterpreterFrameState::CopyFrom(
    const MaglevCompilationUnit& info,
    const MergePointInterpreterFrameState& state) {
  state.frame_state().ForEachValue(
      info, [&](ValueNode* value, interpreter::Register reg) {
        frame_[reg] = value;
      });
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
