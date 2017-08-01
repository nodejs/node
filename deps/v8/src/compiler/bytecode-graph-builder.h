// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
#define V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_

#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-type-hint-lowering.h"
#include "src/compiler/liveness-analyzer.h"
#include "src/compiler/state-values-utils.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecodes.h"
#include "src/source-position-table.h"

namespace v8 {
namespace internal {
namespace compiler {

class Reduction;
class SourcePositionTable;

// The BytecodeGraphBuilder produces a high-level IR graph based on
// interpreter bytecodes.
class BytecodeGraphBuilder {
 public:
  BytecodeGraphBuilder(
      Zone* local_zone, Handle<SharedFunctionInfo> shared,
      Handle<FeedbackVector> feedback_vector, BailoutId osr_ast_id,
      JSGraph* jsgraph, CallFrequency invocation_frequency,
      SourcePositionTable* source_positions,
      int inlining_id = SourcePosition::kNotInlined,
      JSTypeHintLowering::Flags flags = JSTypeHintLowering::kNoFlags);

  // Creates a graph by visiting bytecodes.
  bool CreateGraph(bool stack_check = true);

 private:
  class Environment;
  struct SubEnvironment;

  void VisitBytecodes(bool stack_check);

  // Get or create the node that represents the outer function closure.
  Node* GetFunctionClosure();

  // Get or create the node that represents the outer function context.
  Node* GetFunctionContext();

  // Get or create the node that represents the incoming new target value.
  Node* GetNewTarget();

  // Builder for loading the a native context field.
  Node* BuildLoadNativeContextField(int index);

  // Helper function for creating a pair containing type feedback vector and
  // a feedback slot.
  VectorSlotPair CreateVectorSlotPair(int slot_id);

  void set_environment(Environment* env) { environment_ = env; }
  const Environment* environment() const { return environment_; }
  Environment* environment() { return environment_; }

  // Node creation helpers
  Node* NewNode(const Operator* op, bool incomplete = false) {
    return MakeNode(op, 0, static_cast<Node**>(nullptr), incomplete);
  }

  Node* NewNode(const Operator* op, Node* n1) {
    Node* buffer[] = {n1};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* buffer[] = {n1, n2};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* buffer[] = {n1, n2, n3};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* buffer[] = {n1, n2, n3, n4};
    return MakeNode(op, arraysize(buffer), buffer, false);
  }

  // Helpers to create new control nodes.
  Node* NewIfTrue() { return NewNode(common()->IfTrue()); }
  Node* NewIfFalse() { return NewNode(common()->IfFalse()); }
  Node* NewIfValue(int32_t value) { return NewNode(common()->IfValue(value)); }
  Node* NewIfDefault() { return NewNode(common()->IfDefault()); }
  Node* NewMerge() { return NewNode(common()->Merge(1), true); }
  Node* NewLoop() { return NewNode(common()->Loop(1), true); }
  Node* NewBranch(Node* condition, BranchHint hint = BranchHint::kNone) {
    return NewNode(common()->Branch(hint), condition);
  }
  Node* NewSwitch(Node* condition, int control_output_count) {
    return NewNode(common()->Switch(control_output_count), condition);
  }

  // Creates a new Phi node having {count} input values.
  Node* NewPhi(int count, Node* input, Node* control);
  Node* NewEffectPhi(int count, Node* input, Node* control);

  // Helpers for merging control, effect or value dependencies.
  Node* MergeControl(Node* control, Node* other);
  Node* MergeEffect(Node* effect, Node* other_effect, Node* control);
  Node* MergeValue(Node* value, Node* other_value, Node* control);

  // The main node creation chokepoint. Adds context, frame state, effect,
  // and control dependencies depending on the operator.
  Node* MakeNode(const Operator* op, int value_input_count,
                 Node* const* value_inputs, bool incomplete);

  Node** EnsureInputBufferSize(int size);

  Node* const* GetCallArgumentsFromRegister(Node* callee, Node* receiver,
                                            interpreter::Register first_arg,
                                            int arg_count);
  Node* ProcessCallArguments(const Operator* call_op, Node* const* args,
                             int arg_count);
  Node* ProcessCallArguments(const Operator* call_op, Node* callee,
                             interpreter::Register receiver, size_t reg_count);
  Node* ProcessConstructArguments(const Operator* call_new_op, Node* callee,
                                  Node* new_target,
                                  interpreter::Register receiver,
                                  size_t reg_count);
  Node* ProcessConstructWithSpreadArguments(const Operator* op, Node* callee,
                                            Node* new_target,
                                            interpreter::Register receiver,
                                            size_t reg_count);
  Node* ProcessCallRuntimeArguments(const Operator* call_runtime_op,
                                    interpreter::Register receiver,
                                    size_t reg_count);

