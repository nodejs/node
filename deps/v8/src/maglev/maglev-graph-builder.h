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
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/feedback-source.h"
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
#include "src/objects/elements-kind.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {
namespace maglev {

class CallArguments;

class ReduceResult {
 public:
  enum Kind {
    kDoneWithValue = 0,  // No need to mask while returning the pointer.
    kDoneWithAbort,
    kDoneWithoutValue,
    kFail,
    kNone,
  };

  ReduceResult() : payload_(kNone) {}

  // NOLINTNEXTLINE
  ReduceResult(ValueNode* value) : payload_(value) { DCHECK_NOT_NULL(value); }

  ValueNode* value() const {
    DCHECK(HasValue());
    return payload_.GetPointerWithKnownPayload(kDoneWithValue);
  }
  bool HasValue() const { return kind() == kDoneWithValue; }

  static ReduceResult Done(ValueNode* value) { return ReduceResult(value); }
  static ReduceResult Done() { return ReduceResult(kDoneWithoutValue); }
  static ReduceResult DoneWithAbort() { return ReduceResult(kDoneWithAbort); }
  static ReduceResult Fail() { return ReduceResult(kFail); }

  ReduceResult(const ReduceResult&) V8_NOEXCEPT = default;
  ReduceResult& operator=(const ReduceResult&) V8_NOEXCEPT = default;

  // No/undefined result, created by default constructor.
  bool IsNone() const { return kind() == kNone; }

  // Either DoneWithValue, DoneWithoutValue or DoneWithAbort.
  bool IsDone() const { return !IsFail() && !IsNone(); }

  // ReduceResult failed.
  bool IsFail() const { return kind() == kFail; }

  // Done with a ValueNode.
  bool IsDoneWithValue() const { return HasValue(); }

  // Done without producing a ValueNode.
  bool IsDoneWithoutValue() const { return kind() == kDoneWithoutValue; }

  // Done with an abort (unconditional deopt, infinite loop in an inlined
  // function, etc)
  bool IsDoneWithAbort() const { return kind() == kDoneWithAbort; }

  Kind kind() const { return payload_.GetPayload(); }

 private:
  explicit ReduceResult(Kind kind) : payload_(kind) {}
  base::PointerWithPayload<ValueNode, Kind, 3> payload_;
};

struct FastLiteralField;

// Encoding of a fast allocation site object's element fixed array.
struct FastLiteralFixedArray {
  FastLiteralFixedArray() : type(kUninitialized) {}
  explicit FastLiteralFixedArray(compiler::ObjectRef cow_value)
      : type(kCoW), cow_value(cow_value) {}
  explicit FastLiteralFixedArray(int length, Zone* zone)
      : type(kTagged),
        length(length),
        values(zone->NewArray<FastLiteralField>(length)) {}
  explicit FastLiteralFixedArray(int length, Zone* zone, double)
      : type(kDouble),
        length(length),
        double_values(zone->NewArray<Float64>(length)) {}

  enum { kUninitialized, kCoW, kTagged, kDouble } type;

  union {
    char uninitialized_marker;

    compiler::ObjectRef cow_value;

    struct {
      int length;
      union {
        FastLiteralField* values;
        Float64* double_values;
      };
    };
  };
};

// Encoding of a fast allocation site boilerplate object.
struct FastLiteralObject {
  FastLiteralObject(compiler::MapRef map, Zone* zone,
                    FastLiteralFixedArray elements)
      : map(map),
        fields(zone->NewArray<FastLiteralField>(map.GetInObjectProperties())),
        elements(elements) {}

  compiler::MapRef map;
  FastLiteralField* fields;
  FastLiteralFixedArray elements;
  compiler::OptionalObjectRef js_array_length;
};

// Encoding of a fast allocation site literal value.
struct FastLiteralField {
  FastLiteralField() : type(kUninitialized) {}
  explicit FastLiteralField(FastLiteralObject object)
      : type(kObject), object(object) {}
  explicit FastLiteralField(Float64 mutable_double_value)
      : type(kMutableDouble), mutable_double_value(mutable_double_value) {}
  explicit FastLiteralField(compiler::ObjectRef constant_value)
      : type(kConstant), constant_value(constant_value) {}

  enum { kUninitialized, kObject, kMutableDouble, kConstant } type;

