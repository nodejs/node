// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-assembler.h"

#include "src/codegen/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/linkage.h"
#include "src/compiler/schedule.h"
// For TNode types.
#include "src/objects/heap-number.h"
#include "src/objects/oddball.h"
#include "src/objects/smi.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {
namespace compiler {

class V8_NODISCARD GraphAssembler::BlockInlineReduction {
 public:
  explicit BlockInlineReduction(GraphAssembler* gasm) : gasm_(gasm) {
    DCHECK(!gasm_->inline_reductions_blocked_);
    gasm_->inline_reductions_blocked_ = true;
  }
  ~BlockInlineReduction() {
    DCHECK(gasm_->inline_reductions_blocked_);
    gasm_->inline_reductions_blocked_ = false;
  }

 private:
  GraphAssembler* gasm_;
};

GraphAssembler::GraphAssembler(
    MachineGraph* mcgraph, Zone* zone,
    base::Optional<NodeChangedCallback> node_changed_callback,
    bool mark_loop_exits)
    : temp_zone_(zone),
      mcgraph_(mcgraph),
      effect_(nullptr),
      control_(nullptr),
      node_changed_callback_(node_changed_callback),
      inline_reducers_(zone),
      inline_reductions_blocked_(false),
      loop_headers_(zone),
      mark_loop_exits_(mark_loop_exits) {}

GraphAssembler::~GraphAssembler() { DCHECK_EQ(loop_nesting_level_, 0); }

Node* GraphAssembler::IntPtrConstant(intptr_t value) {
  return AddClonedNode(mcgraph()->IntPtrConstant(value));
}

Node* GraphAssembler::UintPtrConstant(uintptr_t value) {
  return AddClonedNode(mcgraph()->UintPtrConstant(value));
}

Node* GraphAssembler::Int32Constant(int32_t value) {
  return AddClonedNode(mcgraph()->Int32Constant(value));
}

Node* GraphAssembler::Uint32Constant(uint32_t value) {
  return AddClonedNode(mcgraph()->Uint32Constant(value));
}

Node* GraphAssembler::Int64Constant(int64_t value) {
  return AddClonedNode(mcgraph()->Int64Constant(value));
}

Node* GraphAssembler::Uint64Constant(uint64_t value) {
  return AddClonedNode(mcgraph()->Uint64Constant(value));
}

Node* GraphAssembler::UniqueIntPtrConstant(intptr_t value) {
  return AddNode(graph()->NewNode(
      machine()->Is64()
          ? common()->Int64Constant(value)
          : common()->Int32Constant(static_cast<int32_t>(value))));
}

Node* JSGraphAssembler::SmiConstant(int32_t value) {
  return AddClonedNode(jsgraph()->SmiConstant(value));
}

Node* GraphAssembler::Float64Constant(double value) {
  return AddClonedNode(mcgraph()->Float64Constant(value));
}

TNode<HeapObject> JSGraphAssembler::HeapConstant(Handle<HeapObject> object) {
  return TNode<HeapObject>::UncheckedCast(
      AddClonedNode(jsgraph()->HeapConstant(object)));
}

TNode<Object> JSGraphAssembler::Constant(const ObjectRef& ref) {
  return TNode<Object>::UncheckedCast(AddClonedNode(jsgraph()->Constant(ref)));
}

TNode<Number> JSGraphAssembler::NumberConstant(double value) {
  return TNode<Number>::UncheckedCast(
      AddClonedNode(jsgraph()->Constant(value)));
}

Node* GraphAssembler::ExternalConstant(ExternalReference ref) {
  return AddClonedNode(mcgraph()->ExternalConstant(ref));
}

Node* GraphAssembler::Parameter(int index) {
  return AddNode(
      graph()->NewNode(common()->Parameter(index), graph()->start()));
}

Node* JSGraphAssembler::CEntryStubConstant(int result_size) {
  return AddClonedNode(jsgraph()->CEntryStubConstant(result_size));
}

Node* GraphAssembler::LoadFramePointer() {
  return AddNode(graph()->NewNode(machine()->LoadFramePointer()));
}

Node* GraphAssembler::LoadHeapNumberValue(Node* heap_number) {
  return Load(MachineType::Float64(), heap_number,
              IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag));
}