  // Prepare information for eager deoptimization. This information is carried
  // by dedicated {Checkpoint} nodes that are wired into the effect chain.
  // Conceptually this frame state is "before" a given operation.
  void PrepareEagerCheckpoint();

  // Prepare information for lazy deoptimization. This information is attached
  // to the given node and the output value produced by the node is combined.
  // Conceptually this frame state is "after" a given operation.
  void PrepareFrameState(Node* node, OutputFrameStateCombine combine);

  void BuildCreateArguments(CreateArgumentsType type);
  Node* BuildLoadGlobal(Handle<Name> name, uint32_t feedback_slot_index,
                        TypeofMode typeof_mode);
  void BuildStoreGlobal(LanguageMode language_mode);

  enum class StoreMode {
    // Check the prototype chain before storing.
    kNormal,
    // Store value to the receiver without checking the prototype chain.
    kOwn,
  };
  void BuildNamedStore(LanguageMode language_mode, StoreMode store_mode);
  void BuildKeyedStore(LanguageMode language_mode);
  void BuildLdaLookupSlot(TypeofMode typeof_mode);
  void BuildLdaLookupContextSlot(TypeofMode typeof_mode);
  void BuildLdaLookupGlobalSlot(TypeofMode typeof_mode);
  void BuildStaLookupSlot(LanguageMode language_mode);
  void BuildCallVarArgs(TailCallMode tail_call_mode,
                        ConvertReceiverMode receiver_mode);
  void BuildCall(TailCallMode tail_call_mode, ConvertReceiverMode receiver_mode,
                 Node* const* args, size_t arg_count, int slot_id);
  void BuildCall(TailCallMode tail_call_mode, ConvertReceiverMode receiver_mode,
                 std::initializer_list<Node*> args, int slot_id) {
    BuildCall(tail_call_mode, receiver_mode, args.begin(), args.size(),
              slot_id);
  }
  void BuildBinaryOp(const Operator* op);
  void BuildBinaryOpWithImmediate(const Operator* op);
  void BuildCompareOp(const Operator* op);
  void BuildTestingOp(const Operator* op);
  void BuildDelete(LanguageMode language_mode);
  void BuildCastOperator(const Operator* op);

  // Optional early lowering to the simplified operator level. Returns the node
  // representing the lowered operation or {nullptr} if no lowering available.
  // Note that the result has already been wired into the environment just like
  // any other invocation of {NewNode} would do.
  Node* TryBuildSimplifiedBinaryOp(const Operator* op, Node* left, Node* right,
                                   FeedbackSlot slot);
  Node* TryBuildSimplifiedToNumber(Node* input, FeedbackSlot slot);
  Node* TryBuildSimplifiedLoadNamed(const Operator* op, Node* receiver,
                                    FeedbackSlot slot);
  Node* TryBuildSimplifiedLoadKeyed(const Operator* op, Node* receiver,
                                    Node* key, FeedbackSlot slot);
  Node* TryBuildSimplifiedStoreNamed(const Operator* op, Node* receiver,
                                     Node* value, FeedbackSlot slot);
  Node* TryBuildSimplifiedStoreKeyed(const Operator* op, Node* receiver,
                                     Node* key, Node* value, FeedbackSlot slot);

  // Applies the given early reduction onto the current environment.
  void ApplyEarlyReduction(Reduction reduction);

  // Check the context chain for extensions, for lookup fast paths.
  Environment* CheckContextExtensions(uint32_t depth);

  // Helper function to create binary operation hint from the recorded
  // type feedback.
  BinaryOperationHint GetBinaryOperationHint(int operand_index);

  // Helper function to create compare operation hint from the recorded
  // type feedback.
  CompareOperationHint GetCompareOperationHint();

  // Helper function to compute call frequency from the recorded type
  // feedback.
  CallFrequency ComputeCallFrequency(int slot_id) const;

  // Control flow plumbing.
  void BuildJump();
  void BuildJumpIf(Node* condition);
  void BuildJumpIfNot(Node* condition);
  void BuildJumpIfEqual(Node* comperand);
  void BuildJumpIfNotEqual(Node* comperand);
  void BuildJumpIfTrue();
  void BuildJumpIfFalse();
  void BuildJumpIfToBooleanTrue();
  void BuildJumpIfToBooleanFalse();
  void BuildJumpIfNotHole();
  void BuildJumpIfJSReceiver();

  void BuildSwitchOnSmi(Node* condition);

  // Simulates control flow by forward-propagating environments.
  void MergeIntoSuccessorEnvironment(int target_offset);
  void BuildLoopHeaderEnvironment(int current_offset);
  void SwitchToMergeEnvironment(int current_offset);

  // Simulates control flow that exits the function body.
  void MergeControlToLeaveFunction(Node* exit);

  // Builds entry points that are used by OSR deconstruction.
  void BuildOSRLoopEntryPoint(int current_offset);
  void BuildOSRNormalEntryPoint();

