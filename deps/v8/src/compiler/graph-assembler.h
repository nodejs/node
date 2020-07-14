// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_ASSEMBLER_H_
#define V8_COMPILER_GRAPH_ASSEMBLER_H_

#include "src/codegen/tnode.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

class JSGraph;
class Graph;
class Oddball;

// TODO(jgruber): Currently this is too permissive, but at least it lets us
// document which functions expect JS booleans. If a real Boolean type becomes
// possible in the future, use that instead.
using Boolean = Oddball;

namespace compiler {

class Schedule;
class BasicBlock;

#define PURE_ASSEMBLER_MACH_UNOP_LIST(V) \
  V(BitcastFloat32ToInt32)               \
  V(BitcastFloat64ToInt64)               \
  V(BitcastInt32ToFloat32)               \
  V(BitcastWord32ToWord64)               \
  V(BitcastInt64ToFloat64)               \
  V(ChangeFloat64ToInt32)                \
  V(ChangeFloat64ToInt64)                \
  V(ChangeFloat64ToUint32)               \
  V(ChangeInt32ToFloat64)                \
  V(ChangeInt32ToInt64)                  \
  V(ChangeInt64ToFloat64)                \
  V(ChangeUint32ToFloat64)               \
  V(ChangeUint32ToUint64)                \
  V(Float64Abs)                          \
  V(Float64ExtractHighWord32)            \
  V(Float64ExtractLowWord32)             \
  V(Float64SilenceNaN)                   \
  V(RoundFloat64ToInt32)                 \
  V(TruncateFloat64ToInt64)              \
  V(TruncateFloat64ToWord32)             \
  V(TruncateInt64ToInt32)                \
  V(Word32ReverseBytes)                  \
  V(Word64ReverseBytes)

#define PURE_ASSEMBLER_MACH_BINOP_LIST(V) \
  V(Float64Add)                           \
  V(Float64Div)                           \
  V(Float64Equal)                         \
  V(Float64InsertHighWord32)              \
  V(Float64InsertLowWord32)               \
  V(Float64LessThan)                      \
  V(Float64LessThanOrEqual)               \
  V(Float64Mod)                           \
  V(Float64Sub)                           \
  V(Int32Add)                             \
  V(Int32LessThan)                        \
  V(Int32LessThanOrEqual)                 \
  V(Int32Mul)                             \
  V(Int32Sub)                             \
  V(Int64Sub)                             \
  V(IntAdd)                               \
  V(IntLessThan)                          \
  V(IntMul)                               \
  V(IntSub)                               \
  V(Uint32LessThan)                       \
  V(Uint32LessThanOrEqual)                \
  V(Uint64LessThan)                       \
  V(Uint64LessThanOrEqual)                \
  V(UintLessThan)                         \
  V(Word32And)                            \
  V(Word32Equal)                          \
  V(Word32Or)                             \
  V(Word32Sar)                            \
  V(Word32SarShiftOutZeros)               \
  V(Word32Shl)                            \
  V(Word32Shr)                            \
  V(Word32Xor)                            \
  V(Word64And)                            \
  V(Word64Equal)                          \
  V(Word64Or)                             \
  V(WordAnd)                              \
  V(WordEqual)                            \
  V(WordSar)                              \
  V(WordSarShiftOutZeros)                 \
  V(WordShl)                              \
  V(WordXor)

#define CHECKED_ASSEMBLER_MACH_BINOP_LIST(V) \
  V(Int32AddWithOverflow)                    \
  V(Int32Div)                                \
  V(Int32Mod)                                \
  V(Int32MulWithOverflow)                    \
  V(Int32SubWithOverflow)                    \
  V(Uint32Div)                               \
  V(Uint32Mod)

#define JSGRAPH_SINGLETON_CONSTANT_LIST(V)      \
  V(AllocateInOldGenerationStub, Code)          \
  V(AllocateInYoungGenerationStub, Code)        \
  V(AllocateRegularInOldGenerationStub, Code)   \
  V(AllocateRegularInYoungGenerationStub, Code) \
  V(BigIntMap, Map)                             \
  V(BooleanMap, Map)                            \
  V(EmptyString, String)                        \
  V(False, Boolean)                             \
  V(FixedArrayMap, Map)                         \
  V(FixedDoubleArrayMap, Map)                   \
  V(HeapNumberMap, Map)                         \
  V(MinusOne, Number)                           \
  V(NaN, Number)                                \
  V(NoContext, Object)                          \
  V(Null, Oddball)                              \
  V(One, Number)                                \
  V(TheHole, Oddball)                           \
  V(ToNumberBuiltin, Code)                      \
  V(True, Boolean)                              \
  V(Undefined, Oddball)                         \
  V(Zero, Number)

class GraphAssembler;

// Wrapper classes for special node/edge types (effect, control, frame states)
// that otherwise don't fit into the type system.

class NodeWrapper {
 public:
  explicit constexpr NodeWrapper(Node* node) : node_(node) {}
  operator Node*() const { return node_; }
  Node* operator->() const { return node_; }