#define SINGLETON_CONST_DEF(Name, Type)              \
  TNode<Type> JSGraphAssembler::Name##Constant() {   \
    return TNode<Type>::UncheckedCast(               \
        AddClonedNode(jsgraph()->Name##Constant())); \
  }
JSGRAPH_SINGLETON_CONSTANT_LIST(SINGLETON_CONST_DEF)
#undef SINGLETON_CONST_DEF

#define SINGLETON_CONST_TEST_DEF(Name, ...)                        \
  TNode<Boolean> JSGraphAssembler::Is##Name(TNode<Object> value) { \
    return TNode<Boolean>::UncheckedCast(                          \
        ReferenceEqual(value, Name##Constant()));                  \
  }
JSGRAPH_SINGLETON_CONSTANT_LIST(SINGLETON_CONST_TEST_DEF)
#undef SINGLETON_CONST_TEST_DEF

#define PURE_UNOP_DEF(Name)                                     \
  Node* GraphAssembler::Name(Node* input) {                     \
    return AddNode(graph()->NewNode(machine()->Name(), input)); \
  }
PURE_ASSEMBLER_MACH_UNOP_LIST(PURE_UNOP_DEF)
#undef PURE_UNOP_DEF

#define PURE_BINOP_DEF(Name)                                          \
  Node* GraphAssembler::Name(Node* left, Node* right) {               \
    return AddNode(graph()->NewNode(machine()->Name(), left, right)); \
  }
PURE_ASSEMBLER_MACH_BINOP_LIST(PURE_BINOP_DEF)
#undef PURE_BINOP_DEF

#define CHECKED_BINOP_DEF(Name)                                       \
  Node* GraphAssembler::Name(Node* left, Node* right) {               \
    return AddNode(                                                   \
        graph()->NewNode(machine()->Name(), left, right, control())); \
  }
CHECKED_ASSEMBLER_MACH_BINOP_LIST(CHECKED_BINOP_DEF)
#undef CHECKED_BINOP_DEF

Node* GraphAssembler::IntPtrEqual(Node* left, Node* right) {
  return WordEqual(left, right);
}

Node* GraphAssembler::TaggedEqual(Node* left, Node* right) {
  if (COMPRESS_POINTERS_BOOL) {
    return Word32Equal(left, right);
  } else {
    return WordEqual(left, right);
  }
}

Node* GraphAssembler::SmiSub(Node* left, Node* right) {
  if (COMPRESS_POINTERS_BOOL) {
    return Int32Sub(left, right);
  } else {
    return IntSub(left, right);
  }
}

Node* GraphAssembler::SmiLessThan(Node* left, Node* right) {
  if (COMPRESS_POINTERS_BOOL) {
    return Int32LessThan(left, right);
  } else {
    return IntLessThan(left, right);
  }
}

Node* GraphAssembler::Float64RoundDown(Node* value) {
  CHECK(machine()->Float64RoundDown().IsSupported());
  return AddNode(graph()->NewNode(machine()->Float64RoundDown().op(), value));
}

Node* GraphAssembler::Float64RoundTruncate(Node* value) {
  CHECK(machine()->Float64RoundTruncate().IsSupported());
  return AddNode(
      graph()->NewNode(machine()->Float64RoundTruncate().op(), value));
}

Node* GraphAssembler::TruncateFloat64ToInt64(Node* value, TruncateKind kind) {
  return AddNode(
      graph()->NewNode(machine()->TruncateFloat64ToInt64(kind), value));
}

Node* GraphAssembler::Projection(int index, Node* value) {
  return AddNode(
      graph()->NewNode(common()->Projection(index), value, control()));
}

Node* JSGraphAssembler::Allocate(AllocationType allocation, Node* size) {
  return AddNode(
      graph()->NewNode(simplified()->AllocateRaw(Type::Any(), allocation), size,
                       effect(), control()));
}

Node* JSGraphAssembler::LoadField(FieldAccess const& access, Node* object) {
  Node* value = AddNode(graph()->NewNode(simplified()->LoadField(access),
                                         object, effect(), control()));
  return value;
}

Node* JSGraphAssembler::LoadElement(ElementAccess const& access, Node* object,
                                    Node* index) {
  Node* value = AddNode(graph()->NewNode(simplified()->LoadElement(access),
                                         object, index, effect(), control()));
  return value;
}

Node* JSGraphAssembler::StoreField(FieldAccess const& access, Node* object,
                                   Node* value) {
  return AddNode(graph()->NewNode(simplified()->StoreField(access), object,
                                  value, effect(), control()));
}

#ifdef V8_MAP_PACKING
TNode<Map> GraphAssembler::UnpackMapWord(Node* map_word) {
  map_word = BitcastTaggedToWordForTagAndSmiBits(map_word);
  // TODO(wenyuzhao): Clear header metadata.
  Node* map = WordXor(map_word, IntPtrConstant(Internals::kMapWordXorMask));
  return TNode<Map>::UncheckedCast(BitcastWordToTagged(map));
}

Node* GraphAssembler::PackMapWord(TNode<Map> map) {
  Node* map_word = BitcastTaggedToWordForTagAndSmiBits(map);
  Node* packed = WordXor(map_word, IntPtrConstant(Internals::kMapWordXorMask));
  return BitcastWordToTaggedSigned(packed);
}
#endif

TNode<Map> GraphAssembler::LoadMap(Node* object) {
  Node* map_word = Load(MachineType::TaggedPointer(), object,
                        HeapObject::kMapOffset - kHeapObjectTag);
#ifdef V8_MAP_PACKING
  return UnpackMapWord(map_word);
#else
  return TNode<Map>::UncheckedCast(map_word);
#endif
}

Node* JSGraphAssembler::StoreElement(ElementAccess const& access, Node* object,
                                     Node* index, Node* value) {
  return AddNode(graph()->NewNode(simplified()->StoreElement(access), object,
                                  index, value, effect(), control()));
}

void JSGraphAssembler::TransitionAndStoreElement(MapRef double_map,
                                                 MapRef fast_map,
                                                 TNode<HeapObject> object,
                                                 TNode<Number> index,
                                                 TNode<Object> value) {
  AddNode(graph()->NewNode(simplified()->TransitionAndStoreElement(
                               double_map.object(), fast_map.object()),
                           object, index, value, effect(), control()));
}

TNode<Number> JSGraphAssembler::StringLength(TNode<String> string) {
  return AddNode<Number>(
      graph()->NewNode(simplified()->StringLength(), string));
}

TNode<Boolean> JSGraphAssembler::ReferenceEqual(TNode<Object> lhs,
                                                TNode<Object> rhs) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->ReferenceEqual(), lhs, rhs));
}

