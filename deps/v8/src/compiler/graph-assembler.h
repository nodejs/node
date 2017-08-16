// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_ASSEMBLER_H_
#define V8_COMPILER_GRAPH_ASSEMBLER_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

class JSGraph;
class Graph;

namespace compiler {

#define PURE_ASSEMBLER_MACH_UNOP_LIST(V) \
  V(ChangeInt32ToInt64)                  \
  V(ChangeInt32ToFloat64)                \
  V(ChangeUint32ToFloat64)               \
  V(ChangeUint32ToUint64)                \
  V(ChangeFloat64ToInt32)                \
  V(ChangeFloat64ToUint32)               \
  V(TruncateInt64ToInt32)                \
  V(RoundFloat64ToInt32)                 \
  V(TruncateFloat64ToWord32)             \
  V(Float64ExtractHighWord32)            \
  V(Float64Abs)                          \
  V(BitcastWordToTagged)

#define PURE_ASSEMBLER_MACH_BINOP_LIST(V) \
  V(WordShl)                              \
  V(WordSar)                              \
  V(WordAnd)                              \
  V(Word32Or)                             \
  V(Word32And)                            \
  V(Word32Shr)                            \
  V(Word32Shl)                            \
  V(IntAdd)                               \
  V(IntSub)                               \
  V(IntLessThan)                          \
  V(UintLessThan)                         \
  V(Int32Add)                             \
  V(Int32Sub)                             \
  V(Int32Mul)                             \
  V(Int32LessThanOrEqual)                 \
  V(Uint32LessThanOrEqual)                \
  V(Uint32LessThan)                       \
  V(Int32LessThan)                        \
  V(Float64Add)                           \
  V(Float64Sub)                           \
  V(Float64Mod)                           \
  V(Float64Equal)                         \
  V(Float64LessThan)                      \
  V(Float64LessThanOrEqual)               \
  V(Word32Equal)                          \
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
  V(HeapNumberMapConstant)                 \
  V(NoContextConstant)                     \
  V(EmptyStringConstant)                   \
  V(UndefinedConstant)                     \
  V(TheHoleConstant)                       \
  V(FixedArrayMapConstant)                 \
  V(ToNumberBuiltinConstant)               \
  V(AllocateInNewSpaceStubConstant)        \
  V(AllocateInOldSpaceStubConstant)

class GraphAssembler;

enum class GraphAssemblerLabelType { kDeferred, kNonDeferred };

// Label with statically known count of incoming branches and phis.
template <size_t MergeCount, size_t VarCount = 0u>
class GraphAssemblerStaticLabel {
 public:
  Node* PhiAt(size_t index);

  template <typename... Reps>
  explicit GraphAssemblerStaticLabel(GraphAssemblerLabelType is_deferred,
                                     Reps... reps)
      : is_deferred_(is_deferred == GraphAssemblerLabelType::kDeferred) {
    STATIC_ASSERT(VarCount == sizeof...(reps));
    MachineRepresentation reps_array[] = {MachineRepresentation::kNone,
                                          reps...};
    for (size_t i = 0; i < VarCount; i++) {
      representations_[i] = reps_array[i + 1];
    }
  }

  ~GraphAssemblerStaticLabel() { DCHECK(IsBound() || MergedCount() == 0); }

 private:
  friend class GraphAssembler;

  void SetBound() {
    DCHECK(!IsBound());
    DCHECK_EQ(merged_count_, MergeCount);
    is_bound_ = true;
  }
  bool IsBound() const { return is_bound_; }

  size_t PhiCount() const { return VarCount; }
  size_t MaxMergeCount() const { return MergeCount; }
  size_t MergedCount() const { return merged_count_; }
  bool IsDeferred() const { return is_deferred_; }

  // For each phi, the buffer must have at least MaxMergeCount() + 1
  // node entries.
  Node** GetBindingsPtrFor(size_t phi_index) {
    DCHECK_LT(phi_index, PhiCount());
    return &bindings_[phi_index * (MergeCount + 1)];
  }
  void SetBinding(size_t phi_index, size_t merge_index, Node* binding) {
    DCHECK_LT(phi_index, PhiCount());
    DCHECK_LT(merge_index, MergeCount);
    bindings_[phi_index * (MergeCount + 1) + merge_index] = binding;
  }
  MachineRepresentation GetRepresentationFor(size_t phi_index) {
    DCHECK_LT(phi_index, PhiCount());
    return representations_[phi_index];
  }
  // The controls buffer must have at least MaxMergeCount() entries.
  Node** GetControlsPtr() { return controls_; }
  // The effects buffer must have at least MaxMergeCount() + 1 entries.
  Node** GetEffectsPtr() { return effects_; }
  void IncrementMergedCount() { merged_count_++; }