 private:
  Node* node_;
};

class Effect : public NodeWrapper {
 public:
  explicit constexpr Effect(Node* node) : NodeWrapper(node) {
    // TODO(jgruber): Remove the End special case.
    SLOW_DCHECK(node == nullptr || node->op()->opcode() == IrOpcode::kEnd ||
                node->op()->EffectOutputCount() > 0);
  }
};

class Control : public NodeWrapper {
 public:
  explicit constexpr Control(Node* node) : NodeWrapper(node) {
    // TODO(jgruber): Remove the End special case.
    SLOW_DCHECK(node == nullptr || node->opcode() == IrOpcode::kEnd ||
                node->op()->ControlOutputCount() > 0);
  }
};

class FrameState : public NodeWrapper {
 public:
  explicit constexpr FrameState(Node* node) : NodeWrapper(node) {
    // TODO(jgruber): Disallow kStart (needed for PromiseConstructorBasic unit
    // test, among others).
    SLOW_DCHECK(node->opcode() == IrOpcode::kFrameState ||
                node->opcode() == IrOpcode::kStart);
  }
};

enum class GraphAssemblerLabelType { kDeferred, kNonDeferred, kLoop };

// Label with statically known count of incoming branches and phis.
template <size_t VarCount>
class GraphAssemblerLabel {
 public:
  Node* PhiAt(size_t index);

  template <typename T>
  TNode<T> PhiAt(size_t index) {
    // TODO(jgruber): Investigate issues on ptr compression bots and enable.
    // DCHECK(IsMachineRepresentationOf<T>(representations_[index]));
    return TNode<T>::UncheckedCast(PhiAt(index));
  }

  GraphAssemblerLabel(GraphAssemblerLabelType type, BasicBlock* basic_block,
                      int loop_nesting_level,
                      const std::array<MachineRepresentation, VarCount>& reps)
      : type_(type),
        basic_block_(basic_block),
        loop_nesting_level_(loop_nesting_level),
        representations_(reps) {}

  ~GraphAssemblerLabel() { DCHECK(IsBound() || merged_count_ == 0); }

 private:
  friend class GraphAssembler;

  void SetBound() {
    DCHECK(!IsBound());
    is_bound_ = true;
  }
  bool IsBound() const { return is_bound_; }
  bool IsDeferred() const {
    return type_ == GraphAssemblerLabelType::kDeferred;
  }
  bool IsLoop() const { return type_ == GraphAssemblerLabelType::kLoop; }
  BasicBlock* basic_block() { return basic_block_; }

  bool is_bound_ = false;
  const GraphAssemblerLabelType type_;
  BasicBlock* const basic_block_;
  const int loop_nesting_level_;
  size_t merged_count_ = 0;
  Node* effect_;
  Node* control_;
  std::array<Node*, VarCount> bindings_;
  const std::array<MachineRepresentation, VarCount> representations_;
};

class V8_EXPORT_PRIVATE GraphAssembler {
 public:
  // Constructs a GraphAssembler. If {schedule} is not null, the graph assembler
  // will maintain the schedule as it updates blocks.
  GraphAssembler(MachineGraph* jsgraph, Zone* zone,
                 Schedule* schedule = nullptr, bool mark_loop_exits = false);
  virtual ~GraphAssembler();

  void Reset(BasicBlock* block);
  void InitializeEffectControl(Node* effect, Node* control);

  // Create label.
  template <typename... Reps>
  GraphAssemblerLabel<sizeof...(Reps)> MakeLabelFor(
      GraphAssemblerLabelType type, Reps... reps) {
    std::array<MachineRepresentation, sizeof...(Reps)> reps_array = {reps...};
    return MakeLabel<sizeof...(Reps)>(reps_array, type);
  }

