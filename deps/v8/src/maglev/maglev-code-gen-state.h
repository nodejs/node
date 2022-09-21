// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_CODE_GEN_STATE_H_
#define V8_MAGLEV_MAGLEV_CODE_GEN_STATE_H_

#include "src/codegen/assembler.h"
#include "src/codegen/label.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

class InterpreterFrameState;
class MaglevAssembler;

class DeferredCodeInfo {
 public:
  virtual void Generate(MaglevAssembler* masm, Label* return_label) = 0;
  Label deferred_code_label;
  Label return_label;
};

class MaglevCodeGenState {
 public:
  MaglevCodeGenState(MaglevCompilationInfo* compilation_info,
                     MaglevSafepointTableBuilder* safepoint_table_builder)
      : compilation_info_(compilation_info),
        safepoint_table_builder_(safepoint_table_builder) {}

  void set_tagged_slots(int slots) { tagged_slots_ = slots; }
  void set_untagged_slots(int slots) { untagged_slots_ = slots; }

  void PushDeferredCode(DeferredCodeInfo* deferred_code) {
    deferred_code_.push_back(deferred_code);
  }
  const std::vector<DeferredCodeInfo*>& deferred_code() const {
    return deferred_code_;
  }
  void PushEagerDeopt(EagerDeoptInfo* info) { eager_deopts_.push_back(info); }
  void PushLazyDeopt(LazyDeoptInfo* info) { lazy_deopts_.push_back(info); }
  const std::vector<EagerDeoptInfo*>& eager_deopts() const {
    return eager_deopts_;
  }
  const std::vector<LazyDeoptInfo*>& lazy_deopts() const {
    return lazy_deopts_;
  }

  void PushHandlerInfo(NodeBase* node) { handlers_.push_back(node); }
  const std::vector<NodeBase*>& handlers() const { return handlers_; }

  compiler::NativeContextRef native_context() const {
    return broker()->target_native_context();
  }
  Isolate* isolate() const { return compilation_info_->isolate(); }
  compiler::JSHeapBroker* broker() const { return compilation_info_->broker(); }
  MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }
  int stack_slots() const { return untagged_slots_ + tagged_slots_; }
  int tagged_slots() const { return tagged_slots_; }
  MaglevSafepointTableBuilder* safepoint_table_builder() const {
    return safepoint_table_builder_;
  }
  MaglevCompilationInfo* compilation_info() const { return compilation_info_; }

 private:
  MaglevCompilationInfo* const compilation_info_;
  MaglevSafepointTableBuilder* const safepoint_table_builder_;

  std::vector<DeferredCodeInfo*> deferred_code_;
  std::vector<EagerDeoptInfo*> eager_deopts_;
  std::vector<LazyDeoptInfo*> lazy_deopts_;
  std::vector<NodeBase*> handlers_;

  int untagged_slots_ = 0;
  int tagged_slots_ = 0;
};

// Some helpers for codegen.
// TODO(leszeks): consider moving this to a separate header.

inline int GetSafepointIndexForStackSlot(int i) {
  // Safepoint tables also contain slots for all fixed frame slots (both
  // above and below the fp).
  return StandardFrameConstants::kFixedSlotCount + i;
}

inline Register ToRegister(const compiler::InstructionOperand& operand) {
  return compiler::AllocatedOperand::cast(operand).GetRegister();
}

inline DoubleRegister ToDoubleRegister(
    const compiler::InstructionOperand& operand) {
  return compiler::AllocatedOperand::cast(operand).GetDoubleRegister();
}

template <typename RegisterT>
inline auto ToRegisterT(const compiler::InstructionOperand& operand) {
  if constexpr (std::is_same_v<RegisterT, Register>) {
    return ToRegister(operand);
  } else {
    return ToDoubleRegister(operand);
  }
}

inline Register ToRegister(const ValueLocation& location) {
  return ToRegister(location.operand());
}

inline DoubleRegister ToDoubleRegister(const ValueLocation& location) {
  return ToDoubleRegister(location.operand());
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_CODE_GEN_STATE_H_