  bool is_bound_ = false;
  bool is_deferred_;
  size_t merged_count_ = 0;
  Node* effects_[MergeCount + 1];  // Extra element for control edge,
                                   // so that we can use the array to
                                   // construct EffectPhi.
  Node* controls_[MergeCount];
  Node* bindings_[(MergeCount + 1) * VarCount + 1];
  MachineRepresentation representations_[VarCount + 1];
};

// General label (with zone allocated buffers for incoming branches and phi
// inputs).
class GraphAssemblerLabel {
 public:
  Node* PhiAt(size_t index);

  GraphAssemblerLabel(GraphAssemblerLabelType is_deferred, size_t merge_count,
                      size_t var_count, MachineRepresentation* representations,
                      Zone* zone);

  ~GraphAssemblerLabel();

 private:
  friend class GraphAssembler;

  void SetBound() {
    DCHECK(!is_bound_);
    is_bound_ = true;
  }
  bool IsBound() const { return is_bound_; }
  size_t PhiCount() const { return var_count_; }
  size_t MaxMergeCount() const { return max_merge_count_; }
  size_t MergedCount() const { return merged_count_; }
  bool IsDeferred() const { return is_deferred_; }

  // For each phi, the buffer must have at least MaxMergeCount() + 1
  // node entries.
  Node** GetBindingsPtrFor(size_t phi_index);
  void SetBinding(size_t phi_index, size_t merge_index, Node* binding);
  MachineRepresentation GetRepresentationFor(size_t phi_index);
  // The controls buffer must have at least MaxMergeCount() entries.
  Node** GetControlsPtr();
  // The effects buffer must have at least MaxMergeCount() + 1 entries.
  Node** GetEffectsPtr();
  void IncrementMergedCount() { merged_count_++; }

  bool is_bound_ = false;
  bool is_deferred_;
  size_t merged_count_ = 0;
  size_t max_merge_count_;
  size_t var_count_;
  Node** effects_ = nullptr;
  Node** controls_ = nullptr;
  Node** bindings_ = nullptr;
  MachineRepresentation* representations_ = nullptr;
};

class GraphAssembler {
 public:
  GraphAssembler(JSGraph* jsgraph, Node* effect, Node* control, Zone* zone);

  void Reset(Node* effect, Node* control);

  // Create non-deferred label with statically known number of incoming
  // gotos/branches.
  template <size_t MergeCount, typename... Reps>
  static GraphAssemblerStaticLabel<MergeCount, sizeof...(Reps)> MakeLabel(
      Reps... reps) {
    return GraphAssemblerStaticLabel<MergeCount, sizeof...(Reps)>(
        GraphAssemblerLabelType::kNonDeferred, reps...);
  }

  // Create deferred label with statically known number of incoming
  // gotos/branches.
  template <size_t MergeCount, typename... Reps>
  static GraphAssemblerStaticLabel<MergeCount, sizeof...(Reps)>
  MakeDeferredLabel(Reps... reps) {
    return GraphAssemblerStaticLabel<MergeCount, sizeof...(Reps)>(
        GraphAssemblerLabelType::kDeferred, reps...);
  }

  // Create label with number of incoming branches supplied at runtime.
  template <typename... Reps>
  GraphAssemblerLabel MakeLabelFor(GraphAssemblerLabelType is_deferred,
                                   size_t merge_count, Reps... reps) {
    MachineRepresentation reps_array[] = {MachineRepresentation::kNone,
                                          reps...};
    return GraphAssemblerLabel(is_deferred, merge_count, sizeof...(reps),
                               &(reps_array[1]), temp_zone());
  }

  // Value creation.
  Node* IntPtrConstant(intptr_t value);
  Node* Uint32Constant(int32_t value);
  Node* Int32Constant(int32_t value);
  Node* UniqueInt32Constant(int32_t value);
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

  Node* Float64RoundDown(Node* value);

  Node* ToNumber(Node* value);
  Node* Allocate(PretenureFlag pretenure, Node* size);
  Node* LoadField(FieldAccess const&, Node* object);
  Node* LoadElement(ElementAccess const&, Node* object, Node* index);
  Node* StoreField(FieldAccess const&, Node* object, Node* value);
  Node* StoreElement(ElementAccess const&, Node* object, Node* index,
                     Node* value);

  Node* Store(StoreRepresentation rep, Node* object, Node* offset, Node* value);
  Node* Load(MachineType rep, Node* object, Node* offset);

  Node* Retain(Node* buffer);
  Node* UnsafePointerAdd(Node* base, Node* external);

  Node* DeoptimizeIf(DeoptimizeReason reason, Node* condition,
                     Node* frame_state);
  Node* DeoptimizeUnless(DeoptimizeKind kind, DeoptimizeReason reason,
                         Node* condition, Node* frame_state);
  Node* DeoptimizeUnless(DeoptimizeReason reason, Node* condition,
                         Node* frame_state);
  template <typename... Args>
  Node* Call(const CallDescriptor* desc, Args... args);
  template <typename... Args>
  Node* Call(const Operator* op, Args... args);

  // Basic control operations.
  template <class LabelType>
  void Bind(LabelType* label);

  template <class LabelType, typename... vars>
  void Goto(LabelType* label, vars...);