  union {
    char uninitialized_marker;
    FastLiteralObject object;
    Float64 mutable_double_value;
    compiler::ObjectRef constant_value;
  };
};

#define RETURN_IF_DONE(result) \
  do {                         \
    auto res = result;         \
    if (res.IsDone()) {        \
      return res;              \
    }                          \
  } while (false)

#define PROCESS_AND_RETURN_IF_DONE(result, value_processor) \
  do {                                                      \
    auto res = result;                                      \
    if (res.IsDone()) {                                     \
      if (res.IsDoneWithValue()) {                          \
        value_processor(res.value());                       \
      }                                                     \
      return;                                               \
    }                                                       \
  } while (false)

#define RETURN_IF_ABORT(result)           \
  if (result.IsDoneWithAbort()) {         \
    return ReduceResult::DoneWithAbort(); \
  }

class MaglevGraphBuilder {
 public:
  explicit MaglevGraphBuilder(
      LocalIsolate* local_isolate, MaglevCompilationUnit* compilation_unit,
      Graph* graph, float call_frequency = 1.0f,
      BytecodeOffset bytecode_offset = BytecodeOffset::None(),
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

    // Don't use the AddNewNode helper for the function entry stack check, so
    // that we can set a custom deopt frame on it.
    FunctionEntryStackCheck* function_entry_stack_check =
        FunctionEntryStackCheck::New(zone(), {});
    new (function_entry_stack_check->lazy_deopt_info()) LazyDeoptInfo(
        zone(), GetDeoptFrameForEntryStackCheck(),
        interpreter::Register::invalid_value(), 0, compiler::FeedbackSource());
    AddInitializedNodeToGraph(function_entry_stack_check);

    BuildMergeStates();
    EndPrologue();
    BuildBody();
  }

  ReduceResult BuildInlined(const CallArguments& args, BasicBlockRef* start_ref,
                            BasicBlockRef* end_ref);

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