TNode<Boolean> JSGraphAssembler::NumberEqual(TNode<Number> lhs,
                                             TNode<Number> rhs) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->NumberEqual(), lhs, rhs));
}

TNode<Number> JSGraphAssembler::NumberMin(TNode<Number> lhs,
                                          TNode<Number> rhs) {
  return AddNode<Number>(graph()->NewNode(simplified()->NumberMin(), lhs, rhs));
}

TNode<Number> JSGraphAssembler::NumberMax(TNode<Number> lhs,
                                          TNode<Number> rhs) {
  return AddNode<Number>(graph()->NewNode(simplified()->NumberMax(), lhs, rhs));
}

TNode<Number> JSGraphAssembler::NumberAdd(TNode<Number> lhs,
                                          TNode<Number> rhs) {
  return AddNode<Number>(graph()->NewNode(simplified()->NumberAdd(), lhs, rhs));
}

TNode<Number> JSGraphAssembler::NumberSubtract(TNode<Number> lhs,
                                               TNode<Number> rhs) {
  return AddNode<Number>(
      graph()->NewNode(simplified()->NumberSubtract(), lhs, rhs));
}

TNode<Boolean> JSGraphAssembler::NumberLessThan(TNode<Number> lhs,
                                                TNode<Number> rhs) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->NumberLessThan(), lhs, rhs));
}

