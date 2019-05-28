// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_ASSEMBLER_H_
#define V8_COMPILER_GRAPH_ASSEMBLER_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "src/vector-slot-pair.h"

namespace v8 {
namespace internal {

class JSGraph;
class Graph;

namespace compiler {

#define PURE_ASSEMBLER_MACH_UNOP_LIST(V)    \
  V(ChangeInt32ToInt64)                     \
  V(ChangeInt32ToFloat64)                   \
  V(ChangeInt64ToFloat64)                   \
  V(ChangeUint32ToFloat64)                  \
  V(ChangeUint32ToUint64)                   \
  V(ChangeFloat64ToInt32)                   \
  V(ChangeFloat64ToInt64)                   \
  V(ChangeFloat64ToUint32)                  \
  V(TruncateInt64ToInt32)                   \
  V(RoundFloat64ToInt32)                    \
  V(TruncateFloat64ToInt64)                 \
  V(TruncateFloat64ToWord32)                \
  V(Float64ExtractLowWord32)                \
  V(Float64ExtractHighWord32)               \
  V(BitcastInt32ToFloat32)                  \
  V(BitcastInt64ToFloat64)                  \
  V(BitcastFloat32ToInt32)                  \
  V(BitcastFloat64ToInt64)                  \
  V(Float64Abs)                             \
  V(Word32ReverseBytes)                     \
  V(Word64ReverseBytes)                     \
  V(Float64SilenceNaN)                      \
  V(ChangeCompressedToTagged)               \
  V(ChangeTaggedToCompressed)               \
  V(ChangeTaggedSignedToCompressedSigned)   \
  V(ChangeCompressedSignedToTaggedSigned)   \
  V(ChangeTaggedPointerToCompressedPointer) \
  V(ChangeCompressedPointerToTaggedPointer)
#define PURE_ASSEMBLER_MACH_BINOP_LIST(V) \
  V(WordShl)                              \
  V(WordSar)                              \
  V(WordAnd)                              \
  V(Word32Or)                             \
  V(Word32And)                            \
  V(Word32Xor)                            \
  V(Word32Shr)                            \
  V(Word32Shl)                            \
  V(Word32Sar)                            \
  V(IntAdd)                               \
  V(IntSub)                               \
  V(IntMul)                               \
  V(IntLessThan)                          \
  V(UintLessThan)                         \
  V(Int32Add)                             \
  V(Int32Sub)                             \
  V(Int32Mul)                             \
  V(Int32LessThanOrEqual)                 \
  V(Uint32LessThan)                       \
  V(Uint32LessThanOrEqual)                \
  V(Uint64LessThan)                       \
  V(Uint64LessThanOrEqual)                \
  V(Int32LessThan)                        \
  V(Float64Add)                           \
  V(Float64Sub)                           \
  V(Float64Div)                           \
  V(Float64Mod)                           \
  V(Float64Equal)                         \
  V(Float64LessThan)                      \
  V(Float64LessThanOrEqual)               \
  V(Float64InsertLowWord32)               \
  V(Float64InsertHighWord32)              \
  V(Word32Equal)                          \
  V(Word64Equal)                          \
  V(WordEqual)

#define CHECKED_ASSEMBLER_MACH_BINOP_LIST(V) \
  V(Int32AddWithOverflow)                    \
  V(Int32SubWithOverflow)                    \
  V(Int32MulWithOverflow)                    \
  V(Int32Mod)                                \
  V(Int32Div)                                \
  V(Uint32Mod)                               \
  V(Uint32Div)

#define JSGRAPH_SINGLETON_CONSTANT_LIST(V) \
  V(TrueConstant)                          \
  V(FalseConstant)                         \
  V(NullConstant)                          \
  V(BigIntMapConstant)                     \
  V(BooleanMapConstant)                    \
  V(HeapNumberMapConstant)                 \
  V(NoContextConstant)                     \
  V(EmptyStringConstant)                   \
  V(UndefinedConstant)                     \
  V(TheHoleConstant)                       \
  V(FixedArrayMapConstant)                 \
  V(FixedDoubleArrayMapConstant)           \
  V(ToNumberBuiltinConstant)               \
  V(AllocateInYoungGenerationStubConstant) \
  V(AllocateInOldGenerationStubConstant)

class GraphAssembler;

enum class GraphAssemblerLabelType { kDeferred, kNonDeferred, kLoop };

// Label with statically known count of incoming branches and phis.
template <size_t VarCount>
class GraphAssemblerLabel {
 public:
  Node* PhiAt(size_t index);

