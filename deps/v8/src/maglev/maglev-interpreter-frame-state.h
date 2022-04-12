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
      : accumulator_(state.accumulator_), frame_(info) {
    frame_.CopyFrom(info, state.frame_, nullptr);
  }

  void CopyFrom(const MaglevCompilationUnit& info,
                const InterpreterFrameState& state) {
    accumulator_ = state.accumulator_;
    frame_.CopyFrom(info, state.frame_, nullptr);
  }

  inline void CopyFrom(const MaglevCompilationUnit& info,
                       const MergePointInterpreterFrameState& state);

  void set_accumulator(ValueNode* value) { accumulator_ = value; }
  ValueNode* accumulator() const { return accumulator_; }

  void set(interpreter::Register reg, ValueNode* value) {
    DCHECK_IMPLIES(reg.is_parameter(),
                   reg == interpreter::Register::current_context() ||
                       reg == interpreter::Register::function_closure() ||
                       reg.ToParameterIndex() >= 0);
    frame_[reg] = value;
  }
  ValueNode* get(interpreter::Register reg) const {
    DCHECK_IMPLIES(reg.is_parameter(),
                   reg == interpreter::Register::current_context() ||
                       reg == interpreter::Register::function_closure() ||
                       reg.ToParameterIndex() >= 0);
    return frame_[reg];
  }

  const RegisterFrameArray<ValueNode*>& frame() const { return frame_; }

 private:
  ValueNode* accumulator_ = nullptr;
  RegisterFrameArray<ValueNode*> frame_;
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
        live_registers_and_accumulator_(
            info.zone()->NewArray<ValueNode*>(SizeFor(info, liveness))),
        liveness_(liveness),
        predecessors_(info.zone()->NewArray<BasicBlock*>(predecessor_count)) {
    int live_index = 0;
    ForEachRegister(info, [&](interpreter::Register reg) {
      live_registers_and_accumulator_[live_index++] = state.get(reg);
    });
    if (liveness_->AccumulatorIsLive()) {
      live_registers_and_accumulator_[live_index++] = state.accumulator();
    }
    predecessors_[0] = predecessor;
  }

  MergePointInterpreterFrameState(
      const MaglevCompilationUnit& info, int merge_offset,
      int predecessor_count, const compiler::BytecodeLivenessState* liveness,
      const compiler::LoopInfo* loop_info)
      : predecessor_count_(predecessor_count),
        predecessors_so_far_(1),
        live_registers_and_accumulator_(
            info.zone()->NewArray<ValueNode*>(SizeFor(info, liveness))),
        liveness_(liveness),
        predecessors_(info.zone()->NewArray<BasicBlock*>(predecessor_count)) {
    int live_index = 0;
    auto& assignments = loop_info->assignments();
    ForEachParameter(info, [&](interpreter::Register reg) {
      ValueNode* value = nullptr;
      if (assignments.ContainsParameter(reg.ToParameterIndex())) {
        value = NewLoopPhi(info.zone(), reg, merge_offset, value);
      }
      live_registers_and_accumulator_[live_index++] = value;
    });
    ForEachLocal([&](interpreter::Register reg) {
      ValueNode* value = nullptr;
      if (assignments.ContainsLocal(reg.index())) {
        value = NewLoopPhi(info.zone(), reg, merge_offset, value);
      }
      live_registers_and_accumulator_[live_index++] = value;
    });
    DCHECK(!liveness_->AccumulatorIsLive());

#ifdef DEBUG
    predecessors_[0] = nullptr;
#endif
  }

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void Merge(const MaglevCompilationUnit& compilation_unit,
             const InterpreterFrameState& unmerged, BasicBlock* predecessor,
             int merge_offset) {
    DCHECK_GT(predecessor_count_, 1);
    DCHECK_LT(predecessors_so_far_, predecessor_count_);
    predecessors_[predecessors_so_far_] = predecessor;

    ForEachValue(
        compilation_unit, [&](interpreter::Register reg, ValueNode*& value) {
          CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);

          value = MergeValue(compilation_unit.zone(), reg, value,
                             unmerged.get(reg), merge_offset);
        });
    predecessors_so_far_++;
    DCHECK_LE(predecessors_so_far_, predecessor_count_);
  }

  MergePointRegisterState& register_state() { return register_state_; }

  // Merges an unmerged framestate with a possibly merged framestate into |this|
  // framestate.
  void MergeLoop(const MaglevCompilationUnit& compilation_unit,
                 const InterpreterFrameState& loop_end_state,
                 BasicBlock* loop_end_block, int merge_offset) {
    DCHECK_EQ(predecessors_so_far_, predecessor_count_);
    DCHECK_NULL(predecessors_[0]);
    predecessors_[0] = loop_end_block;

    ForEachValue(
        compilation_unit, [&](interpreter::Register reg, ValueNode* value) {
          CheckIsLoopPhiIfNeeded(compilation_unit, merge_offset, reg, value);

          MergeLoopValue(compilation_unit.zone(), reg, value,
                         loop_end_state.get(reg), merge_offset);
        });
    DCHECK(!liveness_->AccumulatorIsLive());
  }

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

  ValueNode* MergeValue(Zone* zone, interpreter::Register owner,
                        ValueNode* merged, ValueNode* unmerged,
                        int merge_offset) {
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
      result->set_input(predecessors_so_far_, unmerged);
      return result;
    }

    if (merged == unmerged) return merged;

    // Up to this point all predecessors had the same value for this interpreter
    // frame slot. Now that we find a distinct value, insert a copy of the first
    // value for each predecessor seen so far, in addition to the new value.
    // TODO(verwaest): Unclear whether we want this for Maglev: Instead of
    // letting the register allocator remove phis, we could always merge through
    // the frame slot. In that case we only need the inputs for representation
    // selection, and hence could remove duplicate inputs. We'd likely need to
    // attach the interpreter register to the phi in that case?
    result = Node::New<Phi>(zone, predecessor_count_, owner, merge_offset);

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

  ValueNode* NewLoopPhi(Zone* zone, interpreter::Register reg, int merge_offset,
                        ValueNode* initial_value) {
    DCHECK_EQ(predecessors_so_far_, 1);
    // Create a new loop phi, which for now is empty.
    Phi* result = Node::New<Phi>(zone, predecessor_count_, reg, merge_offset);
#ifdef DEBUG
    result->set_input(0, nullptr);
#endif
    phis_.Add(result);
    return result;
  }
  static int SizeFor(const MaglevCompilationUnit& info,
                     const compiler::BytecodeLivenessState* liveness) {
    return info.parameter_count() + liveness->live_value_count();
  }

  template <typename Function>
  void ForEachParameter(const MaglevCompilationUnit& info, Function&& f) const {
    for (int i = 0; i < info.parameter_count(); i++) {
      interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
      f(reg);
    }
  }

  template <typename Function>
  void ForEachParameter(const MaglevCompilationUnit& info, Function&& f) {
    for (int i = 0; i < info.parameter_count(); i++) {
      interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
      f(reg);
    }
  }

  template <typename Function>
  void ForEachLocal(Function&& f) const {
    for (int register_index : *liveness_) {
      interpreter::Register reg = interpreter::Register(register_index);
      f(reg);
    }
  }

  template <typename Function>
  void ForEachLocal(Function&& f) {
    for (int register_index : *liveness_) {
      interpreter::Register reg = interpreter::Register(register_index);
      f(reg);
    }
  }

  template <typename Function>
  void ForEachRegister(const MaglevCompilationUnit& info, Function&& f) {
    ForEachParameter(info, f);
    ForEachLocal(f);
  }

  template <typename Function>
  void ForEachRegister(const MaglevCompilationUnit& info, Function&& f) const {
    ForEachParameter(info, f);
    ForEachLocal(f);
  }

  template <typename Function>
  void ForEachValue(const MaglevCompilationUnit& info, Function&& f) {
    int live_index = 0;
    ForEachRegister(info, [&](interpreter::Register reg) {
      f(reg, live_registers_and_accumulator_[live_index++]);
    });
    if (liveness_->AccumulatorIsLive()) {
      f(interpreter::Register::virtual_accumulator(),
        live_registers_and_accumulator_[live_index++]);
      live_index++;
    }
    DCHECK_EQ(live_index, SizeFor(info, liveness_));
  }

  int predecessor_count_;
  int predecessors_so_far_;
  Phi::List phis_;
  ValueNode** live_registers_and_accumulator_;
  const compiler::BytecodeLivenessState* liveness_ = nullptr;
  BasicBlock** predecessors_;

  MergePointRegisterState register_state_;
};

void InterpreterFrameState::CopyFrom(
    const MaglevCompilationUnit& info,
    const MergePointInterpreterFrameState& state) {
  int live_index = 0;
  state.ForEachRegister(info, [&](interpreter::Register reg) {
    frame_[reg] = state.live_registers_and_accumulator_[live_index++];
  });
  if (state.liveness_->AccumulatorIsLive()) {
    accumulator_ = state.live_registers_and_accumulator_[live_index++];
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_INTERPRETER_FRAME_STATE_H_