TNode<Boolean> JSGraphAssembler::NumberLessThanOrEqual(TNode<Number> lhs,
                                                       TNode<Number> rhs) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->NumberLessThanOrEqual(), lhs, rhs));
}

TNode<String> JSGraphAssembler::StringSubstring(TNode<String> string,
                                                TNode<Number> from,
                                                TNode<Number> to) {
  return AddNode<String>(graph()->NewNode(
      simplified()->StringSubstring(), string, from, to, effect(), control()));
}

TNode<Boolean> JSGraphAssembler::ObjectIsCallable(TNode<Object> value) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->ObjectIsCallable(), value));
}

TNode<Boolean> JSGraphAssembler::ObjectIsUndetectable(TNode<Object> value) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->ObjectIsUndetectable(), value));
}

Node* JSGraphAssembler::CheckIf(Node* cond, DeoptimizeReason reason) {
  return AddNode(graph()->NewNode(simplified()->CheckIf(reason), cond, effect(),
                                  control()));
}

TNode<Boolean> JSGraphAssembler::NumberIsFloat64Hole(TNode<Number> value) {
  return AddNode<Boolean>(
      graph()->NewNode(simplified()->NumberIsFloat64Hole(), value));
}

TNode<Boolean> JSGraphAssembler::ToBoolean(TNode<Object> value) {
  return AddNode<Boolean>(graph()->NewNode(simplified()->ToBoolean(), value));
}

TNode<Object> JSGraphAssembler::ConvertTaggedHoleToUndefined(
    TNode<Object> value) {
  return AddNode<Object>(
      graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(), value));
}

TNode<FixedArrayBase> JSGraphAssembler::MaybeGrowFastElements(
    ElementsKind kind, const FeedbackSource& feedback, TNode<JSArray> array,
    TNode<FixedArrayBase> elements, TNode<Number> new_length,
    TNode<Number> old_length) {
  GrowFastElementsMode mode = IsDoubleElementsKind(kind)
                                  ? GrowFastElementsMode::kDoubleElements
                                  : GrowFastElementsMode::kSmiOrObjectElements;
  return AddNode<FixedArrayBase>(graph()->NewNode(
      simplified()->MaybeGrowFastElements(mode, feedback), array, elements,
      new_length, old_length, effect(), control()));
}

Node* JSGraphAssembler::StringCharCodeAt(TNode<String> string,
                                         TNode<Number> position) {
  return AddNode(graph()->NewNode(simplified()->StringCharCodeAt(), string,
                                  position, effect(), control()));
}

Node* GraphAssembler::TypeGuard(Type type, Node* value) {
  return AddNode(
      graph()->NewNode(common()->TypeGuard(type), value, effect(), control()));
}

Node* GraphAssembler::Checkpoint(FrameState frame_state) {
  return AddNode(graph()->NewNode(common()->Checkpoint(), frame_state, effect(),
                                  control()));
}

Node* GraphAssembler::DebugBreak() {
  return AddNode(
      graph()->NewNode(machine()->DebugBreak(), effect(), control()));
}

Node* GraphAssembler::Unreachable(
    GraphAssemblerLabel<0u>* block_updater_successor) {
  Node* result = UnreachableWithoutConnectToEnd();
  ConnectUnreachableToEnd();
  InitializeEffectControl(nullptr, nullptr);
  return result;
}

Node* GraphAssembler::UnreachableWithoutConnectToEnd() {
  return AddNode(
      graph()->NewNode(common()->Unreachable(), effect(), control()));
}

TNode<RawPtrT> GraphAssembler::StackSlot(int size, int alignment) {
  return AddNode<RawPtrT>(
      graph()->NewNode(machine()->StackSlot(size, alignment)));
}

Node* GraphAssembler::Store(StoreRepresentation rep, Node* object, Node* offset,
                            Node* value) {
  return AddNode(graph()->NewNode(machine()->Store(rep), object, offset, value,
                                  effect(), control()));
}

Node* GraphAssembler::Store(StoreRepresentation rep, Node* object, int offset,
                            Node* value) {
  return Store(rep, object, Int32Constant(offset), value);
}