  template <typename... Reps>
  explicit GraphAssemblerLabel(GraphAssemblerLabelType type, Reps... reps)
      : type_(type) {
    STATIC_ASSERT(VarCount == sizeof...(reps));
    MachineRepresentation reps_array[] = {MachineRepresentation::kNone,
                                          reps...};
    for (size_t i = 0; i < VarCount; i++) {
      representations_[i] = reps_array[i + 1];
    }
  }

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

  bool is_bound_ = false;
  GraphAssemblerLabelType const type_;
  size_t merged_count_ = 0;
  Node* effect_;
  Node* control_;
  Node* bindings_[VarCount + 1];
  MachineRepresentation representations_[VarCount + 1];
};

class GraphAssembler {
 public:
  GraphAssembler(JSGraph* jsgraph, Node* effect, Node* control, Zone* zone);

  void Reset(Node* effect, Node* control);

  // Create label.
  template <typename... Reps>
  static GraphAssemblerLabel<sizeof...(Reps)> MakeLabelFor(
      GraphAssemblerLabelType type, Reps... reps) {
    return GraphAssemblerLabel<sizeof...(Reps)>(type, reps...);
  }

  // Convenience wrapper for creating non-deferred labels.
  template <typename... Reps>
  static GraphAssemblerLabel<sizeof...(Reps)> MakeLabel(Reps... reps) {
    return MakeLabelFor(GraphAssemblerLabelType::kNonDeferred, reps...);
  }

  // Convenience wrapper for creating loop labels.
  template <typename... Reps>
  static GraphAssemblerLabel<sizeof...(Reps)> MakeLoopLabel(Reps... reps) {
    return MakeLabelFor(GraphAssemblerLabelType::kLoop, reps...);
  }

  // Convenience wrapper for creating deferred labels.
  template <typename... Reps>
  static GraphAssemblerLabel<sizeof...(Reps)> MakeDeferredLabel(Reps... reps) {
    return MakeLabelFor(GraphAssemblerLabelType::kDeferred, reps...);
  }