  // Builds loop exit nodes for every exited loop between the current bytecode
  // offset and {target_offset}.
  void BuildLoopExitsForBranch(int target_offset);
  void BuildLoopExitsForFunctionExit();
  void BuildLoopExitsUntilLoop(int loop_offset);

  // Simulates entry and exit of exception handlers.
  void EnterAndExitExceptionHandlers(int current_offset);

  // Update the current position of the {SourcePositionTable} to that of the
  // bytecode at {offset}, if any.
  void UpdateCurrentSourcePosition(SourcePositionTableIterator* it, int offset);

  // Growth increment for the temporary buffer used to construct input lists to
  // new nodes.
  static const int kInputBufferSizeIncrement = 64;

  // An abstract representation for an exception handler that is being
  // entered and exited while the graph builder is iterating over the
  // underlying bytecode. The exception handlers within the bytecode are
  // well scoped, hence will form a stack during iteration.
  struct ExceptionHandler {
    int start_offset_;      // Start offset of the handled area in the bytecode.
    int end_offset_;        // End offset of the handled area in the bytecode.
    int handler_offset_;    // Handler entry offset within the bytecode.
    int context_register_;  // Index of register holding handler context.
  };

  // Field accessors
  Graph* graph() const { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  Zone* graph_zone() const { return graph()->zone(); }
  JSGraph* jsgraph() const { return jsgraph_; }
  JSOperatorBuilder* javascript() const { return jsgraph_->javascript(); }
  SimplifiedOperatorBuilder* simplified() const {
    return jsgraph_->simplified();
  }
  Zone* local_zone() const { return local_zone_; }
  const Handle<BytecodeArray>& bytecode_array() const {
    return bytecode_array_;
  }
  const Handle<HandlerTable>& exception_handler_table() const {
    return exception_handler_table_;
  }
  const Handle<FeedbackVector>& feedback_vector() const {
    return feedback_vector_;
  }
  const JSTypeHintLowering& type_hint_lowering() const {
    return type_hint_lowering_;
  }
  const FrameStateFunctionInfo* frame_state_function_info() const {
    return frame_state_function_info_;
  }

  const interpreter::BytecodeArrayIterator& bytecode_iterator() const {
    return *bytecode_iterator_;
  }

  void set_bytecode_iterator(
      const interpreter::BytecodeArrayIterator* bytecode_iterator) {
    bytecode_iterator_ = bytecode_iterator;
  }

  const BytecodeAnalysis* bytecode_analysis() const {
    return bytecode_analysis_;
  }

  void set_bytecode_analysis(const BytecodeAnalysis* bytecode_analysis) {
    bytecode_analysis_ = bytecode_analysis;
  }

  bool needs_eager_checkpoint() const { return needs_eager_checkpoint_; }
  void mark_as_needing_eager_checkpoint(bool value) {
    needs_eager_checkpoint_ = value;
  }

#define DECLARE_VISIT_BYTECODE(name, ...) void Visit##name();
  BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  Zone* local_zone_;
  JSGraph* jsgraph_;
  CallFrequency const invocation_frequency_;
  Handle<BytecodeArray> bytecode_array_;
  Handle<HandlerTable> exception_handler_table_;
  Handle<FeedbackVector> feedback_vector_;
  const JSTypeHintLowering type_hint_lowering_;
  const FrameStateFunctionInfo* frame_state_function_info_;
  const interpreter::BytecodeArrayIterator* bytecode_iterator_;
  const BytecodeAnalysis* bytecode_analysis_;
  Environment* environment_;
  BailoutId osr_ast_id_;

  // Merge environments are snapshots of the environment at points where the
  // control flow merges. This models a forward data flow propagation of all
  // values from all predecessors of the merge in question.
  ZoneMap<int, Environment*> merge_environments_;

  // Exception handlers currently entered by the iteration.
  ZoneStack<ExceptionHandler> exception_handlers_;
  int current_exception_handler_;

  // Temporary storage for building node input lists.
  int input_buffer_size_;
  Node** input_buffer_;

  // Optimization to only create checkpoints when the current position in the
  // control-flow is not effect-dominated by another checkpoint already. All
  // operations that do not have observable side-effects can be re-evaluated.
  bool needs_eager_checkpoint_;

  // Nodes representing values in the activation record.
  SetOncePointer<Node> function_context_;
  SetOncePointer<Node> function_closure_;
  SetOncePointer<Node> new_target_;

  // Control nodes that exit the function body.
  ZoneVector<Node*> exit_controls_;

  StateValuesCache state_values_cache_;

  // The source position table, to be populated.
  SourcePositionTable* source_positions_;

  SourcePosition const start_position_;

  static int const kBinaryOperationHintIndex = 1;
  static int const kCountOperationHintIndex = 0;
  static int const kBinaryOperationSmiHintIndex = 1;

  DISALLOW_COPY_AND_ASSIGN(BytecodeGraphBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