Node* GraphAssembler::Load(MachineType type, Node* object, Node* offset) {
  return AddNode(graph()->NewNode(machine()->Load(type), object, offset,
                                  effect(), control()));
}

Node* GraphAssembler::Load(MachineType type, Node* object, int offset) {
  return Load(type, object, Int32Constant(offset));
}

Node* GraphAssembler::StoreUnaligned(MachineRepresentation rep, Node* object,
                                     Node* offset, Node* value) {
  Operator const* const op =
      (rep == MachineRepresentation::kWord8 ||
       machine()->UnalignedStoreSupported(rep))
          ? machine()->Store(StoreRepresentation(rep, kNoWriteBarrier))
          : machine()->UnalignedStore(rep);
  return AddNode(
      graph()->NewNode(op, object, offset, value, effect(), control()));
}

Node* GraphAssembler::LoadUnaligned(MachineType type, Node* object,
                                    Node* offset) {
  Operator const* const op =
      (type.representation() == MachineRepresentation::kWord8 ||
       machine()->UnalignedLoadSupported(type.representation()))
          ? machine()->Load(type)
          : machine()->UnalignedLoad(type);
  return AddNode(graph()->NewNode(op, object, offset, effect(), control()));
}

Node* GraphAssembler::ProtectedStore(MachineRepresentation rep, Node* object,
                                     Node* offset, Node* value) {
  return AddNode(graph()->NewNode(machine()->ProtectedStore(rep), object,
                                  offset, value, effect(), control()));
}

Node* GraphAssembler::ProtectedLoad(MachineType type, Node* object,
                                    Node* offset) {
  return AddNode(graph()->NewNode(machine()->ProtectedLoad(type), object,
                                  offset, effect(), control()));
}

Node* GraphAssembler::Retain(Node* buffer) {
  return AddNode(graph()->NewNode(common()->Retain(), buffer, effect()));
}

Node* GraphAssembler::UnsafePointerAdd(Node* base, Node* external) {
  return AddNode(graph()->NewNode(machine()->UnsafePointerAdd(), base, external,
                                  effect(), control()));
}

TNode<Number> JSGraphAssembler::PlainPrimitiveToNumber(TNode<Object> value) {
  return AddNode<Number>(graph()->NewNode(
      PlainPrimitiveToNumberOperator(), PlainPrimitiveToNumberBuiltinConstant(),
      value, effect()));
}

Node* GraphAssembler::BitcastWordToTaggedSigned(Node* value) {
  return AddNode(
      graph()->NewNode(machine()->BitcastWordToTaggedSigned(), value));
}

Node* GraphAssembler::BitcastWordToTagged(Node* value) {
  return AddNode(graph()->NewNode(machine()->BitcastWordToTagged(), value,
                                  effect(), control()));
}

Node* GraphAssembler::BitcastTaggedToWord(Node* value) {
  return AddNode(graph()->NewNode(machine()->BitcastTaggedToWord(), value,
                                  effect(), control()));
}

Node* GraphAssembler::BitcastTaggedToWordForTagAndSmiBits(Node* value) {
  return AddNode(graph()->NewNode(
      machine()->BitcastTaggedToWordForTagAndSmiBits(), value));
}

Node* GraphAssembler::BitcastMaybeObjectToWord(Node* value) {
  return AddNode(graph()->NewNode(machine()->BitcastMaybeObjectToWord(), value,
                                  effect(), control()));
}

Node* GraphAssembler::DeoptimizeIf(DeoptimizeReason reason,
                                   FeedbackSource const& feedback,
                                   Node* condition, Node* frame_state) {
  return AddNode(graph()->NewNode(common()->DeoptimizeIf(reason, feedback),
                                  condition, frame_state, effect(), control()));
}

Node* GraphAssembler::DeoptimizeIfNot(DeoptimizeReason reason,
                                      FeedbackSource const& feedback,
                                      Node* condition, Node* frame_state) {
  return AddNode(graph()->NewNode(common()->DeoptimizeUnless(reason, feedback),
                                  condition, frame_state, effect(), control()));
}

TNode<Object> GraphAssembler::Call(const CallDescriptor* call_descriptor,
                                   int inputs_size, Node** inputs) {
  return Call(common()->Call(call_descriptor), inputs_size, inputs);
}