  // Value creation.
  Node* IntPtrConstant(intptr_t value);
  Node* Uint32Constant(int32_t value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* UniqueIntPtrConstant(intptr_t value);
  Node* SmiConstant(int32_t value);
  Node* Float64Constant(double value);
  Node* Projection(int index, Node* value);
  Node* HeapConstant(Handle<HeapObject> object);
  Node* CEntryStubConstant(int result_size);
  Node* ExternalConstant(ExternalReference ref);

  Node* LoadFramePointer();

#define SINGLETON_CONST_DECL(Name) Node* Name();
  JSGRAPH_SINGLETON_CONSTANT_LIST(SINGLETON_CONST_DECL)
#undef SINGLETON_CONST_DECL

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

  Node* Float64RoundDown(Node* value);
  Node* Float64RoundTruncate(Node* value);

  Node* ToNumber(Node* value);
  Node* BitcastWordToTagged(Node* value);
  Node* BitcastTaggedToWord(Node* value);
  Node* Allocate(AllocationType allocation, Node* size);
  Node* LoadField(FieldAccess const&, Node* object);
  Node* LoadElement(ElementAccess const&, Node* object, Node* index);
  Node* StoreField(FieldAccess const&, Node* object, Node* value);
  Node* StoreElement(ElementAccess const&, Node* object, Node* index,
                     Node* value);

  Node* Store(StoreRepresentation rep, Node* object, Node* offset, Node* value);
  Node* Load(MachineType rep, Node* object, Node* offset);

  Node* StoreUnaligned(MachineRepresentation rep, Node* object, Node* offset,
                       Node* value);
  Node* LoadUnaligned(MachineType rep, Node* object, Node* offset);

  Node* Retain(Node* buffer);
  Node* UnsafePointerAdd(Node* base, Node* external);

  Node* Word32PoisonOnSpeculation(Node* value);

  Node* DeoptimizeIf(
      DeoptimizeReason reason, VectorSlotPair const& feedback, Node* condition,
      Node* frame_state,
      IsSafetyCheck is_safety_check = IsSafetyCheck::kSafetyCheck);
  Node* DeoptimizeIfNot(
      DeoptimizeReason reason, VectorSlotPair const& feedback, Node* condition,
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

  void Branch(Node* condition, GraphAssemblerLabel<0u>* if_true,
              GraphAssemblerLabel<0u>* if_false,
              IsSafetyCheck is_safety_check = IsSafetyCheck::kNoSafetyCheck);

  // Control helpers.
  // {GotoIf(c, l)} is equivalent to {Branch(c, l, templ);Bind(templ)}.
  template <typename... Vars>
  void GotoIf(Node* condition, GraphAssemblerLabel<sizeof...(Vars)>* label,
              Vars...);

  // {GotoIfNot(c, l)} is equivalent to {Branch(c, templ, l);Bind(templ)}.
  template <typename... Vars>
  void GotoIfNot(Node* condition, GraphAssemblerLabel<sizeof...(Vars)>* label,
                 Vars...);

  // Extractors (should be only used when destructing/resetting the assembler).
  Node* ExtractCurrentControl();
  Node* ExtractCurrentEffect();

 private:
  template <typename... Vars>
  void MergeState(GraphAssemblerLabel<sizeof...(Vars)>* label, Vars... vars);

  Operator const* ToNumberOperator();

  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const { return jsgraph_->isolate(); }
  Graph* graph() const { return jsgraph_->graph(); }
  Zone* temp_zone() const { return temp_zone_; }
  CommonOperatorBuilder* common() const { return jsgraph()->common(); }
  MachineOperatorBuilder* machine() const { return jsgraph()->machine(); }
  SimplifiedOperatorBuilder* simplified() const {
    return jsgraph()->simplified();
  }

  SetOncePointer<Operator const> to_number_operator_;
  Zone* temp_zone_;
  JSGraph* jsgraph_;
  Node* current_effect_;
  Node* current_control_;
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
  int merged_count = static_cast<int>(label->merged_count_);
  Node* var_array[] = {nullptr, vars...};
  if (label->IsLoop()) {
    if (merged_count == 0) {
      DCHECK(!label->IsBound());
      label->control_ = graph()->NewNode(common()->Loop(2), current_control_,
                                         current_control_);
      label->effect_ = graph()->NewNode(common()->EffectPhi(2), current_effect_,
                                        current_effect_, label->control_);
      Node* terminate = graph()->NewNode(common()->Terminate(), label->effect_,
                                         label->control_);
      NodeProperties::MergeControlToEnd(graph(), common(), terminate);
      for (size_t i = 0; i < sizeof...(vars); i++) {
        label->bindings_[i] = graph()->NewNode(
            common()->Phi(label->representations_[i], 2), var_array[i + 1],
            var_array[i + 1], label->control_);
      }
    } else {
      DCHECK(label->IsBound());
      DCHECK_EQ(1, merged_count);
      label->control_->ReplaceInput(1, current_control_);
      label->effect_->ReplaceInput(1, current_effect_);
      for (size_t i = 0; i < sizeof...(vars); i++) {
        label->bindings_[i]->ReplaceInput(1, var_array[i + 1]);
      }
    }
  } else {
    DCHECK(!label->IsBound());
    if (merged_count == 0) {
      // Just set the control, effect and variables directly.
      DCHECK(!label->IsBound());
      label->control_ = current_control_;
      label->effect_ = current_effect_;
      for (size_t i = 0; i < sizeof...(vars); i++) {
        label->bindings_[i] = var_array[i + 1];
      }
    } else if (merged_count == 1) {
      // Create merge, effect phi and a phi for each variable.
      label->control_ = graph()->NewNode(common()->Merge(2), label->control_,
                                         current_control_);
      label->effect_ = graph()->NewNode(common()->EffectPhi(2), label->effect_,
                                        current_effect_, label->control_);
      for (size_t i = 0; i < sizeof...(vars); i++) {
        label->bindings_[i] = graph()->NewNode(
            common()->Phi(label->representations_[i], 2), label->bindings_[i],
            var_array[i + 1], label->control_);
      }
    } else {
      // Append to the merge, effect phi and phis.
      DCHECK_EQ(IrOpcode::kMerge, label->control_->opcode());
      label->control_->AppendInput(graph()->zone(), current_control_);
      NodeProperties::ChangeOp(label->control_,
                               common()->Merge(merged_count + 1));

      DCHECK_EQ(IrOpcode::kEffectPhi, label->effect_->opcode());
      label->effect_->ReplaceInput(merged_count, current_effect_);
      label->effect_->AppendInput(graph()->zone(), label->control_);
      NodeProperties::ChangeOp(label->effect_,
                               common()->EffectPhi(merged_count + 1));

      for (size_t i = 0; i < sizeof...(vars); i++) {
        DCHECK_EQ(IrOpcode::kPhi, label->bindings_[i]->opcode());
        label->bindings_[i]->ReplaceInput(merged_count, var_array[i + 1]);
        label->bindings_[i]->AppendInput(graph()->zone(), label->control_);
        NodeProperties::ChangeOp(
            label->bindings_[i],
            common()->Phi(label->representations_[i], merged_count + 1));
      }
    }
  }
  label->merged_count_++;
}

template <size_t VarCount>
void GraphAssembler::Bind(GraphAssemblerLabel<VarCount>* label) {
  DCHECK_NULL(current_control_);
  DCHECK_NULL(current_effect_);
  DCHECK_LT(0, label->merged_count_);

  current_control_ = label->control_;
  current_effect_ = label->effect_;

  label->SetBound();
}

template <typename... Vars>
void GraphAssembler::Goto(GraphAssemblerLabel<sizeof...(Vars)>* label,
                          Vars... vars) {
  DCHECK_NOT_NULL(current_control_);
  DCHECK_NOT_NULL(current_effect_);
  MergeState(label, vars...);
  current_control_ = nullptr;
  current_effect_ = nullptr;
}

template <typename... Vars>
void GraphAssembler::GotoIf(Node* condition,
                            GraphAssemblerLabel<sizeof...(Vars)>* label,
                            Vars... vars) {
  BranchHint hint =
      label->IsDeferred() ? BranchHint::kFalse : BranchHint::kNone;
  Node* branch =
      graph()->NewNode(common()->Branch(hint), condition, current_control_);

  current_control_ = graph()->NewNode(common()->IfTrue(), branch);
  MergeState(label, vars...);

  current_control_ = graph()->NewNode(common()->IfFalse(), branch);
}

template <typename... Vars>
void GraphAssembler::GotoIfNot(Node* condition,
                               GraphAssemblerLabel<sizeof...(Vars)>* label,
                               Vars... vars) {
  BranchHint hint = label->IsDeferred() ? BranchHint::kTrue : BranchHint::kNone;
  Node* branch =
      graph()->NewNode(common()->Branch(hint), condition, current_control_);

  current_control_ = graph()->NewNode(common()->IfFalse(), branch);
  MergeState(label, vars...);

  current_control_ = graph()->NewNode(common()->IfTrue(), branch);
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
  Node* args_array[] = {args..., current_effect_, current_control_};
  int size = static_cast<int>(sizeof...(args)) + op->EffectInputCount() +
             op->ControlInputCount();
  Node* call = graph()->NewNode(op, size, args_array);
  DCHECK_EQ(0, op->ControlOutputCount());
  current_effect_ = call;
  return call;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_ASSEMBLER_H_