  // As above, but with an std::array of machine representations.
  template <int VarCount>
  GraphAssemblerLabel<VarCount> MakeLabel(
      std::array<MachineRepresentation, VarCount> reps_array,
      GraphAssemblerLabelType type) {
    return GraphAssemblerLabel<VarCount>(
        type, NewBasicBlock(type == GraphAssemblerLabelType::kDeferred),
        loop_nesting_level_, reps_array);
  }

  // Convenience wrapper for creating non-deferred labels.
  template <typename... Reps>
  GraphAssemblerLabel<sizeof...(Reps)> MakeLabel(Reps... reps) {
    return MakeLabelFor(GraphAssemblerLabelType::kNonDeferred, reps...);
  }

  // Convenience wrapper for creating loop labels.
  template <typename... Reps>
  GraphAssemblerLabel<sizeof...(Reps)> MakeLoopLabel(Reps... reps) {
    return MakeLabelFor(GraphAssemblerLabelType::kLoop, reps...);
  }

  // Convenience wrapper for creating deferred labels.
  template <typename... Reps>
  GraphAssemblerLabel<sizeof...(Reps)> MakeDeferredLabel(Reps... reps) {
    return MakeLabelFor(GraphAssemblerLabelType::kDeferred, reps...);
  }

  // Value creation.
  Node* IntPtrConstant(intptr_t value);
  Node* Uint32Constant(uint32_t value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* UniqueIntPtrConstant(intptr_t value);
  Node* Float64Constant(double value);
  Node* Projection(int index, Node* value);
  Node* ExternalConstant(ExternalReference ref);

  Node* LoadFramePointer();

  Node* LoadHeapNumberValue(Node* heap_number);

#define PURE_UNOP_DECL(Name) Node* Name(Node* input);
  PURE_ASSEMBLER_MACH_UNOP_LIST(PURE_UNOP_DECL)
#undef PURE_UNOP_DECL

#define BINOP_DECL(Name) Node* Name(Node* left, Node* right);
  PURE_ASSEMBLER_MACH_BINOP_LIST(BINOP_DECL)
  CHECKED_ASSEMBLER_MACH_BINOP_LIST(BINOP_DECL)
#undef BINOP_DECL

  // Debugging
  Node* DebugBreak();

  Node* Unreachable();

  Node* IntPtrEqual(Node* left, Node* right);
  Node* TaggedEqual(Node* left, Node* right);

  Node* SmiSub(Node* left, Node* right);
  Node* SmiLessThan(Node* left, Node* right);

  Node* Float64RoundDown(Node* value);
  Node* Float64RoundTruncate(Node* value);

  Node* BitcastWordToTagged(Node* value);
  Node* BitcastWordToTaggedSigned(Node* value);
  Node* BitcastTaggedToWord(Node* value);
  Node* BitcastTaggedToWordForTagAndSmiBits(Node* value);

  Node* TypeGuard(Type type, Node* value);
  Node* Checkpoint(FrameState frame_state);

  Node* Store(StoreRepresentation rep, Node* object, Node* offset, Node* value);
  Node* Store(StoreRepresentation rep, Node* object, int offset, Node* value);
  Node* Load(MachineType type, Node* object, Node* offset);
  Node* Load(MachineType type, Node* object, int offset);

  Node* StoreUnaligned(MachineRepresentation rep, Node* object, Node* offset,
                       Node* value);
  Node* LoadUnaligned(MachineType type, Node* object, Node* offset);

  Node* Retain(Node* buffer);
  Node* UnsafePointerAdd(Node* base, Node* external);

  Node* Word32PoisonOnSpeculation(Node* value);

  Node* DeoptimizeIf(
      DeoptimizeReason reason, FeedbackSource const& feedback, Node* condition,
      Node* frame_state,
      IsSafetyCheck is_safety_check = IsSafetyCheck::kSafetyCheck);
  Node* DeoptimizeIfNot(
      DeoptimizeReason reason, FeedbackSource const& feedback, Node* condition,
      Node* frame_state,
      IsSafetyCheck is_safety_check = IsSafetyCheck::kSafetyCheck);
  template <typename... Args>
  Node* Call(const CallDescriptor* call_descriptor, Args... args);
  template <typename... Args>
  Node* Call(const Operator* op, Args... args);

  // Basic control operations.
  template <size_t VarCount>
  void Bind(GraphAssemblerLabel<VarCount>* label);

  template <typename... Vars>
  void Goto(GraphAssemblerLabel<sizeof...(Vars)>* label, Vars...);

  // Branch hints are inferred from if_true/if_false deferred states.
  void BranchWithCriticalSafetyCheck(Node* condition,
                                     GraphAssemblerLabel<0u>* if_true,
                                     GraphAssemblerLabel<0u>* if_false);

  // Branch hints are inferred from if_true/if_false deferred states.
  template <typename... Vars>
  void Branch(Node* condition, GraphAssemblerLabel<sizeof...(Vars)>* if_true,
              GraphAssemblerLabel<sizeof...(Vars)>* if_false, Vars...);

  template <typename... Vars>
  void BranchWithHint(Node* condition,
                      GraphAssemblerLabel<sizeof...(Vars)>* if_true,
                      GraphAssemblerLabel<sizeof...(Vars)>* if_false,
                      BranchHint hint, Vars...);

  // Control helpers.
  // {GotoIf(c, l)} is equivalent to {Branch(c, l, templ);Bind(templ)}.
  template <typename... Vars>
  void GotoIf(Node* condition, GraphAssemblerLabel<sizeof...(Vars)>* label,
              Vars...);

  // {GotoIfNot(c, l)} is equivalent to {Branch(c, templ, l);Bind(templ)}.
  template <typename... Vars>
  void GotoIfNot(Node* condition, GraphAssemblerLabel<sizeof...(Vars)>* label,
                 Vars...);

  // Updates current effect and control based on outputs of {node}.
  V8_INLINE void UpdateEffectControlWith(Node* node) {
    if (node->op()->EffectOutputCount() > 0) {
      effect_ = node;
    }
    if (node->op()->ControlOutputCount() > 0) {
      control_ = node;
    }
  }

  // Adds {node} to the current position and updates assembler's current effect
  // and control.
  Node* AddNode(Node* node);

  template <typename T>
  TNode<T> AddNode(Node* node) {
    return TNode<T>::UncheckedCast(AddNode(node));
  }

  // Finalizes the {block} being processed by the assembler, returning the
  // finalized block (which may be different from the original block).
  BasicBlock* FinalizeCurrentBlock(BasicBlock* block);

  void ConnectUnreachableToEnd();

  Control control() { return Control(control_); }
  Effect effect() { return Effect(effect_); }

 protected:
  class BasicBlockUpdater;

  template <typename... Vars>
  void MergeState(GraphAssemblerLabel<sizeof...(Vars)>* label, Vars... vars);
  BasicBlock* NewBasicBlock(bool deferred);
  void BindBasicBlock(BasicBlock* block);
  void GotoBasicBlock(BasicBlock* block);
  void GotoIfBasicBlock(BasicBlock* block, Node* branch,
                        IrOpcode::Value goto_if);

  V8_INLINE Node* AddClonedNode(Node* node);

  MachineGraph* mcgraph() const { return mcgraph_; }
  Graph* graph() const { return mcgraph_->graph(); }
  Zone* temp_zone() const { return temp_zone_; }
  CommonOperatorBuilder* common() const { return mcgraph()->common(); }
  MachineOperatorBuilder* machine() const { return mcgraph()->machine(); }

  // Updates machinery for creating {LoopExit,LoopExitEffect,LoopExitValue}
  // nodes on loop exits (which are necessary for loop peeling).
  //
  // All labels created while a LoopScope is live are considered to be inside
  // the loop.
  template <MachineRepresentation... Reps>
  class LoopScope final {
   private:
    // The internal scope is only here to increment the graph assembler's
    // nesting level prior to `loop_header_label` creation below.
    class LoopScopeInternal {
     public:
      explicit LoopScopeInternal(GraphAssembler* gasm)
          : previous_loop_nesting_level_(gasm->loop_nesting_level_),
            gasm_(gasm) {
        gasm->loop_nesting_level_++;
      }

      ~LoopScopeInternal() {
        gasm_->loop_nesting_level_--;
        DCHECK_EQ(gasm_->loop_nesting_level_, previous_loop_nesting_level_);
      }

     private:
      const int previous_loop_nesting_level_;
      GraphAssembler* const gasm_;
    };

   public:
    explicit LoopScope(GraphAssembler* gasm)
        : internal_scope_(gasm),
          gasm_(gasm),
          loop_header_label_(gasm->MakeLoopLabel(Reps...)) {
      // This feature may only be used if it has been enabled.
      DCHECK(gasm_->mark_loop_exits_);
      gasm->loop_headers_.push_back(&loop_header_label_.control_);
      DCHECK_EQ(static_cast<int>(gasm_->loop_headers_.size()),
                gasm_->loop_nesting_level_);
    }

    ~LoopScope() {
      DCHECK_EQ(static_cast<int>(gasm_->loop_headers_.size()),
                gasm_->loop_nesting_level_);
      gasm_->loop_headers_.pop_back();
    }

    GraphAssemblerLabel<sizeof...(Reps)>* loop_header_label() {
      return &loop_header_label_;
    }

   private:
    const LoopScopeInternal internal_scope_;
    GraphAssembler* const gasm_;
    GraphAssemblerLabel<sizeof...(Reps)> loop_header_label_;
  };

  // Upon destruction, restores effect and control to the state at construction.
  class RestoreEffectControlScope {
   public:
    explicit RestoreEffectControlScope(GraphAssembler* gasm)
        : gasm_(gasm), effect_(gasm->effect()), control_(gasm->control()) {}

    ~RestoreEffectControlScope() {
      gasm_->effect_ = effect_;
      gasm_->control_ = control_;
    }

   private:
    GraphAssembler* const gasm_;
    const Effect effect_;
    const Control control_;
  };

 private:
  template <typename... Vars>
  void BranchImpl(Node* condition,
                  GraphAssemblerLabel<sizeof...(Vars)>* if_true,
                  GraphAssemblerLabel<sizeof...(Vars)>* if_false,
                  BranchHint hint, IsSafetyCheck is_safety_check, Vars...);
  void RecordBranchInBlockUpdater(Node* branch, Node* if_true_control,
                                  Node* if_false_control,
                                  BasicBlock* if_true_block,
                                  BasicBlock* if_false_block);

  Zone* temp_zone_;
  MachineGraph* mcgraph_;
  Node* effect_;
  Node* control_;
  std::unique_ptr<BasicBlockUpdater> block_updater_;

  // Track loop information in order to properly mark loop exits with
  // {LoopExit,LoopExitEffect,LoopExitValue} nodes. The outermost level has
  // a nesting level of 0. See also GraphAssembler::LoopScope.
  int loop_nesting_level_ = 0;
  ZoneVector<Node**> loop_headers_;

  // Feature configuration. As more features are added, this should be turned
  // into a bitfield.
  const bool mark_loop_exits_;
};

template <size_t VarCount>
Node* GraphAssemblerLabel<VarCount>::PhiAt(size_t index) {
  DCHECK(IsBound());
  DCHECK_LT(index, VarCount);
  return bindings_[index];
}

template <typename... Vars>
void GraphAssembler::MergeState(GraphAssemblerLabel<sizeof...(Vars)>* label,
                                Vars... vars) {
  RestoreEffectControlScope restore_effect_control_scope(this);

  const int merged_count = static_cast<int>(label->merged_count_);
  static constexpr int kVarCount = sizeof...(vars);
  std::array<Node*, kVarCount> var_array = {vars...};

  const bool is_loop_exit = label->loop_nesting_level_ != loop_nesting_level_;
  if (is_loop_exit) {
    // This feature may only be used if it has been enabled.
    USE(mark_loop_exits_);
    DCHECK(mark_loop_exits_);
    // Jumping from loops to loops not supported.
    DCHECK(!label->IsLoop());
    // Currently only the simple case of jumping one level is supported.
    DCHECK_EQ(label->loop_nesting_level_, loop_nesting_level_ - 1);
    DCHECK(!loop_headers_.empty());
    DCHECK_NOT_NULL(*loop_headers_.back());

    // Mark this exit to enable loop peeling.
    AddNode(graph()->NewNode(common()->LoopExit(), control(),
                             *loop_headers_.back()));
    AddNode(graph()->NewNode(common()->LoopExitEffect(), effect(), control()));
    for (size_t i = 0; i < kVarCount; i++) {
      var_array[i] = AddNode(
          graph()->NewNode(common()->LoopExitValue(), var_array[i], control()));
    }
  }

  if (label->IsLoop()) {
    if (merged_count == 0) {
      DCHECK(!label->IsBound());
      label->control_ =
          graph()->NewNode(common()->Loop(2), control(), control());
      label->effect_ = graph()->NewNode(common()->EffectPhi(2), effect(),
                                        effect(), label->control_);
      Node* terminate = graph()->NewNode(common()->Terminate(), label->effect_,
                                         label->control_);
      NodeProperties::MergeControlToEnd(graph(), common(), terminate);
      for (size_t i = 0; i < kVarCount; i++) {
        label->bindings_[i] =
            graph()->NewNode(common()->Phi(label->representations_[i], 2),
                             var_array[i], var_array[i], label->control_);
      }
    } else {
      DCHECK(label->IsBound());
      DCHECK_EQ(1, merged_count);
      label->control_->ReplaceInput(1, control());
      label->effect_->ReplaceInput(1, effect());
      for (size_t i = 0; i < kVarCount; i++) {
        label->bindings_[i]->ReplaceInput(1, var_array[i]);
        CHECK(!NodeProperties::IsTyped(var_array[i]));  // Unsupported.
      }
    }
  } else {
    DCHECK(!label->IsLoop());
    DCHECK(!label->IsBound());
    if (merged_count == 0) {
      // Just set the control, effect and variables directly.
      label->control_ = control();
      label->effect_ = effect();
      for (size_t i = 0; i < kVarCount; i++) {
        label->bindings_[i] = var_array[i];
      }
    } else if (merged_count == 1) {
      // Create merge, effect phi and a phi for each variable.
      label->control_ =
          graph()->NewNode(common()->Merge(2), label->control_, control());
      label->effect_ = graph()->NewNode(common()->EffectPhi(2), label->effect_,
                                        effect(), label->control_);
      for (size_t i = 0; i < kVarCount; i++) {
        label->bindings_[i] = graph()->NewNode(
            common()->Phi(label->representations_[i], 2), label->bindings_[i],
            var_array[i], label->control_);
      }
    } else {
      // Append to the merge, effect phi and phis.
      DCHECK_EQ(IrOpcode::kMerge, label->control_->opcode());
      label->control_->AppendInput(graph()->zone(), control());
      NodeProperties::ChangeOp(label->control_,
                               common()->Merge(merged_count + 1));

      DCHECK_EQ(IrOpcode::kEffectPhi, label->effect_->opcode());
      label->effect_->ReplaceInput(merged_count, effect());
      label->effect_->AppendInput(graph()->zone(), label->control_);
      NodeProperties::ChangeOp(label->effect_,
                               common()->EffectPhi(merged_count + 1));

      for (size_t i = 0; i < kVarCount; i++) {
        DCHECK_EQ(IrOpcode::kPhi, label->bindings_[i]->opcode());
        label->bindings_[i]->ReplaceInput(merged_count, var_array[i]);
        label->bindings_[i]->AppendInput(graph()->zone(), label->control_);
        NodeProperties::ChangeOp(
            label->bindings_[i],
            common()->Phi(label->representations_[i], merged_count + 1));
        if (NodeProperties::IsTyped(label->bindings_[i])) {
          CHECK(NodeProperties::IsTyped(var_array[i]));
          Type old_type = NodeProperties::GetType(label->bindings_[i]);
          Type new_type = Type::Union(
              old_type, NodeProperties::GetType(var_array[i]), graph()->zone());
          NodeProperties::SetType(label->bindings_[i], new_type);
        }
      }
    }
  }
  label->merged_count_++;
}

template <size_t VarCount>
void GraphAssembler::Bind(GraphAssemblerLabel<VarCount>* label) {
  DCHECK_NULL(control());
  DCHECK_NULL(effect());
  DCHECK_LT(0, label->merged_count_);
  DCHECK_EQ(label->loop_nesting_level_, loop_nesting_level_);

  control_ = label->control_;
  effect_ = label->effect_;
  BindBasicBlock(label->basic_block());

  label->SetBound();

  if (label->merged_count_ > 1 || label->IsLoop()) {
    AddNode(label->control_);
    AddNode(label->effect_);
    for (size_t i = 0; i < VarCount; i++) {
      AddNode(label->bindings_[i]);
    }
  } else {
    // If the basic block does not have a control node, insert a dummy
    // Merge node, so that other passes have a control node to start from.
    control_ = AddNode(graph()->NewNode(common()->Merge(1), control()));
  }
}

template <typename... Vars>
void GraphAssembler::Branch(Node* condition,
                            GraphAssemblerLabel<sizeof...(Vars)>* if_true,
                            GraphAssemblerLabel<sizeof...(Vars)>* if_false,
                            Vars... vars) {
  BranchHint hint = BranchHint::kNone;
  if (if_true->IsDeferred() != if_false->IsDeferred()) {
    hint = if_false->IsDeferred() ? BranchHint::kTrue : BranchHint::kFalse;
  }

  BranchImpl(condition, if_true, if_false, hint, IsSafetyCheck::kNoSafetyCheck,
             vars...);
}

template <typename... Vars>
void GraphAssembler::BranchWithHint(
    Node* condition, GraphAssemblerLabel<sizeof...(Vars)>* if_true,
    GraphAssemblerLabel<sizeof...(Vars)>* if_false, BranchHint hint,
    Vars... vars) {
  BranchImpl(condition, if_true, if_false, hint, IsSafetyCheck::kNoSafetyCheck,
             vars...);
}

template <typename... Vars>
void GraphAssembler::BranchImpl(Node* condition,
                                GraphAssemblerLabel<sizeof...(Vars)>* if_true,
                                GraphAssemblerLabel<sizeof...(Vars)>* if_false,
                                BranchHint hint, IsSafetyCheck is_safety_check,
                                Vars... vars) {
  DCHECK_NOT_NULL(control());

  Node* branch = graph()->NewNode(common()->Branch(hint, is_safety_check),
                                  condition, control());

  Node* if_true_control = control_ =
      graph()->NewNode(common()->IfTrue(), branch);
  MergeState(if_true, vars...);

  Node* if_false_control = control_ =
      graph()->NewNode(common()->IfFalse(), branch);
  MergeState(if_false, vars...);

  if (block_updater_) {
    RecordBranchInBlockUpdater(branch, if_true_control, if_false_control,
                               if_true->basic_block(), if_false->basic_block());
  }

  control_ = nullptr;
  effect_ = nullptr;
}

template <typename... Vars>
void GraphAssembler::Goto(GraphAssemblerLabel<sizeof...(Vars)>* label,
                          Vars... vars) {
  DCHECK_NOT_NULL(control());
  DCHECK_NOT_NULL(effect());
  MergeState(label, vars...);
  GotoBasicBlock(label->basic_block());

  control_ = nullptr;
  effect_ = nullptr;
}

template <typename... Vars>
void GraphAssembler::GotoIf(Node* condition,
                            GraphAssemblerLabel<sizeof...(Vars)>* label,
                            Vars... vars) {
  BranchHint hint =
      label->IsDeferred() ? BranchHint::kFalse : BranchHint::kNone;
  Node* branch = graph()->NewNode(common()->Branch(hint), condition, control());

  control_ = graph()->NewNode(common()->IfTrue(), branch);
  MergeState(label, vars...);

  GotoIfBasicBlock(label->basic_block(), branch, IrOpcode::kIfTrue);
  control_ = AddNode(graph()->NewNode(common()->IfFalse(), branch));
}

template <typename... Vars>
void GraphAssembler::GotoIfNot(Node* condition,
                               GraphAssemblerLabel<sizeof...(Vars)>* label,
                               Vars... vars) {
  BranchHint hint = label->IsDeferred() ? BranchHint::kTrue : BranchHint::kNone;
  Node* branch = graph()->NewNode(common()->Branch(hint), condition, control());

  control_ = graph()->NewNode(common()->IfFalse(), branch);
  MergeState(label, vars...);

  GotoIfBasicBlock(label->basic_block(), branch, IrOpcode::kIfFalse);
  control_ = AddNode(graph()->NewNode(common()->IfTrue(), branch));
}

template <typename... Args>
Node* GraphAssembler::Call(const CallDescriptor* call_descriptor,
                           Args... args) {
  const Operator* op = common()->Call(call_descriptor);
  return Call(op, args...);
}

template <typename... Args>
Node* GraphAssembler::Call(const Operator* op, Args... args) {
  DCHECK_EQ(IrOpcode::kCall, op->opcode());
  Node* args_array[] = {args..., effect(), control()};
  int size = static_cast<int>(sizeof...(args)) + op->EffectInputCount() +
             op->ControlInputCount();
  return AddNode(graph()->NewNode(op, size, args_array));
}

class V8_EXPORT_PRIVATE JSGraphAssembler : public GraphAssembler {
 public:
  // Constructs a JSGraphAssembler. If {schedule} is not null, the graph
  // assembler will maintain the schedule as it updates blocks.
  JSGraphAssembler(JSGraph* jsgraph, Zone* zone, Schedule* schedule = nullptr,
                   bool mark_loop_exits = false)
      : GraphAssembler(jsgraph, zone, schedule, mark_loop_exits),
        jsgraph_(jsgraph) {}

