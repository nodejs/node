// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_

#include <cmath>
#include <iomanip>
#include <map>
#include <type_traits>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/codegen/source-position-table.h"
#include "src/common/globals.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/processed-feedback.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/flags/flags.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {
namespace maglev {

class CallArguments;

template <typename NodeT>
inline void MarkAsLazyDeoptResult(NodeT* value,
                                  interpreter::Register result_location,
                                  int result_size) {
  DCHECK_EQ(NodeT::kProperties.can_lazy_deopt(),
            value->properties().can_lazy_deopt());
  if constexpr (NodeT::kProperties.can_lazy_deopt()) {
    value->lazy_deopt_info()->SetResultLocation(result_location, result_size);
  }
}

template <>
inline void MarkAsLazyDeoptResult(ValueNode* value,
                                  interpreter::Register result_location,
                                  int result_size) {
  if (value->properties().can_lazy_deopt() &&
      !value->lazy_deopt_info()->result_location().is_valid()) {
    value->lazy_deopt_info()->SetResultLocation(result_location, result_size);
  }
}

class MaglevGraphBuilder {
 public:
  explicit MaglevGraphBuilder(LocalIsolate* local_isolate,
                              MaglevCompilationUnit* compilation_unit,
                              Graph* graph,
                              MaglevGraphBuilder* parent = nullptr);

  void Build() {
    DCHECK(!is_inline());

    StartPrologue();
    for (int i = 0; i < parameter_count(); i++) {
      // TODO(v8:7700): Consider creating InitialValue nodes lazily.
      InitialValue* v = AddNewNode<InitialValue>(
          {}, interpreter::Register::FromParameterIndex(i));
      DCHECK_EQ(graph()->parameters().size(), static_cast<size_t>(i));
      graph()->parameters().push_back(v);
      SetArgument(i, v);
    }
    BuildRegisterFrameInitialization();
    BuildMergeStates();
    EndPrologue();
    BuildBody();
  }

  void StartPrologue();
  void SetArgument(int i, ValueNode* value);
  void InitializeRegister(interpreter::Register reg,
                          ValueNode* value = nullptr);
  ValueNode* GetTaggedArgument(int i);
  void BuildRegisterFrameInitialization(ValueNode* context = nullptr,
                                        ValueNode* closure = nullptr);
  void BuildMergeStates();
  BasicBlock* EndPrologue();

  void BuildBody() {
    for (iterator_.Reset(); !iterator_.done(); iterator_.Advance()) {
      local_isolate_->heap()->Safepoint();
      VisitSingleBytecode();
    }
  }

  Graph* graph() const { return graph_; }

 private:
  class CallSpeculationScope;

  bool CheckType(ValueNode* node, NodeType type);
  bool EnsureType(ValueNode* node, NodeType type, NodeType* old = nullptr);
  bool is_toptier() {
    return v8_flags.lower_tier_as_toptier && !v8_flags.turbofan;
  }
  BasicBlock* CreateEdgeSplitBlock(int offset,
                                   int interrupt_budget_correction) {
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "== New empty block ==" << std::endl;
    }
    DCHECK_NULL(current_block_);
    current_block_ = zone()->New<BasicBlock>(nullptr);
    // Add an interrupt budget correction if necessary. This makes the edge
    // split block no longer empty, which is unexpected, but we're not changing
    // interpreter frame state, so that's ok.
    if (!is_toptier() && interrupt_budget_correction != 0) {
      DCHECK_GT(interrupt_budget_correction, 0);
      AddNewNode<IncreaseInterruptBudget>({}, interrupt_budget_correction);
    }
    BasicBlock* result = FinishBlock<Jump>({}, &jump_targets_[offset]);
    result->set_edge_split_block();
#ifdef DEBUG
    new_nodes_.clear();
#endif
    return result;
  }

  void ProcessMergePointAtExceptionHandlerStart(int offset) {
    MergePointInterpreterFrameState& merge_state = *merge_states_[offset];
    DCHECK_EQ(merge_state.predecessor_count(), 0);

    // Copy state.
    current_interpreter_frame_.CopyFrom(*compilation_unit_, merge_state);

    // Merges aren't simple fallthroughs, so we should reset the checkpoint
    // validity.
    latest_checkpointed_frame_.reset();

    // Register exception phis.
    if (has_graph_labeller()) {
      for (Phi* phi : *merge_states_[offset]->phis()) {
        graph_labeller()->RegisterNode(phi);
        if (v8_flags.trace_maglev_graph_building) {
          std::cout << "  " << phi << "  "
                    << PrintNodeLabel(graph_labeller(), phi) << ": "
                    << PrintNode(graph_labeller(), phi) << std::endl;
        }
      }
    }
  }

  void ProcessMergePoint(int offset) {
    // First copy the merge state to be the current state.
    MergePointInterpreterFrameState& merge_state = *merge_states_[offset];
    current_interpreter_frame_.CopyFrom(*compilation_unit_, merge_state);

    // Merges aren't simple fallthroughs, so we should reset the checkpoint
    // validity.
    latest_checkpointed_frame_.reset();

    if (merge_state.predecessor_count() == 1) return;

    // Set up edge-split.
    int predecessor_index = merge_state.predecessor_count() - 1;
    if (merge_state.is_loop()) {
      // For loops, the JumpLoop block hasn't been generated yet, and so isn't
      // in the list of jump targets. IT's the last predecessor, so drop the
      // index by one.
      DCHECK(merge_state.is_unmerged_loop());
      predecessor_index--;
    }
    BasicBlockRef* old_jump_targets = jump_targets_[offset].Reset();
    while (old_jump_targets != nullptr) {
      BasicBlock* predecessor = merge_state.predecessor_at(predecessor_index);
      ControlNode* control = predecessor->control_node();
      if (control->Is<ConditionalControlNode>()) {
        // CreateEmptyBlock automatically registers itself with the offset.
        predecessor = CreateEdgeSplitBlock(
            offset, old_jump_targets->interrupt_budget_correction());
        // Set the old predecessor's (the conditional block) reference to
        // point to the new empty predecessor block.
        old_jump_targets =
            old_jump_targets->SetToBlockAndReturnNext(predecessor);
      } else {
        // Re-register the block in the offset's ref list.
        old_jump_targets =
            old_jump_targets->MoveToRefList(&jump_targets_[offset]);
      }
      predecessor->set_predecessor_id(predecessor_index--);
    }
    DCHECK_EQ(predecessor_index, -1);
    if (has_graph_labeller()) {
      for (Phi* phi : *merge_states_[offset]->phis()) {
        graph_labeller()->RegisterNode(phi);
        if (v8_flags.trace_maglev_graph_building) {
          std::cout << "  " << phi << "  "
                    << PrintNodeLabel(graph_labeller(), phi) << ": "
                    << PrintNode(graph_labeller(), phi) << std::endl;
        }
      }
    }
  }

  // Return true if the given offset is a merge point, i.e. there are jumps
  // targetting it.
  bool IsOffsetAMergePoint(int offset) {
    return merge_states_[offset] != nullptr;
  }

