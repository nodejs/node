// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
#define V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_

#include "src/compiler.h"
#include "src/compiler/bytecode-branch-analysis.h"
#include "src/compiler/js-graph.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace compiler {

// The BytecodeGraphBuilder produces a high-level IR graph based on
// interpreter bytecodes.
class BytecodeGraphBuilder {
 public:
  BytecodeGraphBuilder(Zone* local_zone, CompilationInfo* info,
                       JSGraph* jsgraph);

  // Creates a graph by visiting bytecodes.
  bool CreateGraph(bool stack_check = true);

  Graph* graph() const { return jsgraph_->graph(); }

 private:
  class Environment;
  class FrameStateBeforeAndAfter;

  void CreateGraphBody(bool stack_check);
  void VisitBytecodes();

  Node* LoadAccumulator(Node* value);

  // Get or create the node that represents the outer function closure.
  Node* GetFunctionClosure();

  // Get or create the node that represents the outer function context.
  Node* GetFunctionContext();

  // Get or create the node that represents the incoming new target value.
  Node* GetNewTarget();

  // Builder for accessing a (potentially immutable) object field.
  Node* BuildLoadObjectField(Node* object, int offset);
  Node* BuildLoadImmutableObjectField(Node* object, int offset);

  // Builder for accessing type feedback vector.
  Node* BuildLoadFeedbackVector();

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
  Node* NewMerge() { return NewNode(common()->Merge(1), true); }
  Node* NewLoop() { return NewNode(common()->Loop(1), true); }
  Node* NewBranch(Node* condition, BranchHint hint = BranchHint::kNone) {
    return NewNode(common()->Branch(hint), condition);
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
  Node* MakeNode(const Operator* op, int value_input_count, Node** value_inputs,
                 bool incomplete);

  // Helper to indicate a node exits the function body.
  void UpdateControlDependencyToLeaveFunction(Node* exit);

  Node** EnsureInputBufferSize(int size);

  Node* ProcessCallArguments(const Operator* call_op, Node* callee,
                             interpreter::Register receiver, size_t arity);
  Node* ProcessCallNewArguments(const Operator* call_new_op,
                                interpreter::Register callee,
                                interpreter::Register first_arg, size_t arity);
  Node* ProcessCallRuntimeArguments(const Operator* call_runtime_op,
                                    interpreter::Register first_arg,
                                    size_t arity);

  void BuildCreateLiteral(const Operator* op,
                          const interpreter::BytecodeArrayIterator& iterator);
  void BuildCreateRegExpLiteral(
      const interpreter::BytecodeArrayIterator& iterator);
  void BuildCreateArrayLiteral(
      const interpreter::BytecodeArrayIterator& iterator);
  void BuildCreateObjectLiteral(
      const interpreter::BytecodeArrayIterator& iterator);
  void BuildCreateArguments(CreateArgumentsParameters::Type type,
                            const interpreter::BytecodeArrayIterator& iterator);
  void BuildLoadGlobal(const interpreter::BytecodeArrayIterator& iterator,
                       TypeofMode typeof_mode);
  void BuildStoreGlobal(const interpreter::BytecodeArrayIterator& iterator);
  void BuildNamedLoad(const interpreter::BytecodeArrayIterator& iterator);
  void BuildKeyedLoad(const interpreter::BytecodeArrayIterator& iterator);
  void BuildNamedStore(const interpreter::BytecodeArrayIterator& iterator);
  void BuildKeyedStore(const interpreter::BytecodeArrayIterator& iterator);
  void BuildLdaLookupSlot(TypeofMode typeof_mode,
                          const interpreter::BytecodeArrayIterator& iterator);
  void BuildStaLookupSlot(LanguageMode language_mode,
                          const interpreter::BytecodeArrayIterator& iterator);
  void BuildCall(const interpreter::BytecodeArrayIterator& iterator);
  void BuildBinaryOp(const Operator* op,
                     const interpreter::BytecodeArrayIterator& iterator);
  void BuildCompareOp(const Operator* op,
                      const interpreter::BytecodeArrayIterator& iterator);
  void BuildDelete(const interpreter::BytecodeArrayIterator& iterator);
  void BuildCastOperator(const Operator* js_op,
                         const interpreter::BytecodeArrayIterator& iterator);

  // Control flow plumbing.
  void BuildJump(int source_offset, int target_offset);
  void BuildJump();
  void BuildConditionalJump(Node* condition);
  void BuildJumpIfEqual(Node* comperand);
  void BuildJumpIfToBooleanEqual(Node* boolean_comperand);

  // Constructing merge and loop headers.
  void MergeEnvironmentsOfBackwardBranches(int source_offset,
                                           int target_offset);
  void MergeEnvironmentsOfForwardBranches(int source_offset);
  void BuildLoopHeaderForBackwardBranches(int source_offset);

  // Attaches a frame state to |node| for the entry to the function.
  void PrepareEntryFrameState(Node* node);

  // Growth increment for the temporary buffer used to construct input lists to
  // new nodes.
  static const int kInputBufferSizeIncrement = 64;

  // Field accessors
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  Zone* graph_zone() const { return graph()->zone(); }
  CompilationInfo* info() const { return info_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  JSOperatorBuilder* javascript() const { return jsgraph_->javascript(); }
  Zone* local_zone() const { return local_zone_; }
  const Handle<BytecodeArray>& bytecode_array() const {
    return bytecode_array_;
  }
  const FrameStateFunctionInfo* frame_state_function_info() const {
    return frame_state_function_info_;
  }

  LanguageMode language_mode() const {
    // TODO(mythria): Don't rely on parse information to get language mode.
    return info()->language_mode();
  }

  const interpreter::BytecodeArrayIterator* bytecode_iterator() const {
    return bytecode_iterator_;
  }

  void set_bytecode_iterator(
      const interpreter::BytecodeArrayIterator* bytecode_iterator) {
    bytecode_iterator_ = bytecode_iterator;
  }

  const BytecodeBranchAnalysis* branch_analysis() const {
    return branch_analysis_;
  }

  void set_branch_analysis(const BytecodeBranchAnalysis* branch_analysis) {
    branch_analysis_ = branch_analysis;
  }

#define DECLARE_VISIT_BYTECODE(name, ...) \
  void Visit##name(const interpreter::BytecodeArrayIterator& iterator);
  BYTECODE_LIST(DECLARE_VISIT_BYTECODE)
#undef DECLARE_VISIT_BYTECODE

  Zone* local_zone_;
  CompilationInfo* info_;
  JSGraph* jsgraph_;
  Handle<BytecodeArray> bytecode_array_;
  const FrameStateFunctionInfo* frame_state_function_info_;
  const interpreter::BytecodeArrayIterator* bytecode_iterator_;
  const BytecodeBranchAnalysis* branch_analysis_;
  Environment* environment_;


  // Merge environments are snapshots of the environment at a particular
  // bytecode offset to be merged into a later environment.
  ZoneMap<int, Environment*> merge_environments_;

  // Loop header environments are environments created for bytecodes
  // where it is known there are back branches, ie a loop header.
  ZoneMap<int, Environment*> loop_header_environments_;

  // Temporary storage for building node input lists.
  int input_buffer_size_;
  Node** input_buffer_;

  // Nodes representing values in the activation record.
  SetOncePointer<Node> function_context_;
  SetOncePointer<Node> function_closure_;
  SetOncePointer<Node> new_target_;

  // Optimization to cache loaded feedback vector.
  SetOncePointer<Node> feedback_vector_;

  // Control nodes that exit the function body.
  ZoneVector<Node*> exit_controls_;

  DISALLOW_COPY_AND_ASSIGN(BytecodeGraphBuilder);
};


class BytecodeGraphBuilder::Environment : public ZoneObject {
 public:
  Environment(BytecodeGraphBuilder* builder, int register_count,
              int parameter_count, Node* control_dependency, Node* context);

  int parameter_count() const { return parameter_count_; }
  int register_count() const { return register_count_; }

  Node* LookupAccumulator() const;
  Node* LookupRegister(interpreter::Register the_register) const;

  void ExchangeRegisters(interpreter::Register reg0,
                         interpreter::Register reg1);

  void BindAccumulator(Node* node, FrameStateBeforeAndAfter* states = nullptr);
  void BindRegister(interpreter::Register the_register, Node* node,
                    FrameStateBeforeAndAfter* states = nullptr);
  void BindRegistersToProjections(interpreter::Register first_reg, Node* node,
                                  FrameStateBeforeAndAfter* states = nullptr);
  void RecordAfterState(Node* node, FrameStateBeforeAndAfter* states);

  bool IsMarkedAsUnreachable() const;
  void MarkAsUnreachable();

  // Effect dependency tracked by this environment.
  Node* GetEffectDependency() { return effect_dependency_; }
  void UpdateEffectDependency(Node* dependency) {
    effect_dependency_ = dependency;
  }

  // Preserve a checkpoint of the environment for the IR graph. Any
  // further mutation of the environment will not affect checkpoints.
  Node* Checkpoint(BailoutId bytecode_offset, OutputFrameStateCombine combine);

  // Returns true if the state values are up to date with the current
  // environment.
  bool StateValuesAreUpToDate(int output_poke_offset, int output_poke_count);

  // Control dependency tracked by this environment.
  Node* GetControlDependency() const { return control_dependency_; }
  void UpdateControlDependency(Node* dependency) {
    control_dependency_ = dependency;
  }

  Node* Context() const { return context_; }
  void SetContext(Node* new_context) { context_ = new_context; }

  Environment* CopyForConditional() const;
  Environment* CopyForLoop();
  void Merge(Environment* other);

 private:
  explicit Environment(const Environment* copy);
  void PrepareForLoop();
  bool StateValuesAreUpToDate(Node** state_values, int offset, int count,
                              int output_poke_start, int output_poke_end);
  bool StateValuesRequireUpdate(Node** state_values, int offset, int count);
  void UpdateStateValues(Node** state_values, int offset, int count);

  int RegisterToValuesIndex(interpreter::Register the_register) const;

  Zone* zone() const { return builder_->local_zone(); }
  Graph* graph() const { return builder_->graph(); }
  CommonOperatorBuilder* common() const { return builder_->common(); }
  BytecodeGraphBuilder* builder() const { return builder_; }
  const NodeVector* values() const { return &values_; }
  NodeVector* values() { return &values_; }
  int register_base() const { return register_base_; }
  int accumulator_base() const { return accumulator_base_; }

  BytecodeGraphBuilder* builder_;
  int register_count_;
  int parameter_count_;
  Node* context_;
  Node* control_dependency_;
  Node* effect_dependency_;
  NodeVector values_;
  Node* parameters_state_values_;
  Node* registers_state_values_;
  Node* accumulator_state_values_;
  int register_base_;
  int accumulator_base_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