  void Branch(Node* condition, GraphAssemblerStaticLabel<1>* if_true,
              GraphAssemblerStaticLabel<1>* if_false);

  // Control helpers.
  // {GotoIf(c, l)} is equivalent to {Branch(c, l, templ);Bind(templ)}.
  template <class LabelType, typename... vars>
  void GotoIf(Node* condition, LabelType* label, vars...);

  // {GotoUnless(c, l)} is equivalent to {Branch(c, templ, l);Bind(templ)}.
  template <class LabelType, typename... vars>
  void GotoUnless(Node* condition, LabelType* label, vars...);

  // Extractors (should be only used when destructing/resetting the assembler).
  Node* ExtractCurrentControl();
  Node* ExtractCurrentEffect();

 private:
  template <class LabelType, typename... Vars>
  void MergeState(LabelType label, Vars... vars);

  Operator const* ToNumberOperator();

  JSGraph* jsgraph() const { return jsgraph_; }
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

template <size_t MergeCount, size_t VarCount>
Node* GraphAssemblerStaticLabel<MergeCount, VarCount>::PhiAt(size_t index) {
  DCHECK(IsBound());
  return GetBindingsPtrFor(index)[0];
}

template <class LabelType, typename... Vars>
void GraphAssembler::MergeState(LabelType label, Vars... vars) {
  DCHECK(!label->IsBound());
  size_t merged_count = label->MergedCount();
  DCHECK_LT(merged_count, label->MaxMergeCount());
  DCHECK_EQ(label->PhiCount(), sizeof...(vars));
  label->GetEffectsPtr()[merged_count] = current_effect_;
  label->GetControlsPtr()[merged_count] = current_control_;
  // We need to start with nullptr to avoid 0-length arrays.
  Node* var_array[] = {nullptr, vars...};
  for (size_t i = 0; i < sizeof...(vars); i++) {
    label->SetBinding(i, merged_count, var_array[i + 1]);
  }
  label->IncrementMergedCount();
}

template <class LabelType>
void GraphAssembler::Bind(LabelType* label) {
  DCHECK(current_control_ == nullptr);
  DCHECK(current_effect_ == nullptr);
  DCHECK(label->MaxMergeCount() > 0);
  DCHECK_EQ(label->MaxMergeCount(), label->MergedCount());

  int merge_count = static_cast<int>(label->MaxMergeCount());
  if (merge_count == 1) {
    current_control_ = label->GetControlsPtr()[0];
    current_effect_ = label->GetEffectsPtr()[0];
    label->SetBound();
    return;
  }

  current_control_ = graph()->NewNode(common()->Merge(merge_count), merge_count,
                                      label->GetControlsPtr());

  Node** effects = label->GetEffectsPtr();
  current_effect_ = effects[0];
  for (size_t i = 1; i < label->MaxMergeCount(); i++) {
    if (current_effect_ != effects[i]) {
      effects[label->MaxMergeCount()] = current_control_;
      current_effect_ = graph()->NewNode(common()->EffectPhi(merge_count),
                                         merge_count + 1, effects);
      break;
    }
  }

  for (size_t var = 0; var < label->PhiCount(); var++) {
    Node** bindings = label->GetBindingsPtrFor(var);
    bindings[label->MaxMergeCount()] = current_control_;
    bindings[0] = graph()->NewNode(
        common()->Phi(label->GetRepresentationFor(var), merge_count),
        merge_count + 1, bindings);
  }

  label->SetBound();
}

template <class LabelType, typename... Vars>
void GraphAssembler::Goto(LabelType* label, Vars... vars) {
  DCHECK_NOT_NULL(current_control_);
  DCHECK_NOT_NULL(current_effect_);
  MergeState(label, vars...);
  current_control_ = nullptr;
  current_effect_ = nullptr;
}

template <class LabelType, typename... Vars>
void GraphAssembler::GotoIf(Node* condition, LabelType* label, Vars... vars) {
  BranchHint hint =
      label->IsDeferred() ? BranchHint::kFalse : BranchHint::kNone;
  Node* branch =
      graph()->NewNode(common()->Branch(hint), condition, current_control_);

  current_control_ = graph()->NewNode(common()->IfTrue(), branch);
  MergeState(label, vars...);

  current_control_ = graph()->NewNode(common()->IfFalse(), branch);
}

template <class LabelType, typename... Vars>
void GraphAssembler::GotoUnless(Node* condition, LabelType* label,
                                Vars... vars) {
  BranchHint hint = label->IsDeferred() ? BranchHint::kTrue : BranchHint::kNone;
  Node* branch =
      graph()->NewNode(common()->Branch(hint), condition, current_control_);

  current_control_ = graph()->NewNode(common()->IfFalse(), branch);
  MergeState(label, vars...);

  current_control_ = graph()->NewNode(common()->IfTrue(), branch);
}

template <typename... Args>
Node* GraphAssembler::Call(const CallDescriptor* desc, Args... args) {
  const Operator* op = common()->Call(desc);
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