  // Called when a block is killed by an unconditional eager deopt.
  void EmitUnconditionalDeopt(DeoptimizeReason reason) {
    // Create a block rather than calling finish, since we don't yet know the
    // next block's offset before the loop skipping the rest of the bytecodes.
    FinishBlock<Deopt>({}, reason);
    MarkBytecodeDead();
  }

  void MarkBytecodeDead() {
    DCHECK_NULL(current_block_);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "== Dead ==\n"
                << std::setw(4) << iterator_.current_offset() << " : ";
      interpreter::BytecodeDecoder::Decode(std::cout,
                                           iterator_.current_address());
      std::cout << std::endl;
    }

    // If the current bytecode is a jump to elsewhere, then this jump is
    // also dead and we should make sure to merge it as a dead predecessor.
    interpreter::Bytecode bytecode = iterator_.current_bytecode();
    if (interpreter::Bytecodes::IsForwardJump(bytecode)) {
      // Jumps merge into their target, and conditional jumps also merge into
      // the fallthrough.
      MergeDeadIntoFrameState(iterator_.GetJumpTargetOffset());
      if (interpreter::Bytecodes::IsConditionalJump(bytecode)) {
        MergeDeadIntoFrameState(iterator_.next_offset());
      }
    } else if (bytecode == interpreter::Bytecode::kJumpLoop) {
      // JumpLoop merges into its loop header, which has to be treated
      // specially by the merge.
      MergeDeadLoopIntoFrameState(iterator_.GetJumpTargetOffset());
    } else if (interpreter::Bytecodes::IsSwitch(bytecode)) {
      // Switches merge into their targets, and into the fallthrough.
      for (auto offset : iterator_.GetJumpTableTargetOffsets()) {
        MergeDeadIntoFrameState(offset.target_offset);
      }
      MergeDeadIntoFrameState(iterator_.next_offset());
    } else if (!interpreter::Bytecodes::Returns(bytecode) &&
               !interpreter::Bytecodes::UnconditionallyThrows(bytecode)) {
      // Any other bytecode that doesn't return or throw will merge into the
      // fallthrough.
      MergeDeadIntoFrameState(iterator_.next_offset());
    } else if (interpreter::Bytecodes::Returns(bytecode) && is_inline()) {
      MergeDeadIntoFrameState(inline_exit_offset());
    }

    // TODO(leszeks): We could now continue iterating the bytecode
  }

  void UpdateSourceAndBytecodePosition(int offset) {
    if (source_position_iterator_.done()) return;
    if (source_position_iterator_.code_offset() == offset) {
      // TODO(leszeks): Add inlining support.
      const int kInliningId = SourcePosition::kNotInlined;
      current_source_position_ = SourcePosition(
          source_position_iterator_.source_position().ScriptOffset(),
          kInliningId);
      source_position_iterator_.Advance();
    } else {
      DCHECK_GT(source_position_iterator_.code_offset(), offset);
    }
  }

  void VisitSingleBytecode() {
    int offset = iterator_.current_offset();
    UpdateSourceAndBytecodePosition(offset);

    MergePointInterpreterFrameState* merge_state = merge_states_[offset];
    if (V8_UNLIKELY(merge_state != nullptr)) {
      if (current_block_ != nullptr) {
        // TODO(leszeks): Re-evaluate this DCHECK, we might hit it if the only
        // bytecodes in this basic block were only register juggling.
        // DCHECK(!current_block_->nodes().is_empty());
        BasicBlock* predecessor = FinishBlock<Jump>({}, &jump_targets_[offset]);
        merge_state->Merge(*compilation_unit_, graph_->smi(),
                           current_interpreter_frame_, predecessor, offset);
      }
      if (v8_flags.trace_maglev_graph_building) {
        auto detail = merge_state->is_exception_handler() ? "exception handler"
                      : merge_state->is_loop()            ? "loop header"
                                                          : "merge";
        std::cout << "== New block (" << detail << ") ==" << std::endl;
      }

      if (merge_state->is_exception_handler()) {
        DCHECK_EQ(predecessors_[offset], 0);
        // If we have no reference to this block, then the exception handler is
        // dead.
        if (!jump_targets_[offset].has_ref()) {
          MarkBytecodeDead();
          return;
        }
        ProcessMergePointAtExceptionHandlerStart(offset);
      } else {
        ProcessMergePoint(offset);
      }

      StartNewBlock(offset);
    } else if (V8_UNLIKELY(current_block_ == nullptr)) {
      // If we don't have a current block, the bytecode must be dead (because of
      // some earlier deopt). Mark this bytecode dead too and return.
      // TODO(leszeks): Merge these two conditions by marking dead states with
      // a sentinel value.
#ifdef DEBUG
      if (predecessors_[offset] == 1) {
        DCHECK(bytecode_analysis().IsLoopHeader(offset));
        DCHECK_NULL(merge_state);
      } else {
        DCHECK_EQ(predecessors_[offset], 0);
      }
#endif
      MarkBytecodeDead();
      return;
    }

    // Handle exceptions if we have a table.
    if (bytecode().handler_table_size() > 0) {
      if (catch_block_stack_.size() > 0) {
        // Pop all entries where offset >= end.
        while (catch_block_stack_.size() > 0) {
          HandlerTableEntry& entry = catch_block_stack_.top();
          if (offset < entry.end) break;
          catch_block_stack_.pop();
        }
      }
      // Push new entries from interpreter handler table where offset >= start
      // && offset < end.
      HandlerTable table(*bytecode().object());
      while (next_handler_table_index_ < table.NumberOfRangeEntries()) {
        int start = table.GetRangeStart(next_handler_table_index_);
        if (offset < start) break;
        int end = table.GetRangeEnd(next_handler_table_index_);
        if (offset >= end) {
          next_handler_table_index_++;
          continue;
        }
        int handler = table.GetRangeHandler(next_handler_table_index_);
        catch_block_stack_.push({end, handler});
        next_handler_table_index_++;
      }
    }

    DCHECK_NOT_NULL(current_block_);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << std::setw(4) << iterator_.current_offset() << " : ";
      interpreter::BytecodeDecoder::Decode(std::cout,
                                           iterator_.current_address());
      std::cout << std::endl;
    }
#ifdef DEBUG
    // Clear new nodes for the next VisitFoo
    new_nodes_.clear();
#endif
    switch (iterator_.current_bytecode()) {
#define BYTECODE_CASE(name, ...)       \
  case interpreter::Bytecode::k##name: \
    Visit##name();                     \
    break;
      BYTECODE_LIST(BYTECODE_CASE)
#undef BYTECODE_CASE
    }
  }

#define BYTECODE_VISITOR(name, ...) void Visit##name();
  BYTECODE_LIST(BYTECODE_VISITOR)
#undef BYTECODE_VISITOR

#define DECLARE_VISITOR(name, ...) \
  void VisitIntrinsic##name(interpreter::RegisterList args);
  INTRINSICS_LIST(DECLARE_VISITOR)