  Node* SmiConstant(int32_t value);
  TNode<HeapObject> HeapConstant(Handle<HeapObject> object);
  TNode<Object> Constant(const ObjectRef& ref);
  TNode<Number> NumberConstant(double value);
  Node* CEntryStubConstant(int result_size);

#define SINGLETON_CONST_DECL(Name, Type) TNode<Type> Name##Constant();
  JSGRAPH_SINGLETON_CONSTANT_LIST(SINGLETON_CONST_DECL)
#undef SINGLETON_CONST_DECL

#define SINGLETON_CONST_TEST_DECL(Name, ...) \
  TNode<Boolean> Is##Name(TNode<Object> value);
  JSGRAPH_SINGLETON_CONSTANT_LIST(SINGLETON_CONST_TEST_DECL)
#undef SINGLETON_CONST_TEST_DECL

  Node* Allocate(AllocationType allocation, Node* size);
  Node* LoadField(FieldAccess const&, Node* object);
  template <typename T>
  TNode<T> LoadField(FieldAccess const& access, TNode<HeapObject> object) {
    // TODO(jgruber): Investigate issues on ptr compression bots and enable.
    // DCHECK(IsMachineRepresentationOf<T>(
    //     access.machine_type.representation()));
    return TNode<T>::UncheckedCast(LoadField(access, object));
  }
  Node* LoadElement(ElementAccess const&, Node* object, Node* index);
  template <typename T>
  TNode<T> LoadElement(ElementAccess const& access, TNode<HeapObject> object,
                       TNode<Number> index) {
    // TODO(jgruber): Investigate issues on ptr compression bots and enable.
    // DCHECK(IsMachineRepresentationOf<T>(
    //     access.machine_type.representation()));
    return TNode<T>::UncheckedCast(LoadElement(access, object, index));
  }
  Node* StoreField(FieldAccess const&, Node* object, Node* value);
  Node* StoreElement(ElementAccess const&, Node* object, Node* index,
                     Node* value);
  void TransitionAndStoreElement(MapRef double_map, MapRef fast_map,
                                 TNode<HeapObject> object, TNode<Number> index,
                                 TNode<Object> value);
  TNode<Number> StringLength(TNode<String> string);
  TNode<Boolean> ReferenceEqual(TNode<Object> lhs, TNode<Object> rhs);
  TNode<Number> PlainPrimitiveToNumber(TNode<Object> value);
  TNode<Number> NumberMin(TNode<Number> lhs, TNode<Number> rhs);
  TNode<Number> NumberMax(TNode<Number> lhs, TNode<Number> rhs);
  TNode<Boolean> NumberLessThan(TNode<Number> lhs, TNode<Number> rhs);
  TNode<Boolean> NumberLessThanOrEqual(TNode<Number> lhs, TNode<Number> rhs);
  TNode<Number> NumberAdd(TNode<Number> lhs, TNode<Number> rhs);
  TNode<Number> NumberSubtract(TNode<Number> lhs, TNode<Number> rhs);
  TNode<String> StringSubstring(TNode<String> string, TNode<Number> from,
                                TNode<Number> to);
  TNode<Boolean> ObjectIsCallable(TNode<Object> value);
  TNode<Boolean> ObjectIsUndetectable(TNode<Object> value);
  Node* CheckIf(Node* cond, DeoptimizeReason reason);
  TNode<Boolean> NumberIsFloat64Hole(TNode<Number> value);
  TNode<Boolean> ToBoolean(TNode<Object> value);
  TNode<Object> ConvertTaggedHoleToUndefined(TNode<Object> value);
  TNode<FixedArrayBase> MaybeGrowFastElements(ElementsKind kind,
                                              const FeedbackSource& feedback,
                                              TNode<JSArray> array,
                                              TNode<FixedArrayBase> elements,
                                              TNode<Number> new_length,
                                              TNode<Number> old_length);

  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const { return jsgraph()->isolate(); }
  SimplifiedOperatorBuilder* simplified() const {
    return jsgraph()->simplified();
  }

 protected:
  Operator const* PlainPrimitiveToNumberOperator();

 private:
  JSGraph* jsgraph_;
  SetOncePointer<Operator const> to_number_operator_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_ASSEMBLER_H_
