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
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {
namespace maglev {

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
  ValueNode* GetTaggedArgument(int i);
  void BuildRegisterFrameInitialization();
  void BuildMergeStates();
  BasicBlock* EndPrologue();

  void BuildBody() {
    for (iterator_.Reset(); !iterator_.done(); iterator_.Advance()) {
      VisitSingleBytecode();
    }
  }

  Graph* graph() const { return graph_; }

 private:
  BasicBlock* CreateEmptyBlock(int offset, BasicBlock* predecessor) {
    DCHECK_NULL(current_block_);
    current_block_ = zone()->New<BasicBlock>(nullptr);
    BasicBlock* result = CreateBlock<Jump>({}, &jump_targets_[offset]);
    result->set_empty_block_predecessor(predecessor);
    return result;
  }

  void ProcessMergePointAtExceptionHandlerStart(int offset) {
    MergePointInterpreterFrameState& merge_state = *merge_states_[offset];
    DCHECK_EQ(merge_state.predecessor_count(), 0);

    // Copy state.
    current_interpreter_frame_.CopyFrom(*compilation_unit_, merge_state);

    // Merges aren't simple fallthroughs, so we should reset the checkpoint
    // validity.
    latest_checkpointed_state_.reset();

    // Register exception phis.
    if (has_graph_labeller()) {
      for (Phi* phi : *merge_states_[offset]->phis()) {
        graph_labeller()->RegisterNode(phi);
        if (FLAG_trace_maglev_graph_building) {
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
    latest_checkpointed_state_.reset();

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
        predecessor = CreateEmptyBlock(offset, predecessor);
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
        if (FLAG_trace_maglev_graph_building) {
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
    BasicBlock* block = CreateBlock<Deopt>({}, reason);
    ResolveJumpsToBlockAtOffset(block, block_offset_);

    MarkBytecodeDead();
  }

  void MarkBytecodeDead() {
    DCHECK_NULL(current_block_);
    if (FLAG_trace_maglev_graph_building) {
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
    } else if (bytecode == interpreter::Bytecode::kSuspendGenerator) {
      // Extra special case for SuspendGenerator, if the suspend is dead then
      // the resume has to be dead too. However, the resume already has a merge
      // state, with exactly one predecessor (the generator switch), so it will
      // be revived along the standard path. This can cause havoc if e.g. the
      // suspend/resume are inside a dead loop, because the JumpLoop can become
      // live again.
      //
      // So, manually advance the iterator to the resume, go through the motions
      // of processing the merge state, but immediately emit an unconditional
      // deopt (which also kills the resume).
      iterator_.Advance();
      DCHECK_EQ(iterator_.current_bytecode(),
                interpreter::Bytecode::kResumeGenerator);
      int resume_offset = iterator_.current_offset();
      DCHECK_EQ(NumPredecessors(resume_offset), 1);
      ProcessMergePoint(resume_offset);
      StartNewBlock(resume_offset);
      // TODO(v8:7700): This approach is not ideal. We can create a deopt-reopt
      // loop: the interpreted code runs, creates a generator while feedback is
      // still not yet allocated, then suspends the generator, tiers up to
      // maglev, and reaches this deopt. We then deopt, but since the generator
      // is never created again, we re-opt without the suspend part and we loop!
      EmitUnconditionalDeopt(DeoptimizeReason::kSuspendGeneratorIsDead);
      return;
    }

    // TODO(leszeks): We could now continue iterating the bytecode
  }

  void VisitSingleBytecode() {
    int offset = iterator_.current_offset();
    MergePointInterpreterFrameState* merge_state = merge_states_[offset];
    if (V8_UNLIKELY(merge_state != nullptr)) {
      if (current_block_ != nullptr) {
        // TODO(leszeks): Re-evaluate this DCHECK, we might hit it if the only
        // bytecodes in this basic block were only register juggling.
        // DCHECK(!current_block_->nodes().is_empty());
        FinishBlock<Jump>(offset, {}, &jump_targets_[offset]);

        merge_state->Merge(*compilation_unit_, current_interpreter_frame_,
                           graph()->last_block(), offset);
      }
      if (FLAG_trace_maglev_graph_building) {
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
    if (FLAG_trace_maglev_graph_building) {
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
    if (FLAG_trace_maglev_graph_building) {
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
      return NodeBase::New<NodeT>(zone(), *compilation_unit_,
                                  GetLatestCheckpointedState(),
                                  std::forward<Args>(args)...);
    } else if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      return NodeBase::New<NodeT>(zone(), *compilation_unit_,
                                  GetCheckpointedStateForLazyDeopt(),
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
      compiler::FeedbackSource& feedback,
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
    // Also check that the builtin does not allow var args.
    DCHECK_EQ(Descriptor::kAllowVarArgs, false);
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
    BasicBlock* block = CreateBlock<Abort>({}, reason);
    ResolveJumpsToBlockAtOffset(block, block_offset_);
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
    if (constant != constant) {
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

    // TODO(verwaest): Handle roots.
    const compiler::HeapObjectRef& constant = ref.AsHeapObject();
    auto it = graph_->constants().find(constant);
    if (it == graph_->constants().end()) {
      Constant* node = CreateNewNode<Constant>(0, constant);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->constants().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  bool IsConstantNodeTheHole(ValueNode* value) {
    DCHECK(IsConstantNode(value->opcode()));
    if (RootConstant* constant = value->TryCast<RootConstant>()) {
      return constant->index() == RootIndex::kTheHoleValue;
    }
    if (Constant* constant = value->TryCast<Constant>()) {
      return constant->IsTheHole();
    }
    // The other constants nodes cannot be TheHole.
    return false;
  }

  // Move an existing ValueNode between two registers. You can pass
  // virtual_accumulator as the src or dst to move in or out of the accumulator.
  void MoveNodeBetweenRegisters(interpreter::Register src,
                                interpreter::Register dst) {
    // We shouldn't be moving newly created nodes between registers.
    DCHECK_EQ(0, new_nodes_.count(current_interpreter_frame_.get(src)));
    DCHECK_NOT_NULL(current_interpreter_frame_.get(src));

    current_interpreter_frame_.set(dst, current_interpreter_frame_.get(src));
  }

  template <typename NodeT>
  ValueNode* AddNewConversionNode(interpreter::Register reg, ValueNode* node) {
    // TODO(v8:7700): Use a canonical conversion node. Maybe like in Phi nodes
    // where we always add a the conversion immediately after the ValueNode.
    DCHECK(NodeT::kProperties.is_conversion());
    ValueNode* result = AddNewNode<NodeT>({node});
    current_interpreter_frame_.set(reg, result);
    return result;
  }

  ValueNode* GetTaggedValueHelper(interpreter::Register reg, ValueNode* value) {
    // TODO(victorgomes): Consider adding the representation in the
    // InterpreterFrameState, so that we don't need to derefence a node.
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kTagged:
        return value;
      case ValueRepresentation::kInt32: {
        if (value->Is<CheckedSmiUntag>()) {
          return value->input(0).node();
        }
        return AddNewConversionNode<CheckedSmiTag>(reg, value);
      }
      case ValueRepresentation::kFloat64: {
        if (value->Is<CheckedFloat64Unbox>()) {
          return value->input(0).node();
        }
        if (value->Is<ChangeInt32ToFloat64>()) {
          ValueNode* int32_value = value->input(0).node();
          return GetTaggedValueHelper(reg, int32_value);
        }
        return AddNewConversionNode<Float64Box>(reg, value);
      }
    }
    UNREACHABLE();
  }

  ValueNode* GetTaggedValue(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return GetTaggedValueHelper(reg, value);
  }

  template <typename ConversionNodeT>
  ValueNode* GetValue(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return AddNewConversionNode<ConversionNodeT>(reg, value);
  }

  ValueNode* GetInt32(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kTagged: {
        if (value->Is<CheckedSmiTag>()) {
          return value->input(0).node();
        } else if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
          return GetInt32Constant(constant->value().value());
        }
        return AddNewConversionNode<CheckedSmiUntag>(reg, value);
      }
      case ValueRepresentation::kInt32:
        return value;
      case ValueRepresentation::kFloat64:
        // We should not be able to request an Int32 from a Float64 input,
        // unless it's an unboxing of a tagged value or a conversion from int32.
        if (value->Is<CheckedFloat64Unbox>()) {
          // TODO(leszeks): Maybe convert the CheckedFloat64Unbox to
          // ChangeInt32ToFloat64 with this CheckedSmiUntag as the input.
          return AddNewConversionNode<CheckedSmiUntag>(reg,
                                                       value->input(0).node());
        } else if (value->Is<ChangeInt32ToFloat64>()) {
          return value->input(0).node();
        }
        UNREACHABLE();
    }
    UNREACHABLE();
  }

  ValueNode* GetFloat64(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    switch (value->properties().value_representation()) {
      case ValueRepresentation::kTagged: {
        if (value->Is<Float64Box>()) {
          return value->input(0).node();
        }
        return AddNewConversionNode<CheckedFloat64Unbox>(reg, value);
      }
      case ValueRepresentation::kInt32:
        return AddNewConversionNode<ChangeInt32ToFloat64>(reg, value);
      case ValueRepresentation::kFloat64:
        return value;
    }
    UNREACHABLE();
  }

  template <typename ConversionNodeT>
  ValueNode* GetAccumulator() {
    return GetValue<ConversionNodeT>(
        interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorTagged() {
    return GetTaggedValue(interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorInt32() {
    return GetInt32(interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorFloat64() {
    return GetFloat64(interpreter::Register::virtual_accumulator());
  }

  bool IsRegisterEqualToAccumulator(int operand_index) {
    interpreter::Register source = iterator_.GetRegisterOperand(operand_index);
    return current_interpreter_frame_.get(source) ==
           current_interpreter_frame_.accumulator();
  }

  template <typename ConversionNodeT>
  ValueNode* LoadRegister(int operand_index) {
    return GetValue<ConversionNodeT>(
        iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterTagged(int operand_index) {
    return GetTaggedValue(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterInt32(int operand_index) {
    return GetInt32(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadRegisterFloat64(int operand_index) {
    return GetFloat64(iterator_.GetRegisterOperand(operand_index));
  }

  ValueNode* LoadFixedArrayElement(ValueNode* node, int index) {
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

  CheckpointedInterpreterState GetLatestCheckpointedState() {
    if (!latest_checkpointed_state_) {
      latest_checkpointed_state_.emplace(
          BytecodeOffset(iterator_.current_offset()),
          zone()->New<CompactInterpreterFrameState>(
              *compilation_unit_, GetInLiveness(), current_interpreter_frame_),
          parent_ == nullptr
              ? nullptr
              // TODO(leszeks): Don't always allocate for the parent state,
              // maybe cache it on the graph builder?
              : zone()->New<CheckpointedInterpreterState>(
                    parent_->GetLatestCheckpointedState()));
    }
    return *latest_checkpointed_state_;
  }

  CheckpointedInterpreterState GetCheckpointedStateForLazyDeopt() {
    return CheckpointedInterpreterState(
        BytecodeOffset(iterator_.current_offset()),
        zone()->New<CompactInterpreterFrameState>(
            *compilation_unit_, GetOutLiveness(), current_interpreter_frame_),
        // TODO(leszeks): Support lazy deopts in inlined functions.
        nullptr);
  }

  template <typename NodeT>
  void MarkAsLazyDeoptResult(NodeT* value,
                             interpreter::Register result_location,
                             int result_size) {
    DCHECK_EQ(NodeT::kProperties.can_lazy_deopt(),
              value->properties().can_lazy_deopt());
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      DCHECK(result_location.is_valid());
      DCHECK(!value->lazy_deopt_info()->result_location.is_valid());
      value->lazy_deopt_info()->result_location = result_location;
      value->lazy_deopt_info()->result_size = result_size;
    }
  }

  void MarkPossibleSideEffect() {
    // If there was a potential side effect, invalidate the previous checkpoint.
    latest_checkpointed_state_.reset();

    // A side effect could change existing objects' maps. For stable maps we
    // know this hasn't happened (because we added a dependency on the maps
    // staying stable and therefore not possible to transition away from), but
    // we can no longer assume that objects with unstable maps still have the
    // same map.
    known_node_aspects().unstable_maps.clear();
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
    block_offset_ = offset;
  }

  template <typename ControlNodeT, typename... Args>
  BasicBlock* CreateBlock(std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args) {
    ControlNode* control_node = CreateNewNode<ControlNodeT>(
        control_inputs, std::forward<Args>(args)...);
    current_block_->set_control_node(control_node);

    BasicBlock* block = current_block_;
    current_block_ = nullptr;

    graph()->Add(block);
    if (has_graph_labeller()) {
      graph_labeller()->RegisterBasicBlock(block);
      if (FLAG_trace_maglev_graph_building) {
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
  void ResolveJumpsToBlockAtOffset(BasicBlock* block, int block_offset) const {
    BasicBlockRef* jump_target_refs_head =
        jump_targets_[block_offset].SetToBlockAndReturnNext(block);
    while (jump_target_refs_head != nullptr) {
      jump_target_refs_head =
          jump_target_refs_head->SetToBlockAndReturnNext(block);
    }
    DCHECK_EQ(jump_targets_[block_offset].block_ptr(), block);
  }

  template <typename ControlNodeT, typename... Args>
  BasicBlock* FinishBlock(int next_block_offset,
                          std::initializer_list<ValueNode*> control_inputs,
                          Args&&... args) {
    BasicBlock* block =
        CreateBlock<ControlNodeT>(control_inputs, std::forward<Args>(args)...);
    ResolveJumpsToBlockAtOffset(block, block_offset_);

    // Start a new block for the fallthrough path, unless it's a merge point, in
    // which case we merge our state into it. That merge-point could also be a
    // loop header, in which case the merge state might not exist yet (if the
    // only predecessors are this path and the JumpLoop).
    DCHECK_NULL(current_block_);
    if (std::is_base_of<ConditionalControlNode, ControlNodeT>::value) {
      if (NumPredecessors(next_block_offset) == 1) {
        if (FLAG_trace_maglev_graph_building) {
          std::cout << "== New block (single fallthrough) ==" << std::endl;
        }
        StartNewBlock(next_block_offset);
      } else {
        MergeIntoFrameState(block, next_block_offset);
      }
    }
    return block;
  }

  void InlineCallFromRegisters(int argc_count,
                               ConvertReceiverMode receiver_mode,
                               compiler::JSFunctionRef function);

  void BuildCallFromRegisterList(ConvertReceiverMode receiver_mode);
  void BuildCallFromRegisters(int argc_count,
                              ConvertReceiverMode receiver_mode);

  bool TryBuildPropertyCellAccess(
      const compiler::GlobalAccessFeedback& global_access_feedback);

  void BuildCheckSmi(ValueNode* object);
  void BuildCheckHeapObject(ValueNode* object);
  void BuildCheckString(ValueNode* object);
  void BuildCheckSymbol(ValueNode* object);
  void BuildMapCheck(ValueNode* object, const compiler::MapRef& map);

  bool TryBuildMonomorphicLoad(ValueNode* receiver,
                               ValueNode* lookup_start_object,
                               const compiler::MapRef& map,
                               MaybeObjectHandle handler);
  bool TryBuildMonomorphicLoadFromSmiHandler(ValueNode* receiver,
                                             ValueNode* lookup_start_object,
                                             const compiler::MapRef& map,
                                             int32_t handler);
  bool TryBuildMonomorphicLoadFromLoadHandler(ValueNode* receiver,
                                              ValueNode* lookup_start_object,
                                              const compiler::MapRef& map,
                                              LoadHandler handler);

  bool TryBuildMonomorphicElementLoad(ValueNode* object, ValueNode* index,
                                      const compiler::MapRef& map,
                                      MaybeObjectHandle handler);
  bool TryBuildMonomorphicElementLoadFromSmiHandler(ValueNode* object,
                                                    ValueNode* index,
                                                    const compiler::MapRef& map,
                                                    int32_t handler);

  bool TryBuildMonomorphicStore(ValueNode* object, const compiler::MapRef& map,
                                MaybeObjectHandle handler);
  bool TryBuildMonomorphicStoreFromSmiHandler(ValueNode* object,
                                              const compiler::MapRef& map,
                                              int32_t handler);

  template <Operation kOperation>
  void BuildGenericUnaryOperationNode();
  template <Operation kOperation>
  void BuildGenericBinaryOperationNode();
  template <Operation kOperation>
  void BuildGenericBinarySmiOperationNode();

  template <Operation kOperation>
  ValueNode* AddNewInt32BinaryOperationNode(
      std::initializer_list<ValueNode*> inputs);
  template <Operation kOperation>
  ValueNode* AddNewFloat64BinaryOperationNode(
      std::initializer_list<ValueNode*> inputs);

  template <Operation kOperation>
  void BuildInt32BinaryOperationNode();
  template <Operation kOperation>
  void BuildInt32BinarySmiOperationNode();
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

  template <typename CompareControlNode>
  bool TryBuildCompareOperation(Operation operation, ValueNode* left,
                                ValueNode* right);
  template <Operation kOperation>
  void VisitCompareOperation();

  void MergeIntoFrameState(BasicBlock* block, int target);
  void MergeDeadIntoFrameState(int target);
  void MergeDeadLoopIntoFrameState(int target);
  void MergeIntoInlinedReturnFrameState(BasicBlock* block);
  void BuildBranchIfRootConstant(ValueNode* node, int true_target,
                                 int false_target, RootIndex root_index);
  void BuildBranchIfTrue(ValueNode* node, int true_target, int false_target);
  void BuildBranchIfNull(ValueNode* node, int true_target, int false_target);
  void BuildBranchIfUndefined(ValueNode* node, int true_target,
                              int false_target);
  void BuildBranchIfToBooleanTrue(ValueNode* node, int true_target,
                                  int false_target);

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
    return compilation_unit_->bytecode_analysis();
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
  interpreter::BytecodeArrayIterator iterator_;
  uint32_t* predecessors_;

  // Current block information.
  BasicBlock* current_block_ = nullptr;
  int block_offset_ = 0;
  base::Optional<CheckpointedInterpreterState> latest_checkpointed_state_;

  BasicBlockRef* jump_targets_;
  MergePointInterpreterFrameState** merge_states_;

  InterpreterFrameState current_interpreter_frame_;

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