#undef DECLARE_VISITOR

  template <typename NodeT>
  NodeT* AddNode(NodeT* node) {
    if (node->properties().is_required_when_unused()) {
      MarkPossibleSideEffect();
    }
    current_block_->nodes().Add(node);
    if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  " << node << "  "
                << PrintNodeLabel(graph_labeller(), node) << ": "
                << PrintNode(graph_labeller(), node) << std::endl;
    }
#ifdef DEBUG
    new_nodes_.insert(node);
#endif
    return node;
  }

  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(size_t input_count, Args&&... args) {
    return AddNode(
        CreateNewNode<NodeT>(input_count, std::forward<Args>(args)...));
  }

  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(std::initializer_list<ValueNode*> inputs, Args&&... args) {
    return AddNode(CreateNewNode<NodeT>(inputs, std::forward<Args>(args)...));
  }

  template <typename NodeT, typename... Args>
  NodeT* CreateNewNodeHelper(Args&&... args) {
    if constexpr (NodeT::kProperties.can_eager_deopt()) {
      return NodeBase::New<NodeT>(zone(), GetLatestCheckpointedFrame(),
                                  current_speculation_feedback_,
                                  std::forward<Args>(args)...);
    } else if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      return NodeBase::New<NodeT>(zone(), GetDeoptFrameForLazyDeopt(),
                                  current_speculation_feedback_,
                                  std::forward<Args>(args)...);
    } else {
      return NodeBase::New<NodeT>(zone(), std::forward<Args>(args)...);
    }
  }

  template <typename NodeT, typename... Args>
  NodeT* CreateNewNode(Args&&... args) {
    NodeT* node = CreateNewNodeHelper<NodeT>(std::forward<Args>(args)...);
    if constexpr (NodeT::kProperties.can_throw()) {
      if (catch_block_stack_.size() > 0) {
        // Inside a try-block.
        int handler_offset = catch_block_stack_.top().handler;
        new (node->exception_handler_info())
            ExceptionHandlerInfo(&jump_targets_[handler_offset]);
      } else {
        // Patch no exception handler marker.
        // TODO(victorgomes): Avoid allocating exception handler data in this
        // case.
        new (node->exception_handler_info()) ExceptionHandlerInfo();
      }
    }
    return node;
  }

  enum ContextSlotMutability { kImmutable, kMutable };
  bool TrySpecializeLoadContextSlotToFunctionContext(
      ValueNode** context, size_t* depth, int slot_index,
      ContextSlotMutability slot_mutability);
  ValueNode* LoadAndCacheContextSlot(ValueNode* context, int offset,
                                     ContextSlotMutability slot_mutability);
  void BuildLoadContextSlot(ValueNode* context, size_t depth, int slot_index,
                            ContextSlotMutability slot_mutability);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(std::initializer_list<ValueNode*> inputs) {
    using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
    CallBuiltin* call_builtin;
    if constexpr (Descriptor::HasContextParameter()) {
      call_builtin =
          CreateNewNode<CallBuiltin>(inputs.size() + 1, kBuiltin, GetContext());
    } else {
      call_builtin = CreateNewNode<CallBuiltin>(inputs.size(), kBuiltin);
    }
    int arg_index = 0;
    for (auto* input : inputs) {
      call_builtin->set_arg(arg_index++, input);
    }
    return AddNode(call_builtin);
  }

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(
      std::initializer_list<ValueNode*> inputs,
      compiler::FeedbackSource const& feedback,
      CallBuiltin::FeedbackSlotType slot_type = CallBuiltin::kTaggedIndex) {
    CallBuiltin* call_builtin = BuildCallBuiltin<kBuiltin>(inputs);
    call_builtin->set_feedback(feedback, slot_type);
#ifdef DEBUG
    // Check that the last parameters are kSlot and kVector.
    using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
    int slot_index = call_builtin->InputCountWithoutContext();
    int vector_index = slot_index + 1;
    DCHECK_EQ(slot_index, Descriptor::kSlot);
    // TODO(victorgomes): Rename all kFeedbackVector parameters in the builtins
    // to kVector.
    DCHECK_EQ(vector_index, Descriptor::kVector);
#endif  // DEBUG
    return call_builtin;
  }

  void BuildLoadGlobal(compiler::NameRef name,
                       compiler::FeedbackSource& feedback_source,
                       TypeofMode typeof_mode);

  CallRuntime* BuildCallRuntime(Runtime::FunctionId function_id,
                                std::initializer_list<ValueNode*> inputs) {
    CallRuntime* call_runtime = CreateNewNode<CallRuntime>(
        inputs.size() + CallRuntime::kFixedInputCount, function_id,
        GetContext());
    int arg_index = 0;
    for (auto* input : inputs) {
      call_runtime->set_arg(arg_index++, input);
    }
    return AddNode(call_runtime);
  }

  void BuildAbort(AbortReason reason) {
    // Create a block rather than calling finish, since we don't yet know the
    // next block's offset before the loop skipping the rest of the bytecodes.
    FinishBlock<Abort>({}, reason);
    MarkBytecodeDead();
  }

  void Print(const char* str) {
    Handle<String> string_handle =
        local_isolate()->factory()->NewStringFromAsciiChecked(
            str, AllocationType::kOld);
    ValueNode* string_node = GetConstant(MakeRefAssumeMemoryFence(
        broker(), broker()->CanonicalPersistentHandle(string_handle)));
    BuildCallRuntime(Runtime::kGlobalPrint, {string_node});
  }

  void Print(ValueNode* value) {
    BuildCallRuntime(Runtime::kDebugPrint, {value});
  }

  void Print(const char* str, ValueNode* value) {
    Print(str);
    Print(value);
  }

  ValueNode* GetClosure() const {
    return current_interpreter_frame_.get(
        interpreter::Register::function_closure());
  }

  ValueNode* GetContext() const {
    return current_interpreter_frame_.get(
        interpreter::Register::current_context());
  }

  void SetContext(ValueNode* context) {
    current_interpreter_frame_.set(interpreter::Register::current_context(),
                                   context);
  }

  FeedbackSlot GetSlotOperand(int operand_index) const {
    return iterator_.GetSlotOperand(operand_index);
  }

  uint32_t GetFlag8Operand(int operand_index) const {
    return iterator_.GetFlag8Operand(operand_index);
  }

  uint32_t GetFlag16Operand(int operand_index) const {
    return iterator_.GetFlag16Operand(operand_index);
  }

  template <class T, typename = std::enable_if_t<
                         std::is_convertible<T*, Object*>::value>>
  typename compiler::ref_traits<T>::ref_type GetRefOperand(int operand_index) {
    // The BytecodeArray itself was fetched by using a barrier so all reads
    // from the constant pool are safe.
    return MakeRefAssumeMemoryFence(
        broker(), broker()->CanonicalPersistentHandle(
                      Handle<T>::cast(iterator_.GetConstantForIndexOperand(
                          operand_index, local_isolate()))));
  }

  SmiConstant* GetSmiConstant(int constant) {
    DCHECK(Smi::IsValid(constant));
    auto it = graph_->smi().find(constant);
    if (it == graph_->smi().end()) {
      SmiConstant* node = CreateNewNode<SmiConstant>(0, Smi::FromInt(constant));
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->smi().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  ExternalConstant* GetExternalConstant(ExternalReference reference) {
    auto it = graph_->external_references().find(reference.address());
    if (it == graph_->external_references().end()) {
      ExternalConstant* node = CreateNewNode<ExternalConstant>(0, reference);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->external_references().emplace(reference.address(), node);
      return node;
    }
    return it->second;
  }

  RootConstant* GetRootConstant(RootIndex index) {
    auto it = graph_->root().find(index);
    if (it == graph_->root().end()) {
      RootConstant* node = CreateNewNode<RootConstant>(0, index);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->root().emplace(index, node);
      return node;
    }
    return it->second;
  }

  RootConstant* GetBooleanConstant(bool value) {
    return GetRootConstant(value ? RootIndex::kTrueValue
                                 : RootIndex::kFalseValue);
  }

  Int32Constant* GetInt32Constant(int constant) {
    // The constant must fit in a Smi, since it could be later tagged in a Phi.
    DCHECK(Smi::IsValid(constant));
    auto it = graph_->int32().find(constant);
    if (it == graph_->int32().end()) {
      Int32Constant* node = CreateNewNode<Int32Constant>(0, constant);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->int32().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  Float64Constant* GetFloat64Constant(double constant) {
    if (std::isnan(constant)) {
      if (graph_->nan() == nullptr) {
        graph_->set_nan(CreateNewNode<Float64Constant>(0, constant));
      }
      return graph_->nan();
    }
    auto it = graph_->float64().find(constant);
    if (it == graph_->float64().end()) {
      Float64Constant* node = CreateNewNode<Float64Constant>(0, constant);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->float64().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  ValueNode* GetConstant(const compiler::ObjectRef& ref) {
    if (ref.IsSmi()) return GetSmiConstant(ref.AsSmi());
    const compiler::HeapObjectRef& constant = ref.AsHeapObject();

    auto root_index = broker()->FindRootIndex(constant);
    if (root_index.has_value()) {
      return GetRootConstant(*root_index);
    }

    auto it = graph_->constants().find(constant);
    if (it == graph_->constants().end()) {
      Constant* node = CreateNewNode<Constant>(0, constant);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->constants().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  ValueNode* GetRegisterInput(Register reg) {
    DCHECK(!graph_->register_inputs().has(reg));
    graph_->register_inputs().set(reg);
    return AddNewNode<RegisterInput>({}, reg);
  }

#define DEFINE_IS_ROOT_OBJECT(type, name, CamelName)               \
  bool Is##CamelName(ValueNode* value) const {                     \
    if (RootConstant* constant = value->TryCast<RootConstant>()) { \
      return constant->index() == RootIndex::k##CamelName;         \
    }                                                              \
    return false;                                                  \
  }
  ROOT_LIST(DEFINE_IS_ROOT_OBJECT)
#undef DEFINE_IS_ROOT_OBJECT

  // Move an existing ValueNode between two registers. You can pass
  // virtual_accumulator as the src or dst to move in or out of the accumulator.
  void MoveNodeBetweenRegisters(interpreter::Register src,
                                interpreter::Register dst) {
    // We shouldn't be moving newly created nodes between registers.
    DCHECK_EQ(0, new_nodes_.count(current_interpreter_frame_.get(src)));
    DCHECK_NOT_NULL(current_interpreter_frame_.get(src));

    current_interpreter_frame_.set(dst, current_interpreter_frame_.get(src));
  }

  ValueNode* GetTaggedValue(ValueNode* value) {
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kWord64:
        UNREACHABLE();
      case ValueRepresentation::kTagged:
        return value;
      case ValueRepresentation::kInt32: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->tagged_alternative == nullptr) {
          if (NodeTypeIsSmi(node_info->type)) {
            node_info->tagged_alternative = AddNewNode<UnsafeSmiTag>({value});
          } else {
            node_info->tagged_alternative = AddNewNode<Int32ToNumber>({value});
          }
        }
        return node_info->tagged_alternative;
      }
      case ValueRepresentation::kUint32: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->tagged_alternative == nullptr) {
          if (NodeTypeIsSmi(node_info->type)) {
            node_info->tagged_alternative = AddNewNode<UnsafeSmiTag>({value});
          } else {
            node_info->tagged_alternative = AddNewNode<Uint32ToNumber>({value});
          }
        }
        return node_info->tagged_alternative;
      }
      case ValueRepresentation::kFloat64: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->tagged_alternative == nullptr) {
          node_info->tagged_alternative = AddNewNode<Float64Box>({value});
        }
        return node_info->tagged_alternative;
      }
    }
    UNREACHABLE();
  }

  ValueNode* GetTaggedValue(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return GetTaggedValue(value);
  }

  void SetKnownType(ValueNode* node, NodeType type) {
    NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(node);
    known_info->type = type;
  }

  ValueNode* GetInternalizedString(interpreter::Register reg) {
    ValueNode* node = GetTaggedValue(reg);
    if (known_node_aspects()
            .GetOrCreateInfoFor(node)
            ->is_internalized_string()) {
      return node;
    }
    if (Constant* constant = node->TryCast<Constant>()) {
      if (constant->object().IsInternalizedString()) {
        SetKnownType(constant, NodeType::kInternalizedString);
        return constant;
      }
    }
    node = AddNewNode<CheckedInternalizedString>({node});
    SetKnownType(node, NodeType::kInternalizedString);
    current_interpreter_frame_.set(reg, node);
    return node;
  }

  ValueNode* GetTruncatedInt32FromNumber(ValueNode* value) {
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kWord64:
        UNREACHABLE();
      case ValueRepresentation::kTagged: {
        if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
          return GetInt32Constant(constant->value().value());
        }
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->int32_alternative != nullptr) {
          return node_info->int32_alternative;
        }
        if (node_info->truncated_int32_alternative != nullptr) {
          return node_info->truncated_int32_alternative;
        }
        NodeType old_type;
        EnsureType(value, NodeType::kNumber, &old_type);
        if (NodeTypeIsSmi(old_type)) {
          node_info->int32_alternative = AddNewNode<UnsafeSmiUntag>({value});
          return node_info->int32_alternative;
        }
        if (NodeTypeIsNumber(old_type)) {
          node_info->truncated_int32_alternative =
              AddNewNode<TruncateNumberToInt32>({value});
        } else {
          node_info->truncated_int32_alternative =
              AddNewNode<CheckedTruncateNumberToInt32>({value});
        }
        return node_info->truncated_int32_alternative;
      }
      case ValueRepresentation::kFloat64: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->truncated_int32_alternative == nullptr) {
          node_info->truncated_int32_alternative =
              AddNewNode<TruncateFloat64ToInt32>({value});
        }
        return node_info->truncated_int32_alternative;
      }
      case ValueRepresentation::kInt32:
        // Already good.
        return value;
      case ValueRepresentation::kUint32:
        return AddNewNode<TruncateUint32ToInt32>({value});
    }
    UNREACHABLE();
  }

  ValueNode* GetTruncatedInt32(ValueNode* value) {
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kWord64:
        UNREACHABLE();
      case ValueRepresentation::kTagged:
      case ValueRepresentation::kFloat64:
        return GetInt32(value);
      case ValueRepresentation::kInt32:
        // Already good.
        return value;
      case ValueRepresentation::kUint32:
        return AddNewNode<TruncateUint32ToInt32>({value});
    }
    UNREACHABLE();
  }

  ValueNode* GetTruncatedInt32(interpreter::Register reg) {
    return GetTruncatedInt32(current_interpreter_frame_.get(reg));
  }

  ValueNode* GetInt32(ValueNode* value) {
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kWord64:
        UNREACHABLE();
      case ValueRepresentation::kTagged: {
        if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
          return GetInt32Constant(constant->value().value());
        }
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->int32_alternative == nullptr) {
          node_info->int32_alternative = BuildSmiUntag(value);
        }
        return node_info->int32_alternative;
      }
      case ValueRepresentation::kInt32:
        return value;
      case ValueRepresentation::kUint32: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->int32_alternative == nullptr) {
          if (node_info->is_smi()) {
            node_info->int32_alternative =
                AddNewNode<TruncateUint32ToInt32>({value});
          } else {
            node_info->int32_alternative =
                AddNewNode<CheckedUint32ToInt32>({value});
          }
        }
        return node_info->int32_alternative;
      }
      case ValueRepresentation::kFloat64: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->int32_alternative == nullptr) {
          node_info->int32_alternative =
              AddNewNode<CheckedTruncateFloat64ToInt32>({value});
        }
        return node_info->int32_alternative;
      }
    }
    UNREACHABLE();
  }

  ValueNode* GetInt32(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return GetInt32(value);
  }

  ValueNode* GetFloat64(ValueNode* value) {
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kWord64:
        UNREACHABLE();
      case ValueRepresentation::kTagged: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->float64_alternative == nullptr) {
          node_info->float64_alternative =
              AddNewNode<CheckedFloat64Unbox>({value});
        }
        return node_info->float64_alternative;
      }
      case ValueRepresentation::kInt32: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->float64_alternative == nullptr) {
          node_info->float64_alternative =
              AddNewNode<ChangeInt32ToFloat64>({value});
        }
        return node_info->float64_alternative;
      }
      case ValueRepresentation::kUint32: {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
        if (node_info->float64_alternative == nullptr) {
          node_info->float64_alternative =
              AddNewNode<ChangeUint32ToFloat64>({value});
        }
        return node_info->float64_alternative;
      }
      case ValueRepresentation::kFloat64:
        return value;
    }
    UNREACHABLE();
  }

  ValueNode* GetFloat64(interpreter::Register reg) {
    return GetFloat64(current_interpreter_frame_.get(reg));
  }

  ValueNode* GetAccumulatorTagged() {
    return GetTaggedValue(interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorInt32() {
    return GetInt32(interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorTruncatedInt32() {
    return GetTruncatedInt32(interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorFloat64() {
    return GetFloat64(interpreter::Register::virtual_accumulator());
  }

  bool IsRegisterEqualToAccumulator(int operand_index) {
    interpreter::Register source = iterator_.GetRegisterOperand(operand_index);
    return current_interpreter_frame_.get(source) ==
           current_interpreter_frame_.accumulator();
  }

  ValueNode* LoadRegisterRaw(int operand_index) {
    return current_interpreter_frame_.get(
        iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterTagged(int operand_index) {
    return GetTaggedValue(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterInt32(int operand_index) {
    return GetInt32(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterTruncatedInt32(int operand_index) {
    return GetTruncatedInt32(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterFloat64(int operand_index) {
    return GetFloat64(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* BuildLoadFixedArrayElement(ValueNode* node, int index) {
    return AddNewNode<LoadTaggedField>({node},
                                       FixedArray::OffsetOfElementAt(index));
  }

  template <typename NodeT>
  void SetAccumulator(NodeT* node) {
    // Accumulator stores are equivalent to stores to the virtual accumulator
    // register.
    StoreRegister(interpreter::Register::virtual_accumulator(), node);
  }

  ValueNode* GetSecondValue(ValueNode* result) {
    // GetSecondReturnedValue must be added just after a node that calls a
    // builtin that expects 2 returned values. It simply binds kReturnRegister1
    // to a value node. Since the previous node must have been a builtin
    // call, the register is available in the register allocator. No gap moves
    // would be emitted between these two nodes.
    if (result->opcode() == Opcode::kCallRuntime) {
      DCHECK_EQ(result->Cast<CallRuntime>()->ReturnCount(), 2);
    } else if (result->opcode() == Opcode::kCallBuiltin) {
      DCHECK_EQ(result->Cast<CallBuiltin>()->ReturnCount(), 2);
    } else {
      DCHECK_EQ(result->opcode(), Opcode::kForInPrepare);
    }
    // {result} must be the last node in the current block.
    DCHECK(current_block_->nodes().Contains(result));
    DCHECK_EQ(result->NextNode(), nullptr);
    return AddNewNode<GetSecondReturnedValue>({});
  }

  template <typename NodeT>
  void StoreRegister(interpreter::Register target, NodeT* value) {
    // We should only set register values to nodes that were newly created in
    // this Visit. Existing nodes should be moved between registers with
    // MoveNodeBetweenRegisters.
    if (!IsConstantNode(value->opcode())) {
      DCHECK_NE(0, new_nodes_.count(value));
    }
    MarkAsLazyDeoptResult(value, target, 1);
    current_interpreter_frame_.set(target, value);
  }

  template <typename NodeT>
  void StoreRegisterPair(
      std::pair<interpreter::Register, interpreter::Register> target,
      NodeT* value) {
    const interpreter::Register target0 = target.first;
    const interpreter::Register target1 = target.second;

    DCHECK_EQ(interpreter::Register(target0.index() + 1), target1);
    DCHECK_EQ(value->ReturnCount(), 2);

    DCHECK_NE(0, new_nodes_.count(value));
    MarkAsLazyDeoptResult(value, target0, value->ReturnCount());
    current_interpreter_frame_.set(target0, value);

    ValueNode* second_value = GetSecondValue(value);
    DCHECK_NE(0, new_nodes_.count(second_value));
    current_interpreter_frame_.set(target1, second_value);
  }

  InterpretedDeoptFrame GetLatestCheckpointedFrame() {
    if (!latest_checkpointed_frame_) {
      latest_checkpointed_frame_.emplace(
          *compilation_unit_,
          zone()->New<CompactInterpreterFrameState>(
              *compilation_unit_, GetInLiveness(), current_interpreter_frame_),
          BytecodeOffset(iterator_.current_offset()), current_source_position_,
          // TODO(leszeks): Don't always allocate for the parent state,
          // maybe cache it on the graph builder?
          parent_
              ? zone()->New<DeoptFrame>(parent_->GetLatestCheckpointedFrame())
              : nullptr);
    }
    return *latest_checkpointed_frame_;
  }

  InterpretedDeoptFrame GetDeoptFrameForLazyDeopt() {
    return InterpretedDeoptFrame(
        *compilation_unit_,
        zone()->New<CompactInterpreterFrameState>(
            *compilation_unit_, GetOutLiveness(), current_interpreter_frame_),
        BytecodeOffset(iterator_.current_offset()), current_source_position_,
        // TODO(leszeks): Support inlining for lazy deopts.
        nullptr);
  }

  void MarkPossibleSideEffect() {
    // If there was a potential side effect, invalidate the previous checkpoint.
    latest_checkpointed_frame_.reset();

    // A side effect could change existing objects' maps. For stable maps we
    // know this hasn't happened (because we added a dependency on the maps
    // staying stable and therefore not possible to transition away from), but
    // we can no longer assume that objects with unstable maps still have the
    // same map. Unstable maps can also transition to stable ones, so the
    // set of stable maps becomes invalid for a not that had a unstable map.
    auto it = known_node_aspects().unstable_maps.begin();
    while (it != known_node_aspects().unstable_maps.end()) {
      if (it->second.size() == 0) {
        it++;
      } else {
        known_node_aspects().stable_maps.erase(it->first);
        it = known_node_aspects().unstable_maps.erase(it);
      }
    }
    // Similarly, side-effects can change object contents, so we have to clear
    // our known loaded properties -- however, constant properties are known
    // to not change (and we added a dependency on this), so we don't have to
    // clear those.
    known_node_aspects().loaded_properties.clear();
    known_node_aspects().loaded_context_slots.clear();
  }

  int next_offset() const {
    return iterator_.current_offset() + iterator_.current_bytecode_size();
  }
  const compiler::BytecodeLivenessState* GetInLiveness() const {
    return GetInLivenessFor(iterator_.current_offset());
  }
  const compiler::BytecodeLivenessState* GetInLivenessFor(int offset) const {
    return bytecode_analysis().GetInLivenessFor(offset);
  }
  const compiler::BytecodeLivenessState* GetOutLiveness() const {
    return GetOutLivenessFor(iterator_.current_offset());
  }
  const compiler::BytecodeLivenessState* GetOutLivenessFor(int offset) const {
    return bytecode_analysis().GetOutLivenessFor(offset);
  }

  void StartNewBlock(int offset) {
    DCHECK_NULL(current_block_);
    current_block_ = zone()->New<BasicBlock>(merge_states_[offset]);
    ResolveJumpsToBlockAtOffset(current_block_, offset);
  }

  template <typename ControlNodeT, typename... Args>
  BasicBlock* FinishBlock(std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args) {
    ControlNode* control_node = CreateNewNode<ControlNodeT>(
        control_inputs, std::forward<Args>(args)...);
    current_block_->set_control_node(control_node);

    BasicBlock* block = current_block_;
    current_block_ = nullptr;

    graph()->Add(block);
    if (has_graph_labeller()) {
      graph_labeller()->RegisterBasicBlock(block);
      if (v8_flags.trace_maglev_graph_building) {
        bool kSkipTargets = true;
        std::cout << "  " << control_node << "  "
                  << PrintNodeLabel(graph_labeller(), control_node) << ": "
                  << PrintNode(graph_labeller(), control_node, kSkipTargets)
                  << std::endl;
      }
    }
    return block;
  }

  // Update all jumps which were targetting the not-yet-created block at the
  // given `block_offset`, to now point to the given `block`.
  void ResolveJumpsToBlockAtOffset(BasicBlock* block, int block_offset) {
    int interrupt_budget_correction = 0;
    BasicBlockRef* jump_target_refs_head =
        jump_targets_[block_offset].SetToBlockAndReturnNext(block);
    while (jump_target_refs_head != nullptr) {
      // Only one jump target should ever set the interrupt budget correction.
      DCHECK_EQ(interrupt_budget_correction, 0);
      interrupt_budget_correction =
          jump_target_refs_head->interrupt_budget_correction();
      jump_target_refs_head =
          jump_target_refs_head->SetToBlockAndReturnNext(block);
    }
    if (!is_toptier() && interrupt_budget_correction != 0) {
      DCHECK_GT(interrupt_budget_correction, 0);
      AddNewNode<IncreaseInterruptBudget>({}, interrupt_budget_correction);
    }
    DCHECK_EQ(jump_targets_[block_offset].block_ptr(), block);
  }

  void StartFallthroughBlock(int next_block_offset, BasicBlock* predecessor) {
    // Start a new block for the fallthrough path, unless it's a merge point, in
    // which case we merge our state into it. That merge-point could also be a
    // loop header, in which case the merge state might not exist yet (if the
    // only predecessors are this path and the JumpLoop).
    DCHECK_NULL(current_block_);

    if (NumPredecessors(next_block_offset) == 1) {
      if (v8_flags.trace_maglev_graph_building) {
        std::cout << "== New block (single fallthrough) ==" << std::endl;
      }
      StartNewBlock(next_block_offset);
    } else {
      MergeIntoFrameState(predecessor, next_block_offset);
    }
  }

  ValueNode* GetTaggedOrUndefined(ValueNode* maybe_value) {
    if (maybe_value == nullptr) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    }
    return GetTaggedValue(maybe_value);
  }

  ValueNode* GetConvertReceiver(compiler::JSFunctionRef function,
                                CallArguments& args);

  template <typename LoadNode>
  ValueNode* TryBuildLoadDataView(const CallArguments& args,
                                  ExternalArrayType type);
  template <typename StoreNode, typename Function>
  ValueNode* TryBuildStoreDataView(const CallArguments& args,
                                   ExternalArrayType type, Function&& getValue);

#define MATH_UNARY_IEEE_BUILTIN(V) \
  V(MathAcos)                      \
  V(MathAcosh)                     \
  V(MathAsin)                      \
  V(MathAsinh)                     \
  V(MathAtan)                      \
  V(MathAtanh)                     \
  V(MathCbrt)                      \
  V(MathCos)                       \
  V(MathCosh)                      \
  V(MathExp)                       \
  V(MathExpm1)                     \
  V(MathLog)                       \
  V(MathLog1p)                     \
  V(MathLog10)                     \
  V(MathLog2)                      \
  V(MathSin)                       \
  V(MathSinh)                      \
  V(MathTan)                       \
  V(MathTanh)

#define MAGLEV_REDUCED_BUILTIN(V) \
  V(DataViewPrototypeGetInt8)     \
  V(DataViewPrototypeSetInt8)     \
  V(DataViewPrototypeGetInt16)    \
  V(DataViewPrototypeSetInt16)    \
  V(DataViewPrototypeGetInt32)    \
  V(DataViewPrototypeSetInt32)    \
  V(DataViewPrototypeGetFloat64)  \
  V(DataViewPrototypeSetFloat64)  \
  V(FunctionPrototypeCall)        \
  V(MathPow)                      \
  V(StringFromCharCode)           \
  V(StringPrototypeCharCodeAt)    \
  MATH_UNARY_IEEE_BUILTIN(V)

#define DEFINE_BUILTIN_REDUCER(Name)                                 \
  ValueNode* TryReduce##Name(compiler::JSFunctionRef builtin_target, \
                             CallArguments& args);
  MAGLEV_REDUCED_BUILTIN(DEFINE_BUILTIN_REDUCER)
#undef DEFINE_BUILTIN_REDUCER

  template <typename CallNode, typename... Args>
  CallNode* AddNewCallNode(const CallArguments& args, Args&&... extra_args);

  ValueNode* TryReduceBuiltin(compiler::JSFunctionRef builtin_target,
                              CallArguments& args,
                              const compiler::FeedbackSource& feedback_source,
                              SpeculationMode speculation_mode);
  ValueNode* TryBuildCallKnownJSFunction(compiler::JSFunctionRef function,
                                         CallArguments& args);
  ValueNode* TryBuildInlinedCall(compiler::JSFunctionRef function,
                                 CallArguments& args);
  ValueNode* BuildGenericCall(ValueNode* target, ValueNode* context,
                              Call::TargetType target_type,
                              const CallArguments& args,
                              const compiler::FeedbackSource& feedback_source =
                                  compiler::FeedbackSource());
  ValueNode* ReduceCall(
      compiler::ObjectRef target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source =
          compiler::FeedbackSource(),
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation);
  ValueNode* ReduceCallForTarget(
      ValueNode* target_node, compiler::JSFunctionRef target,
      CallArguments& args, const compiler::FeedbackSource& feedback_source,
      SpeculationMode speculation_mode);
  ValueNode* ReduceFunctionPrototypeApplyCallWithReceiver(
      ValueNode* target_node, compiler::JSFunctionRef receiver,
      CallArguments& args, const compiler::FeedbackSource& feedback_source,
      SpeculationMode speculation_mode);
  void BuildCall(ValueNode* target_node, CallArguments& args,
                 compiler::FeedbackSource& feedback_source);
  void BuildCallFromRegisterList(ConvertReceiverMode receiver_mode);
  void BuildCallFromRegisters(int argc_count,
                              ConvertReceiverMode receiver_mode);

  bool TryBuildScriptContextConstantAccess(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  bool TryBuildScriptContextAccess(
      const compiler::GlobalAccessFeedback& global_access_feedback);
  bool TryBuildPropertyCellAccess(
      const compiler::GlobalAccessFeedback& global_access_feedback);

  ValueNode* BuildSmiUntag(ValueNode* node);

  void BuildCheckSmi(ValueNode* object);
  void BuildCheckNumber(ValueNode* object);
  void BuildCheckHeapObject(ValueNode* object);
  void BuildCheckString(ValueNode* object);
  void BuildCheckSymbol(ValueNode* object);
  void BuildCheckMaps(ValueNode* object,
                      base::Vector<const compiler::MapRef> maps);
  // Emits an unconditional deopt and returns false if the node is a constant
  // that doesn't match the ref.
  bool BuildCheckValue(ValueNode* node, const compiler::ObjectRef& ref);

  ValueNode* GetInt32ElementIndex(interpreter::Register reg) {
    ValueNode* index_object = current_interpreter_frame_.get(reg);
    return GetInt32ElementIndex(index_object);
  }
  ValueNode* GetInt32ElementIndex(ValueNode* index_object);

  ValueNode* GetUint32ElementIndex(interpreter::Register reg) {
    ValueNode* index_object = current_interpreter_frame_.get(reg);
    return GetUint32ElementIndex(index_object);
  }
  ValueNode* GetUint32ElementIndex(ValueNode* index_object);

  bool CanTreatHoleAsUndefined(
      ZoneVector<compiler::MapRef> const& receiver_maps);

  bool TryFoldLoadDictPrototypeConstant(
      compiler::PropertyAccessInfo access_info);
  // Returns a ValueNode if the load could be folded, and nullptr otherwise.
  ValueNode* TryFoldLoadConstantDataField(
      compiler::PropertyAccessInfo access_info, ValueNode* lookup_start_object);

  // Returns the loaded value node but doesn't update the accumulator yet.
  ValueNode* BuildLoadField(compiler::PropertyAccessInfo access_info,
                            ValueNode* lookup_start_object);
  bool TryBuildStoreField(compiler::PropertyAccessInfo access_info,
                          ValueNode* receiver,
                          compiler::AccessMode access_mode);
  bool TryBuildPropertyGetterCall(compiler::PropertyAccessInfo access_info,
                                  ValueNode* receiver,
                                  ValueNode* lookup_start_object);
  bool TryBuildPropertySetterCall(compiler::PropertyAccessInfo access_info,
                                  ValueNode* receiver, ValueNode* value);

  bool TryBuildPropertyLoad(ValueNode* receiver, ValueNode* lookup_start_object,
                            compiler::NameRef name,
                            compiler::PropertyAccessInfo const& access_info);
  bool TryBuildPropertyStore(ValueNode* receiver, compiler::NameRef name,
                             compiler::PropertyAccessInfo const& access_info,
                             compiler::AccessMode access_mode);
  bool TryBuildPropertyAccess(ValueNode* receiver,
                              ValueNode* lookup_start_object,
                              compiler::NameRef name,
                              compiler::PropertyAccessInfo const& access_info,
                              compiler::AccessMode access_mode);

  bool TryBuildElementAccessOnString(
      ValueNode* object, ValueNode* index,
      compiler::KeyedAccessMode const& keyed_mode);

  bool TryBuildNamedAccess(ValueNode* receiver, ValueNode* lookup_start_object,
                           compiler::NamedAccessFeedback const& feedback,
                           compiler::FeedbackSource const& feedback_source,
                           compiler::AccessMode access_mode);
  bool TryBuildElementAccess(ValueNode* object, ValueNode* index,
                             compiler::ElementAccessFeedback const& feedback,
                             compiler::FeedbackSource const& feedback_source);

  // Load elimination -- when loading or storing a simple property without
  // side effects, record its value, and allow that value to be re-used on
  // subsequent loads.
  void RecordKnownProperty(ValueNode* lookup_start_object,
                           compiler::NameRef name, ValueNode* value,
                           compiler::PropertyAccessInfo const& access_info);
  bool TryReuseKnownPropertyLoad(ValueNode* lookup_start_object,
                                 compiler::NameRef name);

  enum InferHasInPrototypeChainResult {
    kMayBeInPrototypeChain,
    kIsInPrototypeChain,
    kIsNotInPrototypeChain
  };
  InferHasInPrototypeChainResult InferHasInPrototypeChain(
      ValueNode* receiver, compiler::HeapObjectRef prototype);
  bool TryBuildFastHasInPrototypeChain(ValueNode* object,
                                       compiler::ObjectRef prototype);
  void BuildHasInPrototypeChain(ValueNode* object,
                                compiler::ObjectRef prototype);
  bool TryBuildFastOrdinaryHasInstance(ValueNode* object,
                                       compiler::JSObjectRef callable,
                                       ValueNode* callable_node);
  void BuildOrdinaryHasInstance(ValueNode* object,
                                compiler::JSObjectRef callable,
                                ValueNode* callable_node);
  bool TryBuildFastInstanceOf(ValueNode* object,
                              compiler::JSObjectRef callable_ref,
                              ValueNode* callable_node);
  bool TryBuildFastInstanceOfWithFeedback(
      ValueNode* object, ValueNode* callable,
      compiler::FeedbackSource feedback_source);

  template <Operation kOperation>
  void BuildGenericUnaryOperationNode();
  template <Operation kOperation>
  void BuildGenericBinaryOperationNode();
  template <Operation kOperation>
  void BuildGenericBinarySmiOperationNode();

  template <Operation kOperation>
  ValueNode* TryFoldInt32BinaryOperation(ValueNode* left, ValueNode* right);
  template <Operation kOperation>
  ValueNode* TryFoldInt32BinaryOperation(ValueNode* left, int right);

  template <Operation kOperation>
  void BuildInt32UnaryOperationNode();
  void BuildTruncatingInt32BitwiseNotForNumber();
  template <Operation kOperation>
  void BuildInt32BinaryOperationNode();
  template <Operation kOperation>
  void BuildInt32BinarySmiOperationNode();
  template <Operation kOperation>
  void BuildTruncatingInt32BinaryOperationNodeForNumber();
  template <Operation kOperation>
  void BuildTruncatingInt32BinarySmiOperationNodeForNumber();
  template <Operation kOperation>
  void BuildFloat64UnaryOperationNode();
  template <Operation kOperation>
  void BuildFloat64BinaryOperationNode();
  template <Operation kOperation>
  void BuildFloat64BinarySmiOperationNode();

  template <Operation kOperation>
  void VisitUnaryOperation();
  template <Operation kOperation>
  void VisitBinaryOperation();
  template <Operation kOperation>
  void VisitBinarySmiOperation();

  template <typename BranchControlNodeT, typename... Args>
  bool TryBuildBranchFor(std::initializer_list<ValueNode*> control_inputs,
                         Args&&... args);

  template <Operation kOperation>
  void VisitCompareOperation();

  void MergeIntoFrameState(BasicBlock* block, int target);
  void MergeDeadIntoFrameState(int target);
  void MergeDeadLoopIntoFrameState(int target);
  void MergeIntoInlinedReturnFrameState(BasicBlock* block);

  enum JumpType { kJumpIfTrue, kJumpIfFalse };
  void BuildBranchIfRootConstant(ValueNode* node, JumpType jump_type,
                                 RootIndex root_index);
  void BuildBranchIfTrue(ValueNode* node, JumpType jump_type);
  void BuildBranchIfNull(ValueNode* node, JumpType jump_type);
  void BuildBranchIfUndefined(ValueNode* node, JumpType jump_type);
  void BuildBranchIfToBooleanTrue(ValueNode* node, JumpType jump_type);

  void BuildToNumberOrToNumeric(Object::Conversion mode);

  void CalculatePredecessorCounts() {
    // Add 1 after the end of the bytecode so we can always write to the offset
    // after the last bytecode.
    size_t array_length = bytecode().length() + 1;
    predecessors_ = zone()->NewArray<uint32_t>(array_length);
    MemsetUint32(predecessors_, 1, array_length);

    interpreter::BytecodeArrayIterator iterator(bytecode().object());
    for (; !iterator.done(); iterator.Advance()) {
      interpreter::Bytecode bytecode = iterator.current_bytecode();
      if (interpreter::Bytecodes::IsJump(bytecode)) {
        predecessors_[iterator.GetJumpTargetOffset()]++;
        if (!interpreter::Bytecodes::IsConditionalJump(bytecode)) {
          predecessors_[iterator.next_offset()]--;
        }
      } else if (interpreter::Bytecodes::IsSwitch(bytecode)) {
        for (auto offset : iterator.GetJumpTableTargetOffsets()) {
          predecessors_[offset.target_offset]++;
        }
      } else if (interpreter::Bytecodes::Returns(bytecode) ||
                 interpreter::Bytecodes::UnconditionallyThrows(bytecode)) {
        predecessors_[iterator.next_offset()]--;
        // Collect inline return jumps in the slot after the last bytecode.
        if (is_inline() && interpreter::Bytecodes::Returns(bytecode)) {
          predecessors_[array_length - 1]++;
        }
      }
      // TODO(leszeks): Also consider handler entries (the bytecode analysis)
      // will do this automatically I guess if we merge this into that.
    }
    if (!is_inline()) {
      DCHECK_EQ(0, predecessors_[bytecode().length()]);
    }
  }

  int NumPredecessors(int offset) { return predecessors_[offset]; }

  compiler::JSHeapBroker* broker() const { return compilation_unit_->broker(); }
  const compiler::FeedbackVectorRef& feedback() const {
    return compilation_unit_->feedback();
  }
  const FeedbackNexus FeedbackNexusForOperand(int slot_operand_index) const {
    return FeedbackNexus(feedback().object(),
                         GetSlotOperand(slot_operand_index),
                         broker()->feedback_nexus_config());
  }
  const FeedbackNexus FeedbackNexusForSlot(FeedbackSlot slot) const {
    return FeedbackNexus(feedback().object(), slot,
                         broker()->feedback_nexus_config());
  }
  const compiler::BytecodeArrayRef& bytecode() const {
    return compilation_unit_->bytecode();
  }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return bytecode_analysis_;
  }
  LocalIsolate* local_isolate() const { return local_isolate_; }
  Zone* zone() const { return compilation_unit_->zone(); }
  int parameter_count() const { return compilation_unit_->parameter_count(); }
  int parameter_count_without_receiver() { return parameter_count() - 1; }
  int register_count() const { return compilation_unit_->register_count(); }
  bool has_graph_labeller() const {
    return compilation_unit_->has_graph_labeller();
  }
  MaglevGraphLabeller* graph_labeller() const {
    return compilation_unit_->graph_labeller();
  }
  KnownNodeAspects& known_node_aspects() {
    return current_interpreter_frame_.known_node_aspects();
  }

  // True when this graph builder is building the subgraph of an inlined
  // function.
  bool is_inline() const { return parent_ != nullptr; }

  // The fake offset used as a target for all exits of an inlined function.
  int inline_exit_offset() const {
    DCHECK(is_inline());
    return bytecode().length();
  }

  LocalIsolate* const local_isolate_;
  MaglevCompilationUnit* const compilation_unit_;
  MaglevGraphBuilder* const parent_;
  Graph* const graph_;
  compiler::BytecodeAnalysis bytecode_analysis_;
  interpreter::BytecodeArrayIterator iterator_;
  SourcePositionTableIterator source_position_iterator_;
  uint32_t* predecessors_;

  // Current block information.
  BasicBlock* current_block_ = nullptr;
  base::Optional<InterpretedDeoptFrame> latest_checkpointed_frame_;
  SourcePosition current_source_position_;

  BasicBlockRef* jump_targets_;
  MergePointInterpreterFrameState** merge_states_;

  InterpreterFrameState current_interpreter_frame_;
  compiler::FeedbackSource current_speculation_feedback_;

  struct HandlerTableEntry {
    int end;
    int handler;
  };
  ZoneStack<HandlerTableEntry> catch_block_stack_;
  int next_handler_table_index_ = 0;

#ifdef DEBUG
  std::unordered_set<Node*> new_nodes_;
#endif
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