  Int32Constant* GetInt32Constant(int constant) {
    // The constant must fit in a Smi, since it could be later tagged in a Phi.
    DCHECK(Smi::IsValid(constant));
    auto it = graph_->int32().find(constant);
    if (it == graph_->int32().end()) {
      Int32Constant* node = CreateNewConstantNode<Int32Constant>(0, constant);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->int32().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  Float64Constant* GetFloat64Constant(double constant) {
    return GetFloat64Constant(
        Float64::FromBits(base::double_to_uint64(constant)));
  }

  Float64Constant* GetFloat64Constant(Float64 constant) {
    auto it = graph_->float64().find(constant.get_bits());
    if (it == graph_->float64().end()) {
      Float64Constant* node =
          CreateNewConstantNode<Float64Constant>(0, constant);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->float64().emplace(constant.get_bits(), node);
      return node;
    }
    return it->second;
  }

  Graph* graph() const { return graph_; }
  Zone* zone() const { return compilation_unit_->zone(); }
  MaglevCompilationUnit* compilation_unit() const { return compilation_unit_; }

  bool has_graph_labeller() const {
    return compilation_unit_->has_graph_labeller();
  }
  MaglevGraphLabeller* graph_labeller() const {
    return compilation_unit_->graph_labeller();
  }

 private:
  class CallSpeculationScope;
  class LazyDeoptContinuationScope;

  bool CheckType(ValueNode* node, NodeType type);
  bool EnsureType(ValueNode* node, NodeType type, NodeType* old = nullptr);
  bool ShouldEmitInterruptBudgetChecks() {
    return v8_flags.force_emit_interrupt_budget_checks || v8_flags.turbofan;
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
    if (v8_flags.maglev_increase_budget_forward_jump &&
        ShouldEmitInterruptBudgetChecks() && interrupt_budget_correction != 0) {
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
    // and for-in state validity.
    latest_checkpointed_frame_.reset();
    current_for_in_state.receiver_needs_map_check = true;

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
                           current_interpreter_frame_, predecessor);
      }
      if (v8_flags.trace_maglev_graph_building) {
        auto detail = merge_state->is_exception_handler() ? "exception handler"
                      : merge_state->is_loop()            ? "loop header"
                                                          : "merge";
        std::cout << "== New block (" << detail << ") at " << function()
                  << "==" << std::endl;
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

  void AddInitializedNodeToGraph(Node* node) {
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
  }

  // Add a new node with a dynamic set of inputs which are initialized by the
  // `post_create_input_initializer` function before the node is added to the
  // graph.
  template <typename NodeT, typename Function, typename... Args>
  NodeT* AddNewNode(size_t input_count,
                    Function&& post_create_input_initializer, Args&&... args) {
    NodeT* node =
        NodeBase::New<NodeT>(zone(), input_count, std::forward<Args>(args)...);
    post_create_input_initializer(node);
    return AttachExtraInfoAndAddToGraph(node);
  }

  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(std::initializer_list<ValueNode*> inputs, Args&&... args) {
    NodeT* node =
        NodeBase::New<NodeT>(zone(), inputs, std::forward<Args>(args)...);
    return AttachExtraInfoAndAddToGraph(node);
  }

  template <typename NodeT, typename... Args>
  NodeT* CreateNewConstantNode(Args&&... args) {
    static_assert(IsConstantNode(Node::opcode_of<NodeT>));
    NodeT* node = NodeBase::New<NodeT>(zone(), std::forward<Args>(args)...);
    static_assert(!NodeT::kProperties.can_eager_deopt());
    static_assert(!NodeT::kProperties.can_lazy_deopt());
    static_assert(!NodeT::kProperties.can_throw());
    static_assert(!NodeT::kProperties.has_any_side_effects());
    return node;
  }

  template <typename NodeT>
  NodeT* AttachExtraInfoAndAddToGraph(NodeT* node) {
    AttachEagerDeoptInfo(node);
    AttachLazyDeoptInfo(node);
    AttachExceptionHandlerInfo(node);
    MarkPossibleSideEffect(node);
    AddInitializedNodeToGraph(node);
    return node;
  }

  template <typename NodeT>
  void AttachEagerDeoptInfo(NodeT* node) {
    if constexpr (NodeT::kProperties.can_eager_deopt()) {
      new (node->eager_deopt_info()) EagerDeoptInfo(
          zone(), GetLatestCheckpointedFrame(), current_speculation_feedback_);
    }
  }

  template <typename NodeT>
  void AttachLazyDeoptInfo(NodeT* node) {
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      auto [register_result, register_count] = GetResultLocationAndSize();
      new (node->lazy_deopt_info())
          LazyDeoptInfo(zone(), GetDeoptFrameForLazyDeopt(), register_result,
                        register_count, current_speculation_feedback_);
    }
  }

  template <typename NodeT>
  void AttachExceptionHandlerInfo(NodeT* node) {
    if constexpr (NodeT::kProperties.can_throw()) {
      BasicBlockRef* catch_block_ref = GetCurrentTryCatchBlockOffset();
      if (catch_block_ref) {
        new (node->exception_handler_info())
            ExceptionHandlerInfo(catch_block_ref);
      } else {
        // Patch no exception handler marker.
        // TODO(victorgomes): Avoid allocating exception handler data in this
        // case.
        new (node->exception_handler_info()) ExceptionHandlerInfo();
      }
    }
  }

  BasicBlockRef* GetCurrentTryCatchBlockOffset() {
    if (catch_block_stack_.size() > 0) {
      // Inside a try-block.
      return &jump_targets_[catch_block_stack_.top().handler];
    }
    // Function is inlined and the call is inside a catch block.
    if (parent_catch_block_) {
      DCHECK(is_inline());
      return parent_catch_block_;
    }
    return nullptr;
  }

  enum ContextSlotMutability { kImmutable, kMutable };
  bool TrySpecializeLoadContextSlotToFunctionContext(
      ValueNode** context, size_t* depth, int slot_index,
      ContextSlotMutability slot_mutability);
  ValueNode* LoadAndCacheContextSlot(ValueNode* context, int offset,
                                     ContextSlotMutability slot_mutability);
  void StoreAndCacheContextSlot(ValueNode* context, int offset,
                                ValueNode* value);
  void BuildLoadContextSlot(ValueNode* context, size_t depth, int slot_index,
                            ContextSlotMutability slot_mutability);
  void BuildStoreContextSlot(ValueNode* context, size_t depth, int slot_index,
                             ValueNode* value);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(std::initializer_list<ValueNode*> inputs) {
    using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
    if constexpr (Descriptor::HasContextParameter()) {
      return AddNewNode<CallBuiltin>(
          inputs.size() + 1,
          [&](CallBuiltin* call_builtin) {
            int arg_index = 0;
            for (auto* input : inputs) {
              call_builtin->set_arg(arg_index++, input);
            }
          },
          kBuiltin, GetContext());
    } else {
      return AddNewNode<CallBuiltin>(inputs, kBuiltin);
    }
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
    return AddNewNode<CallRuntime>(
        inputs.size() + CallRuntime::kFixedInputCount,
        [&](CallRuntime* call_runtime) {
          int arg_index = 0;
          for (auto* input : inputs) {
            call_runtime->set_arg(arg_index++, input);
          }
        },
        function_id, GetContext());
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
      SmiConstant* node =
          CreateNewConstantNode<SmiConstant>(0, Smi::FromInt(constant));
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->smi().emplace(constant, node);
      return node;
    }
    return it->second;
  }

  ExternalConstant* GetExternalConstant(ExternalReference reference) {
    auto it = graph_->external_references().find(reference.address());
    if (it == graph_->external_references().end()) {
      ExternalConstant* node =
          CreateNewConstantNode<ExternalConstant>(0, reference);
      if (has_graph_labeller()) graph_labeller()->RegisterNode(node);
      graph_->external_references().emplace(reference.address(), node);
      return node;
    }
    return it->second;
  }

  RootConstant* GetRootConstant(RootIndex index) {
    auto it = graph_->root().find(index);
    if (it == graph_->root().end()) {
      RootConstant* node = CreateNewConstantNode<RootConstant>(0, index);
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

  ValueNode* GetConstant(const compiler::ObjectRef& ref) {
    if (ref.IsSmi()) return GetSmiConstant(ref.AsSmi());
    const compiler::HeapObjectRef& constant = ref.AsHeapObject();

    auto root_index = broker()->FindRootIndex(constant);
    if (root_index.has_value()) {
      return GetRootConstant(*root_index);
    }

    auto it = graph_->constants().find(constant);
    if (it == graph_->constants().end()) {
      Constant* node = CreateNewConstantNode<Constant>(0, constant);
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
    DCHECK(!IsNodeCreatedForThisBytecode(current_interpreter_frame_.get(src)));
    DCHECK_NOT_NULL(current_interpreter_frame_.get(src));

    current_interpreter_frame_.set(dst, current_interpreter_frame_.get(src));
  }

  ValueNode* GetTaggedValue(ValueNode* value);

  ValueNode* GetTaggedValue(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return GetTaggedValue(value);
  }

  void SetKnownType(ValueNode* node, NodeType type) {
    NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(node);
    known_info->type = type;
  }

  ValueNode* GetInternalizedString(interpreter::Register reg);

  ValueNode* GetTruncatedInt32FromNumber(
      ValueNode* value, TaggedToFloat64ConversionType conversion_type);

  ValueNode* GetTruncatedInt32FromNumber(
      interpreter::Register reg,
      TaggedToFloat64ConversionType conversion_type) {
    return GetTruncatedInt32FromNumber(current_interpreter_frame_.get(reg),
                                       conversion_type);
  }

  ValueNode* GetUint32ClampedFromNumber(ValueNode* value);

  ValueNode* GetUint32ClampedFromNumber(interpreter::Register reg) {
    return GetUint32ClampedFromNumber(current_interpreter_frame_.get(reg));
  }

  ValueNode* GetTruncatedInt32(ValueNode* node);

  ValueNode* GetTruncatedInt32(interpreter::Register reg) {
    return GetTruncatedInt32(current_interpreter_frame_.get(reg));
  }

  ValueNode* GetInt32(ValueNode* value);

  ValueNode* GetInt32(interpreter::Register reg) {
    ValueNode* value = current_interpreter_frame_.get(reg);
    return GetInt32(value);
  }

  ValueNode* GetFloat64(ValueNode* value,
                        TaggedToFloat64ConversionType conversion_type);

  ValueNode* GetFloat64(interpreter::Register reg,
                        TaggedToFloat64ConversionType conversion_type) {
    return GetFloat64(current_interpreter_frame_.get(reg), conversion_type);
  }

  ValueNode* GetRawAccumulator() {
    return current_interpreter_frame_.get(
        interpreter::Register::virtual_accumulator());
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

  ValueNode* GetAccumulatorTruncatedInt32FromNumber(
      TaggedToFloat64ConversionType conversion_type) {
    return GetTruncatedInt32FromNumber(
        interpreter::Register::virtual_accumulator(), conversion_type);
  }

  ValueNode* GetAccumulatorUint32ClampedFromNumber() {
    return GetUint32ClampedFromNumber(
        interpreter::Register::virtual_accumulator());
  }

  ValueNode* GetAccumulatorFloat64(
      TaggedToFloat64ConversionType conversion_type) {
    return GetFloat64(interpreter::Register::virtual_accumulator(),
                      conversion_type);
  }

  ValueNode* GetSilencedNaN(ValueNode* value) {
    DCHECK_EQ(value->properties().value_representation(),
              ValueRepresentation::kFloat64);

    // We only need to check for silenced NaN in non-conversion nodes, since
    // conversions can't be signalling NaNs.
    if (value->properties().is_conversion()) return value;

    // Special case constants, since we know what they are.
    Float64Constant* constant = value->TryCast<Float64Constant>();
    if (constant) {
      constexpr double quiet_NaN = std::numeric_limits<double>::quiet_NaN();
      if (!constant->value().is_nan()) return constant;
      return GetFloat64Constant(quiet_NaN);
    }

    // Silence all other values.
    return AddNewNode<Float64SilenceNaN>({value});
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

  ValueNode* LoadRegisterFloat64(
      int operand_index, TaggedToFloat64ConversionType conversion_type) {
    return GetFloat64(iterator_.GetRegisterOperand(operand_index),
                      conversion_type);
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
    static_assert(std::is_base_of_v<ValueNode, NodeT>);
    DCHECK(HasOutputRegister(target));
    current_interpreter_frame_.set(target, value);

    // Make sure the lazy deopt info of this value, if any, is registered as
    // mutating this register.
    DCHECK_IMPLIES(value->properties().can_lazy_deopt() &&
                       IsNodeCreatedForThisBytecode(value),
                   value->lazy_deopt_info()->IsResultRegister(target));
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
    DCHECK(HasOutputRegister(target0));
    current_interpreter_frame_.set(target0, value);

    ValueNode* second_value = GetSecondValue(value);
    DCHECK_NE(0, new_nodes_.count(second_value));
    DCHECK(HasOutputRegister(target1));
    current_interpreter_frame_.set(target1, second_value);

    // Make sure the lazy deopt info of this value, if any, is registered as
    // mutating these registers.
    DCHECK_IMPLIES(value->properties().can_lazy_deopt() &&
                       IsNodeCreatedForThisBytecode(value),
                   value->lazy_deopt_info()->IsResultRegister(target0));
    DCHECK_IMPLIES(value->properties().can_lazy_deopt() &&
                       IsNodeCreatedForThisBytecode(value),
                   value->lazy_deopt_info()->IsResultRegister(target1));
  }

  std::pair<interpreter::Register, int> GetResultLocationAndSize() const;
#ifdef DEBUG
  bool HasOutputRegister(interpreter::Register reg) const;
#endif

  DeoptFrame* GetParentDeoptFrame();
  DeoptFrame GetLatestCheckpointedFrame();
  DeoptFrame GetDeoptFrameForLazyDeopt();
  DeoptFrame GetDeoptFrameForLazyDeoptHelper(
      LazyDeoptContinuationScope* continuation_scope,
      bool mark_accumulator_dead);
  InterpretedDeoptFrame GetDeoptFrameForEntryStackCheck();

  template <typename NodeT>
  void MarkPossibleSideEffect(NodeT* node) {
    // Don't do anything for nodes without side effects.
    if constexpr (!NodeT::kProperties.has_any_side_effects()) return;

    // Simple field stores are stores which do nothing but change a field value
    // (i.e. no map transitions or calls into user code).
    static constexpr bool is_simple_field_store =
        std::is_same_v<NodeT, StoreTaggedFieldWithWriteBarrier> ||
        std::is_same_v<NodeT, StoreTaggedFieldNoWriteBarrier> ||
        std::is_same_v<NodeT, StoreDoubleField> ||
        std::is_same_v<NodeT, CheckedStoreSmiField>;

    // Don't change known node aspects for:
    //
    //   * Simple field stores -- the only relevant side effect on these is
    //     writes to objects which invalidate loaded properties and context
    //     slots, and we invalidate these already as part of emitting the store.
    //
    //   * CheckMapsWithMigration -- this only migrates representations of
    //     values, not the values themselves, so cached values are still valid.
    static constexpr bool should_clear_unstable_node_aspects =
        !is_simple_field_store &&
        !std::is_same_v<NodeT, CheckMapsWithMigration>;

    // Simple field stores can't possibly change or migrate the map.
    static constexpr bool is_possible_map_change = !is_simple_field_store;

    // We only need to clear unstable node aspects on the current builder, not
    // the parent, since we'll anyway copy the known_node_aspects to the parent
    // once we finish the inlined function.
    if constexpr (should_clear_unstable_node_aspects) {
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

    // Other kinds of side effect have to be propagated up to the parent.
    for (MaglevGraphBuilder* builder = this; builder != nullptr;
         builder = builder->parent_) {
      // All user-observable side effects need to clear the checkpointed frame.
      // TODO(leszeks): What side effects aren't observable? Maybe migrations?
      builder->latest_checkpointed_frame_.reset();

      // If a map might have changed, then we need to re-check it for for-in.
      if (is_possible_map_change) {
        builder->current_for_in_state.receiver_needs_map_check = true;
      }
    }
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
    ControlNodeT* control_node = NodeBase::New<ControlNodeT>(
        zone(), control_inputs, std::forward<Args>(args)...);
    AttachEagerDeoptInfo(control_node);
    static_assert(!ControlNodeT::kProperties.can_lazy_deopt());
    static_assert(!ControlNodeT::kProperties.can_throw());
    static_assert(!ControlNodeT::kProperties.has_any_side_effects());
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
    if (v8_flags.maglev_increase_budget_forward_jump &&
        ShouldEmitInterruptBudgetChecks() && interrupt_budget_correction != 0) {
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
        std::cout << "== New block (single fallthrough) at " << function()
                  << "==" << std::endl;
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
                                const CallArguments& args);

  template <typename LoadNode>
  ReduceResult TryBuildLoadDataView(const CallArguments& args,
                                    ExternalArrayType type);
  template <typename StoreNode, typename Function>
  ReduceResult TryBuildStoreDataView(const CallArguments& args,
                                     ExternalArrayType type,
                                     Function&& getValue);

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

#define MAGLEV_REDUCED_BUILTIN(V)  \
  V(DataViewPrototypeGetInt8)      \
  V(DataViewPrototypeSetInt8)      \
  V(DataViewPrototypeGetInt16)     \
  V(DataViewPrototypeSetInt16)     \
  V(DataViewPrototypeGetInt32)     \
  V(DataViewPrototypeSetInt32)     \
  V(DataViewPrototypeGetFloat64)   \
  V(DataViewPrototypeSetFloat64)   \
  V(FunctionPrototypeCall)         \
  V(ObjectPrototypeHasOwnProperty) \
  V(MathCeil)                      \
  V(MathFloor)                     \
  V(MathPow)                       \
  V(MathRound)                     \
  V(StringFromCharCode)            \
  V(StringPrototypeCharCodeAt)     \
  V(StringPrototypeCodePointAt)    \
  MATH_UNARY_IEEE_BUILTIN(V)

#define DEFINE_BUILTIN_REDUCER(Name)                                   \
  ReduceResult TryReduce##Name(compiler::JSFunctionRef builtin_target, \
                               CallArguments& args);
  MAGLEV_REDUCED_BUILTIN(DEFINE_BUILTIN_REDUCER)
#undef DEFINE_BUILTIN_REDUCER

  ReduceResult DoTryReduceMathRound(compiler::JSFunctionRef builtin_target,
                                    CallArguments& args,
                                    Float64Round::Kind kind);

  template <typename CallNode, typename... Args>
  CallNode* AddNewCallNode(const CallArguments& args, Args&&... extra_args);

  ValueNode* BuildCallSelf(compiler::JSFunctionRef function,
                           CallArguments& args);
  ReduceResult TryReduceBuiltin(compiler::JSFunctionRef builtin_target,
                                CallArguments& args,
                                const compiler::FeedbackSource& feedback_source,
                                SpeculationMode speculation_mode);
  bool TargetIsCurrentCompilingUnit(compiler::JSFunctionRef target);
  ReduceResult TryBuildCallKnownJSFunction(
      compiler::JSFunctionRef function, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  bool ShouldInlineCall(compiler::JSFunctionRef function, float call_frequency);
  ReduceResult TryBuildInlinedCall(
      compiler::JSFunctionRef function, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);
  ValueNode* BuildGenericCall(ValueNode* target, ValueNode* context,
                              Call::TargetType target_type,
                              const CallArguments& args,
                              const compiler::FeedbackSource& feedback_source =
                                  compiler::FeedbackSource());
  ReduceResult ReduceCall(
      compiler::ObjectRef target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source =
          compiler::FeedbackSource(),
      SpeculationMode speculation_mode = SpeculationMode::kDisallowSpeculation);
  ReduceResult ReduceCallForTarget(
      ValueNode* target_node, compiler::JSFunctionRef target,
      CallArguments& args, const compiler::FeedbackSource& feedback_source,
      SpeculationMode speculation_mode);
  ReduceResult ReduceFunctionPrototypeApplyCallWithReceiver(
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
  ValueNode* BuildNumberOrOddballToFloat64(
      ValueNode* node, TaggedToFloat64ConversionType conversion_type);

  void BuildCheckSmi(ValueNode* object);
  void BuildCheckNumber(ValueNode* object);
  void BuildCheckHeapObject(ValueNode* object);
  void BuildCheckString(ValueNode* object);
  void BuildCheckSymbol(ValueNode* object);
  void BuildCheckMaps(ValueNode* object,
                      base::Vector<const compiler::MapRef> maps);
  // Emits an unconditional deopt and returns false if the node is a constant
  // that doesn't match the ref.
  ReduceResult BuildCheckValue(ValueNode* node,
                               const compiler::HeapObjectRef& ref);

  bool CanElideWriteBarrier(ValueNode* object, ValueNode* value);
  void BuildStoreTaggedField(ValueNode* object, ValueNode* value, int offset);
  void BuildStoreTaggedFieldNoWriteBarrier(ValueNode* object, ValueNode* value,
                                           int offset);
  void BuildStoreFixedArrayElement(ValueNode* elements, ValueNode* index,
                                   ValueNode* value);

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
      base::Vector<const compiler::MapRef> const& receiver_maps);

  compiler::OptionalObjectRef TryFoldLoadDictPrototypeConstant(
      compiler::PropertyAccessInfo access_info);
  compiler::OptionalObjectRef TryFoldLoadConstantDataField(
      compiler::PropertyAccessInfo access_info, ValueNode* lookup_start_object);

  // Returns the loaded value node but doesn't update the accumulator yet.
  ValueNode* BuildLoadField(compiler::PropertyAccessInfo access_info,
                            ValueNode* lookup_start_object);
  ReduceResult TryBuildStoreField(compiler::PropertyAccessInfo access_info,
                                  ValueNode* receiver,
                                  compiler::AccessMode access_mode);
  ReduceResult TryBuildPropertyGetterCall(
      compiler::PropertyAccessInfo access_info, ValueNode* receiver,
      ValueNode* lookup_start_object);
  ReduceResult TryBuildPropertySetterCall(
      compiler::PropertyAccessInfo access_info, ValueNode* receiver,
      ValueNode* value);

  ReduceResult TryBuildPropertyLoad(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info);
  ReduceResult TryBuildPropertyStore(
      ValueNode* receiver, compiler::NameRef name,
      compiler::PropertyAccessInfo const& access_info,
      compiler::AccessMode access_mode);
  ReduceResult TryBuildPropertyAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NameRef name, compiler::PropertyAccessInfo const& access_info,
      compiler::AccessMode access_mode);
  ReduceResult TryBuildNamedAccess(
      ValueNode* receiver, ValueNode* lookup_start_object,
      compiler::NamedAccessFeedback const& feedback,
      compiler::FeedbackSource const& feedback_source,
      compiler::AccessMode access_mode);

  ValueNode* BuildLoadTypedArrayElement(ValueNode* object, ValueNode* index,
                                        ElementsKind elements_kind);
  void BuildStoreTypedArrayElement(ValueNode* object, ValueNode* index,
                                   ElementsKind elements_kind);

  ReduceResult TryBuildElementAccessOnString(
      ValueNode* object, ValueNode* index,
      compiler::KeyedAccessMode const& keyed_mode);
  ReduceResult TryBuildElementAccessOnTypedArray(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessInfo& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  ReduceResult TryBuildElementAccessOnJSArrayOrJSObject(
      ValueNode* object, ValueNode* index,
      const compiler::ElementAccessInfo& access_info,
      compiler::KeyedAccessMode const& keyed_mode);
  ReduceResult TryBuildElementAccess(
      ValueNode* object, ValueNode* index,
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

  ValueNode* ExtendOrReallocateCurrentRawAllocation(
      int size, AllocationType allocation_type);
  void ClearCurrentRawAllocation();

  ReduceResult TryBuildFastCreateObjectOrArrayLiteral(
      const compiler::LiteralFeedback& feedback);
  base::Optional<FastLiteralObject> TryReadBoilerplateForFastLiteral(
      compiler::JSObjectRef boilerplate, AllocationType allocation,
      int max_depth, int* max_properties);
  ValueNode* BuildAllocateFastLiteral(FastLiteralObject object,
                                      AllocationType allocation);
  ValueNode* BuildAllocateFastLiteral(FastLiteralField value,
                                      AllocationType allocation);
  ValueNode* BuildAllocateFastLiteral(FastLiteralFixedArray array,
                                      AllocationType allocation);

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
  void BuildTruncatingInt32BitwiseNotForNumber(
      TaggedToFloat64ConversionType conversion_type);
  template <Operation kOperation>
  void BuildInt32BinaryOperationNode();
  template <Operation kOperation>
  void BuildInt32BinarySmiOperationNode();
  template <Operation kOperation>
  void BuildTruncatingInt32BinaryOperationNodeForNumber(
      TaggedToFloat64ConversionType conversion_type);
  template <Operation kOperation>
  void BuildTruncatingInt32BinarySmiOperationNodeForNumber(
      TaggedToFloat64ConversionType conversion_type);
  template <Operation kOperation>
  void BuildFloat64UnaryOperationNode(
      TaggedToFloat64ConversionType conversion_type);
  template <Operation kOperation>
  void BuildFloat64BinaryOperationNode(
      TaggedToFloat64ConversionType conversion_type);
  template <Operation kOperation>
  void BuildFloat64BinarySmiOperationNode(
      TaggedToFloat64ConversionType conversion_type);

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

  compiler::JSHeapBroker* broker() const { return broker_; }
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
  const compiler::JSFunctionRef& function() const {
    return compilation_unit_->function();
  }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return bytecode_analysis_;
  }
  LocalIsolate* local_isolate() const { return local_isolate_; }
  int parameter_count() const { return compilation_unit_->parameter_count(); }
  int parameter_count_without_receiver() { return parameter_count() - 1; }
  int register_count() const { return compilation_unit_->register_count(); }
  KnownNodeAspects& known_node_aspects() {
    return *current_interpreter_frame_.known_node_aspects();
  }

  // True when this graph builder is building the subgraph of an inlined
  // function.
  bool is_inline() const { return parent_ != nullptr; }
  int inlining_depth() const { return compilation_unit_->inlining_depth(); }

  // The fake offset used as a target for all exits of an inlined function.
  int inline_exit_offset() const {
    DCHECK(is_inline());
    return bytecode().length();
  }

  LocalIsolate* const local_isolate_;
  MaglevCompilationUnit* const compilation_unit_;
  MaglevGraphBuilder* const parent_;
  DeoptFrame* parent_deopt_frame_ = nullptr;
  BasicBlockRef* parent_catch_block_ = nullptr;
  // Cache the heap broker since we access it a bunch.
  compiler::JSHeapBroker* broker_ = compilation_unit_->broker();

  Graph* const graph_;
  compiler::BytecodeAnalysis bytecode_analysis_;
  interpreter::BytecodeArrayIterator iterator_;
  SourcePositionTableIterator source_position_iterator_;
  uint32_t* predecessors_;

  // Current block information.
  BasicBlock* current_block_ = nullptr;
  base::Optional<InterpretedDeoptFrame> latest_checkpointed_frame_;
  SourcePosition current_source_position_;
  struct ForInState {
    ValueNode* receiver = nullptr;
    ValueNode* cache_type = nullptr;
    ValueNode* enum_cache = nullptr;
    ValueNode* key = nullptr;
    ValueNode* index = nullptr;
    bool receiver_needs_map_check = false;
  };
  // TODO(leszeks): Allow having a stack of these.
  ForInState current_for_in_state = ForInState();

  AllocateRaw* current_raw_allocation_ = nullptr;

  float call_frequency_;

  BasicBlockRef* jump_targets_;
  MergePointInterpreterFrameState** merge_states_;

  InterpreterFrameState current_interpreter_frame_;
  compiler::FeedbackSource current_speculation_feedback_;

  // TODO(victorgomes): Refactor all inlined data to a
  // base::Optional<InlinedGraphBuilderData>.
  // base::Vector<ValueNode*>* inlined_arguments_ = nullptr;
  base::Optional<base::Vector<ValueNode*>> inlined_arguments_;
  BytecodeOffset caller_bytecode_offset_;

  LazyDeoptContinuationScope* current_lazy_deopt_continuation_scope_ = nullptr;

  struct HandlerTableEntry {
    int end;
    int handler;
  };
  ZoneStack<HandlerTableEntry> catch_block_stack_;
  int next_handler_table_index_ = 0;

#ifdef DEBUG
  bool IsNodeCreatedForThisBytecode(ValueNode* node) const {
    return new_nodes_.find(node) != new_nodes_.end();
  }
  std::unordered_set<Node*> new_nodes_;
#endif
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_BUILDER_H_