TNode<Object> GraphAssembler::Call(const Operator* op, int inputs_size,
                                   Node** inputs) {
  DCHECK_EQ(IrOpcode::kCall, op->opcode());
  return AddNode<Object>(graph()->NewNode(op, inputs_size, inputs));
}

void GraphAssembler::TailCall(const CallDescriptor* call_descriptor,
                              int inputs_size, Node** inputs) {
#ifdef DEBUG
  static constexpr int kTargetEffectControl = 3;
  DCHECK_EQ(inputs_size,
            call_descriptor->ParameterCount() + kTargetEffectControl);
#endif  // DEBUG

  Node* node = AddNode(graph()->NewNode(common()->TailCall(call_descriptor),
                                        inputs_size, inputs));

  // Unlike ConnectUnreachableToEnd, the TailCall node terminates a block; to
  // keep it live, it *must* be connected to End (also in Turboprop schedules).
  NodeProperties::MergeControlToEnd(graph(), common(), node);

  // Setting effect, control to nullptr effectively terminates the current block
  // by disallowing the addition of new nodes until a new label has been bound.
  InitializeEffectControl(nullptr, nullptr);
}

void GraphAssembler::BranchWithCriticalSafetyCheck(
    Node* condition, GraphAssemblerLabel<0u>* if_true,
    GraphAssemblerLabel<0u>* if_false) {
  BranchHint hint = BranchHint::kNone;
  if (if_true->IsDeferred() != if_false->IsDeferred()) {
    hint = if_false->IsDeferred() ? BranchHint::kTrue : BranchHint::kFalse;
  }

  BranchImpl(condition, if_true, if_false, hint);
}

void GraphAssembler::ConnectUnreachableToEnd() {
  DCHECK_EQ(effect()->opcode(), IrOpcode::kUnreachable);
  Node* throw_node = graph()->NewNode(common()->Throw(), effect(), control());
  NodeProperties::MergeControlToEnd(graph(), common(), throw_node);
  if (node_changed_callback_.has_value()) {
    (*node_changed_callback_)(graph()->end());
  }
  effect_ = control_ = mcgraph()->Dead();
}

Node* GraphAssembler::AddClonedNode(Node* node) {
  DCHECK(node->op()->HasProperty(Operator::kPure));
  UpdateEffectControlWith(node);
  return node;
}

Node* GraphAssembler::AddNode(Node* node) {
  if (!inline_reducers_.empty() && !inline_reductions_blocked_) {
    // Reducers may add new nodes to the graph using this graph assembler,
    // however they should never introduce nodes that need further reduction,
    // so block reduction
    BlockInlineReduction scope(this);
    Reduction reduction;
    for (auto reducer : inline_reducers_) {
      reduction = reducer->Reduce(node, nullptr);
      if (reduction.Changed()) break;
    }
    if (reduction.Changed()) {
      Node* replacement = reduction.replacement();
      if (replacement != node) {
        // Replace all uses of node and kill the node to make sure we don't
        // leave dangling dead uses.
        NodeProperties::ReplaceUses(node, replacement, effect(), control());
        node->Kill();
        return replacement;
      }
    }
  }

  if (node->opcode() == IrOpcode::kTerminate) {
    return node;
  }

  UpdateEffectControlWith(node);
  return node;
}

void GraphAssembler::Reset() {
  effect_ = nullptr;
  control_ = nullptr;
}

void GraphAssembler::InitializeEffectControl(Node* effect, Node* control) {
  effect_ = effect;
  control_ = control;
}

Operator const* JSGraphAssembler::PlainPrimitiveToNumberOperator() {
  if (!to_number_operator_.is_set()) {
    Callable callable =
        Builtins::CallableFor(isolate(), Builtin::kPlainPrimitiveToNumber);
    CallDescriptor::Flags flags = CallDescriptor::kNoFlags;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), callable.descriptor(),
        callable.descriptor().GetStackParameterCount(), flags,
        Operator::kEliminatable);
    to_number_operator_.set(common()->Call(call_descriptor));
  }
  return to_number_operator_.get();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
