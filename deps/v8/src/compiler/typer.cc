// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typer.h"

#include "src/base/flags.h"
#include "src/base/lazy-instance.h"
#include "src/bootstrapper.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

class TyperCache final {
 private:
  // This has to be first for the initialization magic to work.
  Zone zone_;

 public:
  TyperCache() = default;

  Type* const kInt8 =
      CreateNative(CreateRange<int8_t>(), Type::UntaggedSigned8());
  Type* const kUint8 =
      CreateNative(CreateRange<uint8_t>(), Type::UntaggedUnsigned8());
  Type* const kUint8Clamped = kUint8;
  Type* const kInt16 =
      CreateNative(CreateRange<int16_t>(), Type::UntaggedSigned16());
  Type* const kUint16 =
      CreateNative(CreateRange<uint16_t>(), Type::UntaggedUnsigned16());
  Type* const kInt32 = CreateNative(Type::Signed32(), Type::UntaggedSigned32());
  Type* const kUint32 =
      CreateNative(Type::Unsigned32(), Type::UntaggedUnsigned32());
  Type* const kFloat32 = CreateNative(Type::Number(), Type::UntaggedFloat32());
  Type* const kFloat64 = CreateNative(Type::Number(), Type::UntaggedFloat64());

  Type* const kSingletonZero = CreateRange(0.0, 0.0);
  Type* const kSingletonOne = CreateRange(1.0, 1.0);
  Type* const kZeroOrOne = CreateRange(0.0, 1.0);
  Type* const kZeroish =
      Type::Union(kSingletonZero, Type::MinusZeroOrNaN(), zone());
  Type* const kInteger = CreateRange(-V8_INFINITY, V8_INFINITY);
  Type* const kWeakint = Type::Union(kInteger, Type::MinusZeroOrNaN(), zone());
  Type* const kWeakintFunc1 = Type::Function(kWeakint, Type::Number(), zone());

  Type* const kRandomFunc0 = Type::Function(Type::OrderedNumber(), zone());
  Type* const kAnyFunc0 = Type::Function(Type::Any(), zone());
  Type* const kAnyFunc1 = Type::Function(Type::Any(), Type::Any(), zone());
  Type* const kAnyFunc2 =
      Type::Function(Type::Any(), Type::Any(), Type::Any(), zone());
  Type* const kAnyFunc3 = Type::Function(Type::Any(), Type::Any(), Type::Any(),
                                         Type::Any(), zone());
  Type* const kNumberFunc0 = Type::Function(Type::Number(), zone());
  Type* const kNumberFunc1 =
      Type::Function(Type::Number(), Type::Number(), zone());
  Type* const kNumberFunc2 =
      Type::Function(Type::Number(), Type::Number(), Type::Number(), zone());
  Type* const kImulFunc = Type::Function(Type::Signed32(), Type::Integral32(),
                                         Type::Integral32(), zone());
  Type* const kClz32Func =
      Type::Function(CreateRange(0, 32), Type::Number(), zone());

#define TYPED_ARRAY(TypeName, type_name, TYPE_NAME, ctype, size) \
  Type* const k##TypeName##Array = CreateArray(k##TypeName);
  TYPED_ARRAYS(TYPED_ARRAY)
#undef TYPED_ARRAY

 private:
  Type* CreateArray(Type* element) { return Type::Array(element, zone()); }

  Type* CreateArrayFunction(Type* array) {
    Type* arg1 = Type::Union(Type::Unsigned32(), Type::Object(), zone());
    Type* arg2 = Type::Union(Type::Unsigned32(), Type::Undefined(), zone());
    Type* arg3 = arg2;
    return Type::Function(array, arg1, arg2, arg3, zone());
  }

  Type* CreateNative(Type* semantic, Type* representation) {
    return Type::Intersect(semantic, representation, zone());
  }

  template <typename T>
  Type* CreateRange() {
    return CreateRange(std::numeric_limits<T>::min(),
                       std::numeric_limits<T>::max());
  }

  Type* CreateRange(double min, double max) {
    return Type::Range(min, max, zone());
  }

  Zone* zone() { return &zone_; }
};


namespace {

base::LazyInstance<TyperCache>::type kCache = LAZY_INSTANCE_INITIALIZER;

}  // namespace


class Typer::Decorator final : public GraphDecorator {
 public:
  explicit Decorator(Typer* typer) : typer_(typer) {}
  void Decorate(Node* node) final;

 private:
  Typer* const typer_;
};


Typer::Typer(Isolate* isolate, Graph* graph, Type::FunctionType* function_type)
    : isolate_(isolate),
      graph_(graph),
      function_type_(function_type),
      decorator_(nullptr),
      cache_(kCache.Get()) {
  Zone* zone = this->zone();
  Factory* const factory = isolate->factory();

  Type* infinity = Type::Constant(factory->infinity_value(), zone);
  Type* minus_infinity = Type::Constant(factory->minus_infinity_value(), zone);
  Type* truncating_to_zero =
      Type::Union(Type::Union(infinity, minus_infinity, zone),
                  Type::MinusZeroOrNaN(), zone);

  singleton_false_ = Type::Constant(factory->false_value(), zone);
  singleton_true_ = Type::Constant(factory->true_value(), zone);
  signed32ish_ = Type::Union(Type::Signed32(), truncating_to_zero, zone);
  unsigned32ish_ = Type::Union(Type::Unsigned32(), truncating_to_zero, zone);
  falsish_ = Type::Union(
      Type::Undetectable(),
      Type::Union(Type::Union(singleton_false_, cache_.kZeroish, zone),
                  Type::NullOrUndefined(), zone),
      zone);
  truish_ = Type::Union(
      singleton_true_,
      Type::Union(Type::DetectableReceiver(), Type::Symbol(), zone), zone);

  decorator_ = new (zone) Decorator(this);
  graph_->AddDecorator(decorator_);
}


Typer::~Typer() {
  graph_->RemoveDecorator(decorator_);
}


class Typer::Visitor : public Reducer {
 public:
  explicit Visitor(Typer* typer)
      : typer_(typer), weakened_nodes_(typer->zone()) {}

  Reduction Reduce(Node* node) override {
    if (node->op()->ValueOutputCount() == 0) return NoChange();
    switch (node->opcode()) {
#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return UpdateBounds(node, TypeBinaryOp(node, x##Typer));
      JS_SIMPLE_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return UpdateBounds(node, Type##x(node));
      DECLARE_CASE(Start)
      DECLARE_CASE(IfException)
      // VALUE_OP_LIST without JS_SIMPLE_BINOP_LIST:
      COMMON_OP_LIST(DECLARE_CASE)
      SIMPLIFIED_OP_LIST(DECLARE_CASE)
      MACHINE_OP_LIST(DECLARE_CASE)
      JS_SIMPLE_UNOP_LIST(DECLARE_CASE)
      JS_OBJECT_OP_LIST(DECLARE_CASE)
      JS_CONTEXT_OP_LIST(DECLARE_CASE)
      JS_OTHER_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) case IrOpcode::k##x:
      DECLARE_CASE(Loop)
      DECLARE_CASE(Branch)
      DECLARE_CASE(IfTrue)
      DECLARE_CASE(IfFalse)
      DECLARE_CASE(IfSuccess)
      DECLARE_CASE(Switch)
      DECLARE_CASE(IfValue)
      DECLARE_CASE(IfDefault)
      DECLARE_CASE(Merge)
      DECLARE_CASE(Deoptimize)
      DECLARE_CASE(Return)
      DECLARE_CASE(TailCall)
      DECLARE_CASE(Terminate)
      DECLARE_CASE(OsrNormalEntry)
      DECLARE_CASE(OsrLoopEntry)
      DECLARE_CASE(Throw)
      DECLARE_CASE(End)
#undef DECLARE_CASE
      break;
    }
    return NoChange();
  }

  Bounds TypeNode(Node* node) {
    switch (node->opcode()) {
#define DECLARE_CASE(x) \
      case IrOpcode::k##x: return TypeBinaryOp(node, x##Typer);
      JS_SIMPLE_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) case IrOpcode::k##x: return Type##x(node);
      DECLARE_CASE(Start)
      DECLARE_CASE(IfException)
      // VALUE_OP_LIST without JS_SIMPLE_BINOP_LIST:
      COMMON_OP_LIST(DECLARE_CASE)
      SIMPLIFIED_OP_LIST(DECLARE_CASE)
      MACHINE_OP_LIST(DECLARE_CASE)
      JS_SIMPLE_UNOP_LIST(DECLARE_CASE)
      JS_OBJECT_OP_LIST(DECLARE_CASE)
      JS_CONTEXT_OP_LIST(DECLARE_CASE)
      JS_OTHER_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) case IrOpcode::k##x:
      DECLARE_CASE(Loop)
      DECLARE_CASE(Branch)
      DECLARE_CASE(IfTrue)
      DECLARE_CASE(IfFalse)
      DECLARE_CASE(IfSuccess)
      DECLARE_CASE(Switch)
      DECLARE_CASE(IfValue)
      DECLARE_CASE(IfDefault)
      DECLARE_CASE(Merge)
      DECLARE_CASE(Deoptimize)
      DECLARE_CASE(Return)
      DECLARE_CASE(TailCall)
      DECLARE_CASE(Terminate)
      DECLARE_CASE(OsrNormalEntry)
      DECLARE_CASE(OsrLoopEntry)
      DECLARE_CASE(Throw)
      DECLARE_CASE(End)
#undef DECLARE_CASE
      break;
    }
    UNREACHABLE();
    return Bounds();
  }

  Type* TypeConstant(Handle<Object> value);

 private:
  Typer* typer_;
  ZoneSet<NodeId> weakened_nodes_;

#define DECLARE_METHOD(x) inline Bounds Type##x(Node* node);
  DECLARE_METHOD(Start)
  DECLARE_METHOD(IfException)
  VALUE_OP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  Bounds BoundsOrNone(Node* node) {
    return NodeProperties::IsTyped(node) ? NodeProperties::GetBounds(node)
                                         : Bounds(Type::None());
  }

  Bounds Operand(Node* node, int i) {
    Node* operand_node = NodeProperties::GetValueInput(node, i);
    return BoundsOrNone(operand_node);
  }

  Bounds WrapContextBoundsForInput(Node* node);
  Type* Weaken(Node* node, Type* current_type, Type* previous_type);

  Zone* zone() { return typer_->zone(); }
  Isolate* isolate() { return typer_->isolate(); }
  Graph* graph() { return typer_->graph(); }

  void SetWeakened(NodeId node_id) { weakened_nodes_.insert(node_id); }
  bool IsWeakened(NodeId node_id) {
    return weakened_nodes_.find(node_id) != weakened_nodes_.end();
  }

  typedef Type* (*UnaryTyperFun)(Type*, Typer* t);
  typedef Type* (*BinaryTyperFun)(Type*, Type*, Typer* t);

  Bounds TypeUnaryOp(Node* node, UnaryTyperFun);
  Bounds TypeBinaryOp(Node* node, BinaryTyperFun);

  enum ComparisonOutcomeFlags {
    kComparisonTrue = 1,
    kComparisonFalse = 2,
    kComparisonUndefined = 4
  };
  typedef base::Flags<ComparisonOutcomeFlags> ComparisonOutcome;

  static ComparisonOutcome Invert(ComparisonOutcome, Typer*);
  static Type* Invert(Type*, Typer*);
  static Type* FalsifyUndefined(ComparisonOutcome, Typer*);
  static Type* Rangify(Type*, Typer*);

  static Type* ToPrimitive(Type*, Typer*);
  static Type* ToBoolean(Type*, Typer*);
  static Type* ToNumber(Type*, Typer*);
  static Type* ToString(Type*, Typer*);
  static Type* NumberToInt32(Type*, Typer*);
  static Type* NumberToUint32(Type*, Typer*);

  static Type* JSAddRanger(Type::RangeType*, Type::RangeType*, Typer*);
  static Type* JSSubtractRanger(Type::RangeType*, Type::RangeType*, Typer*);
  static Type* JSMultiplyRanger(Type::RangeType*, Type::RangeType*, Typer*);
  static Type* JSDivideRanger(Type::RangeType*, Type::RangeType*, Typer*);
  static Type* JSModulusRanger(Type::RangeType*, Type::RangeType*, Typer*);

  static ComparisonOutcome JSCompareTyper(Type*, Type*, Typer*);

#define DECLARE_METHOD(x) static Type* x##Typer(Type*, Type*, Typer*);
  JS_SIMPLE_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  static Type* JSUnaryNotTyper(Type*, Typer*);
  static Type* JSTypeOfTyper(Type*, Typer*);
  static Type* JSLoadPropertyTyper(Type*, Type*, Typer*);
  static Type* JSCallFunctionTyper(Type*, Typer*);

  Reduction UpdateBounds(Node* node, Bounds current) {
    if (NodeProperties::IsTyped(node)) {
      // Widen the bounds of a previously typed node.
      Bounds previous = NodeProperties::GetBounds(node);
      if (node->opcode() == IrOpcode::kPhi) {
        // Speed up termination in the presence of range types:
        current.upper = Weaken(node, current.upper, previous.upper);
        current.lower = Weaken(node, current.lower, previous.lower);
      }

      // Types should not get less precise.
      DCHECK(previous.lower->Is(current.lower));
      DCHECK(previous.upper->Is(current.upper));

      NodeProperties::SetBounds(node, current);
      if (!(previous.Narrows(current) && current.Narrows(previous))) {
        // If something changed, revisit all uses.
        return Changed(node);
      }
      return NoChange();
    } else {
      // No previous type, simply update the bounds.
      NodeProperties::SetBounds(node, current);
      return Changed(node);
    }
  }
};


void Typer::Run() { Run(NodeVector(zone())); }


void Typer::Run(const NodeVector& roots) {
  Visitor visitor(this);
  GraphReducer graph_reducer(zone(), graph());
  graph_reducer.AddReducer(&visitor);
  for (Node* const root : roots) graph_reducer.ReduceNode(root);
  graph_reducer.ReduceGraph();
}


void Typer::Decorator::Decorate(Node* node) {
  if (node->op()->ValueOutputCount() > 0) {
    // Only eagerly type-decorate nodes with known input types.
    // Other cases will generally require a proper fixpoint iteration with Run.
    bool is_typed = NodeProperties::IsTyped(node);
    if (is_typed || NodeProperties::AllValueInputsAreTyped(node)) {
      Visitor typing(typer_);
      Bounds bounds = typing.TypeNode(node);
      if (is_typed) {
        bounds =
          Bounds::Both(bounds, NodeProperties::GetBounds(node), typer_->zone());
      }
      NodeProperties::SetBounds(node, bounds);
    }
  }
}


// -----------------------------------------------------------------------------

// Helper functions that lift a function f on types to a function on bounds,
// and uses that to type the given node.  Note that f is never called with None
// as an argument.


Bounds Typer::Visitor::TypeUnaryOp(Node* node, UnaryTyperFun f) {
  Bounds input = Operand(node, 0);
  Type* upper =
      input.upper->IsInhabited() ? f(input.upper, typer_) : Type::None();
  Type* lower = input.lower->IsInhabited()
                    ? ((input.lower == input.upper || upper->IsConstant())
                           ? upper  // TODO(neis): Extend this to Range(x,x),
                                    // NaN, MinusZero, ...?
                           : f(input.lower, typer_))
                    : Type::None();
  // TODO(neis): Figure out what to do with lower bound.
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeBinaryOp(Node* node, BinaryTyperFun f) {
  Bounds left = Operand(node, 0);
  Bounds right = Operand(node, 1);
  Type* upper = left.upper->IsInhabited() && right.upper->IsInhabited()
                    ? f(left.upper, right.upper, typer_)
                    : Type::None();
  Type* lower =
      left.lower->IsInhabited() && right.lower->IsInhabited()
          ? (((left.lower == left.upper && right.lower == right.upper) ||
              upper->IsConstant())
                 ? upper
                 : f(left.lower, right.lower, typer_))
          : Type::None();
  // TODO(neis): Figure out what to do with lower bound.
  return Bounds(lower, upper);
}


Type* Typer::Visitor::Invert(Type* type, Typer* t) {
  DCHECK(type->Is(Type::Boolean()));
  DCHECK(type->IsInhabited());
  if (type->Is(t->singleton_false_)) return t->singleton_true_;
  if (type->Is(t->singleton_true_)) return t->singleton_false_;
  return type;
}


Typer::Visitor::ComparisonOutcome Typer::Visitor::Invert(
    ComparisonOutcome outcome, Typer* t) {
  ComparisonOutcome result(0);
  if ((outcome & kComparisonUndefined) != 0) result |= kComparisonUndefined;
  if ((outcome & kComparisonTrue) != 0) result |= kComparisonFalse;
  if ((outcome & kComparisonFalse) != 0) result |= kComparisonTrue;
  return result;
}


Type* Typer::Visitor::FalsifyUndefined(ComparisonOutcome outcome, Typer* t) {
  if ((outcome & kComparisonFalse) != 0 ||
      (outcome & kComparisonUndefined) != 0) {
    return (outcome & kComparisonTrue) != 0 ? Type::Boolean()
                                            : t->singleton_false_;
  }
  // Type should be non empty, so we know it should be true.
  DCHECK((outcome & kComparisonTrue) != 0);
  return t->singleton_true_;
}


Type* Typer::Visitor::Rangify(Type* type, Typer* t) {
  if (type->IsRange()) return type;        // Shortcut.
  if (!type->Is(t->cache_.kInteger)) {
    return type;  // Give up on non-integer types.
  }
  double min = type->Min();
  double max = type->Max();
  // Handle the degenerate case of empty bitset types (such as
  // OtherUnsigned31 and OtherSigned32 on 64-bit architectures).
  if (std::isnan(min)) {
    DCHECK(std::isnan(max));
    return type;
  }
  return Type::Range(min, max, t->zone());
}


// Type conversion.


Type* Typer::Visitor::ToPrimitive(Type* type, Typer* t) {
  if (type->Is(Type::Primitive()) && !type->Maybe(Type::Receiver())) {
    return type;
  }
  return Type::Primitive();
}


Type* Typer::Visitor::ToBoolean(Type* type, Typer* t) {
  if (type->Is(Type::Boolean())) return type;
  if (type->Is(t->falsish_)) return t->singleton_false_;
  if (type->Is(t->truish_)) return t->singleton_true_;
  if (type->Is(Type::PlainNumber()) && (type->Max() < 0 || 0 < type->Min())) {
    return t->singleton_true_;  // Ruled out nan, -0 and +0.
  }
  return Type::Boolean();
}


Type* Typer::Visitor::ToNumber(Type* type, Typer* t) {
  if (type->Is(Type::Number())) return type;
  if (type->Is(Type::NullOrUndefined())) {
    if (type->Is(Type::Null())) return t->cache_.kSingletonZero;
    if (type->Is(Type::Undefined())) return Type::NaN();
    return Type::Union(Type::NaN(), t->cache_.kSingletonZero, t->zone());
  }
  if (type->Is(Type::NumberOrUndefined())) {
    return Type::Union(Type::Intersect(type, Type::Number(), t->zone()),
                       Type::NaN(), t->zone());
  }
  if (type->Is(t->singleton_false_)) return t->cache_.kSingletonZero;
  if (type->Is(t->singleton_true_)) return t->cache_.kSingletonOne;
  if (type->Is(Type::Boolean())) return t->cache_.kZeroOrOne;
  if (type->Is(Type::BooleanOrNumber())) {
    return Type::Union(Type::Intersect(type, Type::Number(), t->zone()),
                       t->cache_.kZeroOrOne, t->zone());
  }
  return Type::Number();
}


Type* Typer::Visitor::ToString(Type* type, Typer* t) {
  if (type->Is(Type::String())) return type;
  return Type::String();
}


Type* Typer::Visitor::NumberToInt32(Type* type, Typer* t) {
  // TODO(neis): DCHECK(type->Is(Type::Number()));
  if (type->Is(Type::Signed32())) return type;
  if (type->Is(t->cache_.kZeroish)) return t->cache_.kSingletonZero;
  if (type->Is(t->signed32ish_)) {
    return Type::Intersect(
        Type::Union(type, t->cache_.kSingletonZero, t->zone()),
        Type::Signed32(), t->zone());
  }
  return Type::Signed32();
}


Type* Typer::Visitor::NumberToUint32(Type* type, Typer* t) {
  // TODO(neis): DCHECK(type->Is(Type::Number()));
  if (type->Is(Type::Unsigned32())) return type;
  if (type->Is(t->cache_.kZeroish)) return t->cache_.kSingletonZero;
  if (type->Is(t->unsigned32ish_)) {
    return Type::Intersect(
        Type::Union(type, t->cache_.kSingletonZero, t->zone()),
        Type::Unsigned32(), t->zone());
  }
  return Type::Unsigned32();
}


// -----------------------------------------------------------------------------


// Control operators.


Bounds Typer::Visitor::TypeStart(Node* node) {
  return Bounds(Type::None(zone()), Type::Internal(zone()));
}


Bounds Typer::Visitor::TypeIfException(Node* node) {
  return Bounds::Unbounded(zone());
}


// Common operators.


Bounds Typer::Visitor::TypeParameter(Node* node) {
  int param = OpParameter<int>(node);
  Type::FunctionType* function_type = typer_->function_type();
  if (function_type != nullptr && param >= 0 &&
      param < static_cast<int>(function_type->Arity())) {
    return Bounds(Type::None(), function_type->Parameter(param));
  }
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeOsrValue(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeInt32Constant(Node* node) {
  double number = OpParameter<int32_t>(node);
  return Bounds(Type::Intersect(
      Type::Range(number, number, zone()), Type::UntaggedSigned32(), zone()));
}


Bounds Typer::Visitor::TypeInt64Constant(Node* node) {
  // TODO(rossberg): This actually seems to be a PointerConstant so far...
  return Bounds(Type::Internal());  // TODO(rossberg): Add int64 bitset type?
}


Bounds Typer::Visitor::TypeFloat32Constant(Node* node) {
  return Bounds(Type::Intersect(
      Type::Of(OpParameter<float>(node), zone()),
      Type::UntaggedFloat32(), zone()));
}


Bounds Typer::Visitor::TypeFloat64Constant(Node* node) {
  return Bounds(Type::Intersect(
      Type::Of(OpParameter<double>(node), zone()),
      Type::UntaggedFloat64(), zone()));
}


Bounds Typer::Visitor::TypeNumberConstant(Node* node) {
  Factory* f = isolate()->factory();
  return Bounds(Type::Constant(
      f->NewNumber(OpParameter<double>(node)), zone()));
}


Bounds Typer::Visitor::TypeHeapConstant(Node* node) {
  return Bounds(TypeConstant(OpParameter<Unique<HeapObject> >(node).handle()));
}


Bounds Typer::Visitor::TypeExternalConstant(Node* node) {
  return Bounds(Type::None(zone()), Type::Internal(zone()));
}


Bounds Typer::Visitor::TypeSelect(Node* node) {
  return Bounds::Either(Operand(node, 1), Operand(node, 2), zone());
}


Bounds Typer::Visitor::TypePhi(Node* node) {
  int arity = node->op()->ValueInputCount();
  Bounds bounds = Operand(node, 0);
  for (int i = 1; i < arity; ++i) {
    bounds = Bounds::Either(bounds, Operand(node, i), zone());
  }
  return bounds;
}


Bounds Typer::Visitor::TypeEffectPhi(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeEffectSet(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeValueEffect(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeFinish(Node* node) {
  return Operand(node, 0);
}


Bounds Typer::Visitor::TypeFrameState(Node* node) {
  // TODO(rossberg): Ideally FrameState wouldn't have a value output.
  return Bounds(Type::None(zone()), Type::Internal(zone()));
}


Bounds Typer::Visitor::TypeStateValues(Node* node) {
  return Bounds(Type::None(zone()), Type::Internal(zone()));
}


Bounds Typer::Visitor::TypeTypedStateValues(Node* node) {
  return Bounds(Type::None(zone()), Type::Internal(zone()));
}


Bounds Typer::Visitor::TypeCall(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeProjection(Node* node) {
  // TODO(titzer): use the output type of the input to determine the bounds.
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeDead(Node* node) {
  return Bounds::Unbounded(zone());
}


// JS comparison operators.


Type* Typer::Visitor::JSEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return t->singleton_false_;
  if (lhs->Is(Type::NullOrUndefined()) && rhs->Is(Type::NullOrUndefined())) {
    return t->singleton_true_;
  }
  if (lhs->Is(Type::Number()) && rhs->Is(Type::Number()) &&
      (lhs->Max() < rhs->Min() || lhs->Min() > rhs->Max())) {
    return t->singleton_false_;
  }
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value,
    // which is not nan due to the earlier check.
    // TODO(neis): Extend this to Range(x,x), MinusZero, ...?
    return t->singleton_true_;
  }
  return Type::Boolean();
}


Type* Typer::Visitor::JSNotEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  return Invert(JSEqualTyper(lhs, rhs, t), t);
}


static Type* JSType(Type* type) {
  if (type->Is(Type::Boolean())) return Type::Boolean();
  if (type->Is(Type::String())) return Type::String();
  if (type->Is(Type::Number())) return Type::Number();
  if (type->Is(Type::Undefined())) return Type::Undefined();
  if (type->Is(Type::Null())) return Type::Null();
  if (type->Is(Type::Symbol())) return Type::Symbol();
  if (type->Is(Type::Receiver())) return Type::Receiver();  // JS "Object"
  return Type::Any();
}


Type* Typer::Visitor::JSStrictEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  if (!JSType(lhs)->Maybe(JSType(rhs))) return t->singleton_false_;
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return t->singleton_false_;
  if (lhs->Is(Type::Number()) && rhs->Is(Type::Number()) &&
      (lhs->Max() < rhs->Min() || lhs->Min() > rhs->Max())) {
    return t->singleton_false_;
  }
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value,
    // which is not nan due to the earlier check.
    return t->singleton_true_;
  }
  return Type::Boolean();
}


Type* Typer::Visitor::JSStrictNotEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  return Invert(JSStrictEqualTyper(lhs, rhs, t), t);
}


// The EcmaScript specification defines the four relational comparison operators
// (<, <=, >=, >) with the help of a single abstract one.  It behaves like <
// but returns undefined when the inputs cannot be compared.
// We implement the typing analogously.
Typer::Visitor::ComparisonOutcome Typer::Visitor::JSCompareTyper(Type* lhs,
                                                                 Type* rhs,
                                                                 Typer* t) {
  lhs = ToPrimitive(lhs, t);
  rhs = ToPrimitive(rhs, t);
  if (lhs->Maybe(Type::String()) && rhs->Maybe(Type::String())) {
    return ComparisonOutcome(kComparisonTrue) |
           ComparisonOutcome(kComparisonFalse);
  }
  lhs = ToNumber(lhs, t);
  rhs = ToNumber(rhs, t);

  // Shortcut for NaNs.
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return kComparisonUndefined;

  ComparisonOutcome result;
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value.
    result = kComparisonFalse;
  } else if (lhs->Min() >= rhs->Max()) {
    result = kComparisonFalse;
  } else if (lhs->Max() < rhs->Min()) {
    result = kComparisonTrue;
  } else {
    // We cannot figure out the result, return both true and false. (We do not
    // have to return undefined because that cannot affect the result of
    // FalsifyUndefined.)
    return ComparisonOutcome(kComparisonTrue) |
           ComparisonOutcome(kComparisonFalse);
  }
  // Add the undefined if we could see NaN.
  if (lhs->Maybe(Type::NaN()) || rhs->Maybe(Type::NaN())) {
    result |= kComparisonUndefined;
  }
  return result;
}


Type* Typer::Visitor::JSLessThanTyper(Type* lhs, Type* rhs, Typer* t) {
  return FalsifyUndefined(JSCompareTyper(lhs, rhs, t), t);
}


Type* Typer::Visitor::JSGreaterThanTyper(Type* lhs, Type* rhs, Typer* t) {
  return FalsifyUndefined(JSCompareTyper(rhs, lhs, t), t);
}


Type* Typer::Visitor::JSLessThanOrEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  return FalsifyUndefined(Invert(JSCompareTyper(rhs, lhs, t), t), t);
}


Type* Typer::Visitor::JSGreaterThanOrEqualTyper(
    Type* lhs, Type* rhs, Typer* t) {
  return FalsifyUndefined(Invert(JSCompareTyper(lhs, rhs, t), t), t);
}


// JS bitwise operators.


Type* Typer::Visitor::JSBitwiseOrTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  rhs = NumberToInt32(ToNumber(rhs, t), t);
  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  // Or-ing any two values results in a value no smaller than their minimum.
  // Even no smaller than their maximum if both values are non-negative.
  double min =
      lmin >= 0 && rmin >= 0 ? std::max(lmin, rmin) : std::min(lmin, rmin);
  double max = Type::Signed32()->Max();

  // Or-ing with 0 is essentially a conversion to int32.
  if (rmin == 0 && rmax == 0) {
    min = lmin;
    max = lmax;
  }
  if (lmin == 0 && lmax == 0) {
    min = rmin;
    max = rmax;
  }

  if (lmax < 0 || rmax < 0) {
    // Or-ing two values of which at least one is negative results in a negative
    // value.
    max = std::min(max, -1.0);
  }
  return Type::Range(min, max, t->zone());
  // TODO(neis): Be precise for singleton inputs, here and elsewhere.
}


Type* Typer::Visitor::JSBitwiseAndTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  rhs = NumberToInt32(ToNumber(rhs, t), t);
  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  double min = Type::Signed32()->Min();
  // And-ing any two values results in a value no larger than their maximum.
  // Even no larger than their minimum if both values are non-negative.
  double max =
      lmin >= 0 && rmin >= 0 ? std::min(lmax, rmax) : std::max(lmax, rmax);
  // And-ing with a non-negative value x causes the result to be between
  // zero and x.
  if (lmin >= 0) {
    min = 0;
    max = std::min(max, lmax);
  }
  if (rmin >= 0) {
    min = 0;
    max = std::min(max, rmax);
  }
  return Type::Range(min, max, t->zone());
}


Type* Typer::Visitor::JSBitwiseXorTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  rhs = NumberToInt32(ToNumber(rhs, t), t);
  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  if ((lmin >= 0 && rmin >= 0) || (lmax < 0 && rmax < 0)) {
    // Xor-ing negative or non-negative values results in a non-negative value.
    return Type::Unsigned31();
  }
  if ((lmax < 0 && rmin >= 0) || (lmin >= 0 && rmax < 0)) {
    // Xor-ing a negative and a non-negative value results in a negative value.
    // TODO(jarin) Use a range here.
    return Type::Negative32();
  }
  return Type::Signed32();
}


Type* Typer::Visitor::JSShiftLeftTyper(Type* lhs, Type* rhs, Typer* t) {
  return Type::Signed32();
}


Type* Typer::Visitor::JSShiftRightTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  rhs = NumberToUint32(ToNumber(rhs, t), t);
  double min = kMinInt;
  double max = kMaxInt;
  if (lhs->Min() >= 0) {
    // Right-shifting a non-negative value cannot make it negative, nor larger.
    min = std::max(min, 0.0);
    max = std::min(max, lhs->Max());
    if (rhs->Min() > 0 && rhs->Max() <= 31) {
      max = static_cast<int>(max) >> static_cast<int>(rhs->Min());
    }
  }
  if (lhs->Max() < 0) {
    // Right-shifting a negative value cannot make it non-negative, nor smaller.
    min = std::max(min, lhs->Min());
    max = std::min(max, -1.0);
    if (rhs->Min() > 0 && rhs->Max() <= 31) {
      min = static_cast<int>(min) >> static_cast<int>(rhs->Min());
    }
  }
  if (rhs->Min() > 0 && rhs->Max() <= 31) {
    // Right-shifting by a positive value yields a small integer value.
    double shift_min = kMinInt >> static_cast<int>(rhs->Min());
    double shift_max = kMaxInt >> static_cast<int>(rhs->Min());
    min = std::max(min, shift_min);
    max = std::min(max, shift_max);
  }
  // TODO(jarin) Ideally, the following micro-optimization should be performed
  // by the type constructor.
  if (max != Type::Signed32()->Max() || min != Type::Signed32()->Min()) {
    return Type::Range(min, max, t->zone());
  }
  return Type::Signed32();
}


Type* Typer::Visitor::JSShiftRightLogicalTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToUint32(ToNumber(lhs, t), t);
  // Logical right-shifting any value cannot make it larger.
  return Type::Range(0.0, lhs->Max(), t->zone());
}


// JS arithmetic operators.


// Returns the array's least element, ignoring NaN.
// There must be at least one non-NaN element.
// Any -0 is converted to 0.
static double array_min(double a[], size_t n) {
  DCHECK(n != 0);
  double x = +V8_INFINITY;
  for (size_t i = 0; i < n; ++i) {
    if (!std::isnan(a[i])) {
      x = std::min(a[i], x);
    }
  }
  DCHECK(!std::isnan(x));
  return x == 0 ? 0 : x;  // -0 -> 0
}


// Returns the array's greatest element, ignoring NaN.
// There must be at least one non-NaN element.
// Any -0 is converted to 0.
static double array_max(double a[], size_t n) {
  DCHECK(n != 0);
  double x = -V8_INFINITY;
  for (size_t i = 0; i < n; ++i) {
    if (!std::isnan(a[i])) {
      x = std::max(a[i], x);
    }
  }
  DCHECK(!std::isnan(x));
  return x == 0 ? 0 : x;  // -0 -> 0
}


Type* Typer::Visitor::JSAddRanger(Type::RangeType* lhs, Type::RangeType* rhs,
                                  Typer* t) {
  double results[4];
  results[0] = lhs->Min() + rhs->Min();
  results[1] = lhs->Min() + rhs->Max();
  results[2] = lhs->Max() + rhs->Min();
  results[3] = lhs->Max() + rhs->Max();
  // Since none of the inputs can be -0, the result cannot be -0 either.
  // However, it can be nan (the sum of two infinities of opposite sign).
  // On the other hand, if none of the "results" above is nan, then the actual
  // result cannot be nan either.
  int nans = 0;
  for (int i = 0; i < 4; ++i) {
    if (std::isnan(results[i])) ++nans;
  }
  if (nans == 4) return Type::NaN();  // [-inf..-inf] + [inf..inf] or vice versa
  Type* range =
      Type::Range(array_min(results, 4), array_max(results, 4), t->zone());
  return nans == 0 ? range : Type::Union(range, Type::NaN(), t->zone());
  // Examples:
  //   [-inf, -inf] + [+inf, +inf] = NaN
  //   [-inf, -inf] + [n, +inf] = [-inf, -inf] \/ NaN
  //   [-inf, +inf] + [n, +inf] = [-inf, +inf] \/ NaN
  //   [-inf, m] + [n, +inf] = [-inf, +inf] \/ NaN
}


Type* Typer::Visitor::JSAddTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = ToPrimitive(lhs, t);
  rhs = ToPrimitive(rhs, t);
  if (lhs->Maybe(Type::String()) || rhs->Maybe(Type::String())) {
    if (lhs->Is(Type::String()) || rhs->Is(Type::String())) {
      return Type::String();
    } else {
      return Type::NumberOrString();
    }
  }
  lhs = Rangify(ToNumber(lhs, t), t);
  rhs = Rangify(ToNumber(rhs, t), t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  if (lhs->IsRange() && rhs->IsRange()) {
    return JSAddRanger(lhs->AsRange(), rhs->AsRange(), t);
  }
  // TODO(neis): Deal with numeric bitsets here and elsewhere.
  return Type::Number();
}


Type* Typer::Visitor::JSSubtractRanger(Type::RangeType* lhs,
                                       Type::RangeType* rhs, Typer* t) {
  double results[4];
  results[0] = lhs->Min() - rhs->Min();
  results[1] = lhs->Min() - rhs->Max();
  results[2] = lhs->Max() - rhs->Min();
  results[3] = lhs->Max() - rhs->Max();
  // Since none of the inputs can be -0, the result cannot be -0.
  // However, it can be nan (the subtraction of two infinities of same sign).
  // On the other hand, if none of the "results" above is nan, then the actual
  // result cannot be nan either.
  int nans = 0;
  for (int i = 0; i < 4; ++i) {
    if (std::isnan(results[i])) ++nans;
  }
  if (nans == 4) return Type::NaN();  // [inf..inf] - [inf..inf] (all same sign)
  Type* range =
      Type::Range(array_min(results, 4), array_max(results, 4), t->zone());
  return nans == 0 ? range : Type::Union(range, Type::NaN(), t->zone());
  // Examples:
  //   [-inf, +inf] - [-inf, +inf] = [-inf, +inf] \/ NaN
  //   [-inf, -inf] - [-inf, -inf] = NaN
  //   [-inf, -inf] - [n, +inf] = [-inf, -inf] \/ NaN
  //   [m, +inf] - [-inf, n] = [-inf, +inf] \/ NaN
}


Type* Typer::Visitor::JSSubtractTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = Rangify(ToNumber(lhs, t), t);
  rhs = Rangify(ToNumber(rhs, t), t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  if (lhs->IsRange() && rhs->IsRange()) {
    return JSSubtractRanger(lhs->AsRange(), rhs->AsRange(), t);
  }
  return Type::Number();
}


Type* Typer::Visitor::JSMultiplyRanger(Type::RangeType* lhs,
                                       Type::RangeType* rhs, Typer* t) {
  double results[4];
  double lmin = lhs->Min();
  double lmax = lhs->Max();
  double rmin = rhs->Min();
  double rmax = rhs->Max();
  results[0] = lmin * rmin;
  results[1] = lmin * rmax;
  results[2] = lmax * rmin;
  results[3] = lmax * rmax;
  // If the result may be nan, we give up on calculating a precise type, because
  // the discontinuity makes it too complicated.  Note that even if none of the
  // "results" above is nan, the actual result may still be, so we have to do a
  // different check:
  bool maybe_nan = (lhs->Maybe(t->cache_.kSingletonZero) &&
                    (rmin == -V8_INFINITY || rmax == +V8_INFINITY)) ||
                   (rhs->Maybe(t->cache_.kSingletonZero) &&
                    (lmin == -V8_INFINITY || lmax == +V8_INFINITY));
  if (maybe_nan) return t->cache_.kWeakint;  // Giving up.
  bool maybe_minuszero = (lhs->Maybe(t->cache_.kSingletonZero) && rmin < 0) ||
                         (rhs->Maybe(t->cache_.kSingletonZero) && lmin < 0);
  Type* range =
      Type::Range(array_min(results, 4), array_max(results, 4), t->zone());
  return maybe_minuszero ? Type::Union(range, Type::MinusZero(), t->zone())
                         : range;
}


Type* Typer::Visitor::JSMultiplyTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = Rangify(ToNumber(lhs, t), t);
  rhs = Rangify(ToNumber(rhs, t), t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  if (lhs->IsRange() && rhs->IsRange()) {
    return JSMultiplyRanger(lhs->AsRange(), rhs->AsRange(), t);
  }
  return Type::Number();
}


Type* Typer::Visitor::JSDivideTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = ToNumber(lhs, t);
  rhs = ToNumber(rhs, t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  // Division is tricky, so all we do is try ruling out nan.
  // TODO(neis): try ruling out -0 as well?
  bool maybe_nan =
      lhs->Maybe(Type::NaN()) || rhs->Maybe(t->cache_.kZeroish) ||
      ((lhs->Min() == -V8_INFINITY || lhs->Max() == +V8_INFINITY) &&
       (rhs->Min() == -V8_INFINITY || rhs->Max() == +V8_INFINITY));
  return maybe_nan ? Type::Number() : Type::OrderedNumber();
}


Type* Typer::Visitor::JSModulusRanger(Type::RangeType* lhs,
                                      Type::RangeType* rhs, Typer* t) {
  double lmin = lhs->Min();
  double lmax = lhs->Max();
  double rmin = rhs->Min();
  double rmax = rhs->Max();

  double labs = std::max(std::abs(lmin), std::abs(lmax));
  double rabs = std::max(std::abs(rmin), std::abs(rmax)) - 1;
  double abs = std::min(labs, rabs);
  bool maybe_minus_zero = false;
  double omin = 0;
  double omax = 0;
  if (lmin >= 0) {  // {lhs} positive.
    omin = 0;
    omax = abs;
  } else if (lmax <= 0) {  // {lhs} negative.
    omin = 0 - abs;
    omax = 0;
    maybe_minus_zero = true;
  } else {
    omin = 0 - abs;
    omax = abs;
    maybe_minus_zero = true;
  }

  Type* result = Type::Range(omin, omax, t->zone());
  if (maybe_minus_zero)
    result = Type::Union(result, Type::MinusZero(), t->zone());
  return result;
}


Type* Typer::Visitor::JSModulusTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = ToNumber(lhs, t);
  rhs = ToNumber(rhs, t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();

  if (lhs->Maybe(Type::NaN()) || rhs->Maybe(t->cache_.kZeroish) ||
      lhs->Min() == -V8_INFINITY || lhs->Max() == +V8_INFINITY) {
    // Result maybe NaN.
    return Type::Number();
  }

  lhs = Rangify(lhs, t);
  rhs = Rangify(rhs, t);
  if (lhs->IsRange() && rhs->IsRange()) {
    return JSModulusRanger(lhs->AsRange(), rhs->AsRange(), t);
  }
  return Type::OrderedNumber();
}


// JS unary operators.


Type* Typer::Visitor::JSUnaryNotTyper(Type* type, Typer* t) {
  return Invert(ToBoolean(type, t), t);
}


Bounds Typer::Visitor::TypeJSUnaryNot(Node* node) {
  return TypeUnaryOp(node, JSUnaryNotTyper);
}


Type* Typer::Visitor::JSTypeOfTyper(Type* type, Typer* t) {
  Factory* const f = t->isolate()->factory();
  if (type->Is(Type::Boolean())) {
    return Type::Constant(f->boolean_string(), t->zone());
  } else if (type->Is(Type::Number())) {
    return Type::Constant(f->number_string(), t->zone());
  } else if (type->Is(Type::Symbol())) {
    return Type::Constant(f->symbol_string(), t->zone());
  } else if (type->Is(Type::Union(Type::Undefined(), Type::Undetectable()))) {
    return Type::Constant(f->undefined_string(), t->zone());
  } else if (type->Is(Type::Null())) {
    return Type::Constant(f->object_string(), t->zone());
  }
  return Type::InternalizedString();
}


Bounds Typer::Visitor::TypeJSTypeOf(Node* node) {
  return TypeUnaryOp(node, JSTypeOfTyper);
}


// JS conversion operators.


Bounds Typer::Visitor::TypeJSToBoolean(Node* node) {
  return TypeUnaryOp(node, ToBoolean);
}


Bounds Typer::Visitor::TypeJSToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}


Bounds Typer::Visitor::TypeJSToString(Node* node) {
  return TypeUnaryOp(node, ToString);
}


Bounds Typer::Visitor::TypeJSToName(Node* node) {
  return Bounds(Type::None(), Type::Name());
}


Bounds Typer::Visitor::TypeJSToObject(Node* node) {
  return Bounds(Type::None(), Type::Receiver());
}


// JS object operators.


Bounds Typer::Visitor::TypeJSCreate(Node* node) {
  return Bounds(Type::None(), Type::Object());
}


Bounds Typer::Visitor::TypeJSCreateClosure(Node* node) {
  return Bounds(Type::None(), Type::OtherObject());
}


Bounds Typer::Visitor::TypeJSCreateLiteralArray(Node* node) {
  return Bounds(Type::None(), Type::OtherObject());
}


Bounds Typer::Visitor::TypeJSCreateLiteralObject(Node* node) {
  return Bounds(Type::None(), Type::OtherObject());
}


Type* Typer::Visitor::JSLoadPropertyTyper(Type* object, Type* name, Typer* t) {
  // TODO(rossberg): Use range types and sized array types to filter undefined.
  if (object->IsArray() && name->Is(Type::Integral32())) {
    return Type::Union(
        object->AsArray()->Element(), Type::Undefined(), t->zone());
  }
  return Type::Any();
}


Bounds Typer::Visitor::TypeJSLoadProperty(Node* node) {
  return TypeBinaryOp(node, JSLoadPropertyTyper);
}


Bounds Typer::Visitor::TypeJSLoadNamed(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSLoadGlobal(Node* node) {
  return Bounds::Unbounded(zone());
}


// Returns a somewhat larger range if we previously assigned
// a (smaller) range to this node. This is used  to speed up
// the fixpoint calculation in case there appears to be a loop
// in the graph. In the current implementation, we are
// increasing the limits to the closest power of two.
Type* Typer::Visitor::Weaken(Node* node, Type* current_type,
                             Type* previous_type) {
  static const double kWeakenMinLimits[] = {
      0.0, -1073741824.0, -2147483648.0, -4294967296.0, -8589934592.0,
      -17179869184.0, -34359738368.0, -68719476736.0, -137438953472.0,
      -274877906944.0, -549755813888.0, -1099511627776.0, -2199023255552.0,
      -4398046511104.0, -8796093022208.0, -17592186044416.0, -35184372088832.0,
      -70368744177664.0, -140737488355328.0, -281474976710656.0,
      -562949953421312.0};
  static const double kWeakenMaxLimits[] = {
      0.0, 1073741823.0, 2147483647.0, 4294967295.0, 8589934591.0,
      17179869183.0, 34359738367.0, 68719476735.0, 137438953471.0,
      274877906943.0, 549755813887.0, 1099511627775.0, 2199023255551.0,
      4398046511103.0, 8796093022207.0, 17592186044415.0, 35184372088831.0,
      70368744177663.0, 140737488355327.0, 281474976710655.0,
      562949953421311.0};
  STATIC_ASSERT(arraysize(kWeakenMinLimits) == arraysize(kWeakenMaxLimits));

  // If the types have nothing to do with integers, return the types.
  Type* const integer = typer_->cache_.kInteger;
  if (!previous_type->Maybe(integer)) {
    return current_type;
  }
  DCHECK(current_type->Maybe(integer));

  Type* current_integer = Type::Intersect(current_type, integer, zone());
  Type* previous_integer = Type::Intersect(previous_type, integer, zone());

  // Once we start weakening a node, we should always weaken.
  if (!IsWeakened(node->id())) {
    // Only weaken if there is range involved; we should converge quickly
    // for all other types (the exception is a union of many constants,
    // but we currently do not increase the number of constants in unions).
    Type::RangeType* previous = previous_integer->GetRange();
    Type::RangeType* current = current_integer->GetRange();
    if (current == nullptr || previous == nullptr) {
      return current_type;
    }
    // Range is involved => we are weakening.
    SetWeakened(node->id());
  }

  double current_min = current_integer->Min();
  double new_min = current_min;
  // Find the closest lower entry in the list of allowed
  // minima (or negative infinity if there is no such entry).
  if (current_min != previous_integer->Min()) {
    new_min = -V8_INFINITY;
    for (double const min : kWeakenMinLimits) {
      if (min <= current_min) {
        new_min = min;
        break;
      }
    }
  }

  double current_max = current_integer->Max();
  double new_max = current_max;
  // Find the closest greater entry in the list of allowed
  // maxima (or infinity if there is no such entry).
  if (current_max != previous_integer->Max()) {
    new_max = V8_INFINITY;
    for (double const max : kWeakenMaxLimits) {
      if (max >= current_max) {
        new_max = max;
        break;
      }
    }
  }

  return Type::Union(current_type,
                     Type::Range(new_min, new_max, typer_->zone()),
                     typer_->zone());
}


Bounds Typer::Visitor::TypeJSStoreProperty(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeJSStoreNamed(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeJSStoreGlobal(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeJSDeleteProperty(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSHasProperty(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSInstanceOf(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


// JS context operators.


Bounds Typer::Visitor::TypeJSLoadContext(Node* node) {
  ContextAccess access = OpParameter<ContextAccess>(node);
  Bounds outer = Operand(node, 0);
  Type* context_type = outer.upper;
  if (context_type->Is(Type::None())) {
    // Upper bound of context is not yet known.
    return Bounds(Type::None(), Type::Any());
  }

  DCHECK(context_type->Maybe(Type::Internal()));
  // TODO(rossberg): More precisely, instead of the above assertion, we should
  // back-propagate the constraint that it has to be a subtype of Internal.

  MaybeHandle<Context> context;
  if (context_type->IsConstant()) {
    context = Handle<Context>::cast(context_type->AsConstant()->Value());
  }
  // Walk context chain (as far as known), mirroring dynamic lookup.
  // Since contexts are mutable, the information is only useful as a lower
  // bound.
  for (size_t i = access.depth(); i > 0; --i) {
    if (context_type->IsContext()) {
      context_type = context_type->AsContext()->Outer();
      if (context_type->IsConstant()) {
        context = Handle<Context>::cast(context_type->AsConstant()->Value());
      }
    } else if (!context.is_null()) {
      context = handle(context.ToHandleChecked()->previous(), isolate());
    }
  }
  Type* lower = Type::None();
  if (!context.is_null()) {
    lower = TypeConstant(
        handle(context.ToHandleChecked()->get(static_cast<int>(access.index())),
               isolate()));
  }
  return Bounds(lower, Type::Any());
}


Bounds Typer::Visitor::TypeJSStoreContext(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeJSLoadDynamicGlobal(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSLoadDynamicContext(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::WrapContextBoundsForInput(Node* node) {
  Bounds outer = BoundsOrNone(NodeProperties::GetContextInput(node));
  if (outer.upper->Is(Type::None())) {
    return Bounds(Type::None());
  } else {
    DCHECK(outer.upper->Maybe(Type::Internal()));
    return Bounds(Type::Context(outer.upper, zone()));
  }
}


Bounds Typer::Visitor::TypeJSCreateFunctionContext(Node* node) {
  return WrapContextBoundsForInput(node);
}


Bounds Typer::Visitor::TypeJSCreateCatchContext(Node* node) {
  return WrapContextBoundsForInput(node);
}


Bounds Typer::Visitor::TypeJSCreateWithContext(Node* node) {
  return WrapContextBoundsForInput(node);
}


Bounds Typer::Visitor::TypeJSCreateBlockContext(Node* node) {
  return WrapContextBoundsForInput(node);
}


Bounds Typer::Visitor::TypeJSCreateModuleContext(Node* node) {
  // TODO(rossberg): this is probably incorrect
  return WrapContextBoundsForInput(node);
}


Bounds Typer::Visitor::TypeJSCreateScriptContext(Node* node) {
  return WrapContextBoundsForInput(node);
}


// JS other operators.


Bounds Typer::Visitor::TypeJSYield(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSCallConstruct(Node* node) {
  return Bounds(Type::None(), Type::Receiver());
}


Type* Typer::Visitor::JSCallFunctionTyper(Type* fun, Typer* t) {
  return fun->IsFunction() ? fun->AsFunction()->Result() : Type::Any();
}


Bounds Typer::Visitor::TypeJSCallFunction(Node* node) {
  return TypeUnaryOp(node, JSCallFunctionTyper);  // We ignore argument types.
}


Bounds Typer::Visitor::TypeJSCallRuntime(Node* node) {
  switch (CallRuntimeParametersOf(node->op()).id()) {
    case Runtime::kInlineIsSmi:
    case Runtime::kInlineIsNonNegativeSmi:
    case Runtime::kInlineIsArray:
    case Runtime::kInlineIsDate:
    case Runtime::kInlineIsTypedArray:
    case Runtime::kInlineIsMinusZero:
    case Runtime::kInlineIsFunction:
    case Runtime::kInlineIsRegExp:
      return Bounds(Type::None(zone()), Type::Boolean(zone()));
    case Runtime::kInlineDoubleLo:
    case Runtime::kInlineDoubleHi:
      return Bounds(Type::None(zone()), Type::Signed32());
    case Runtime::kInlineConstructDouble:
    case Runtime::kInlineDateField:
    case Runtime::kInlineMathFloor:
    case Runtime::kInlineMathSqrt:
    case Runtime::kInlineMathAcos:
    case Runtime::kInlineMathAsin:
    case Runtime::kInlineMathAtan:
    case Runtime::kInlineMathAtan2:
      return Bounds(Type::None(zone()), Type::Number());
    case Runtime::kInlineMathClz32:
      return Bounds(Type::None(), Type::Range(0, 32, zone()));
    case Runtime::kInlineStringGetLength:
      return Bounds(Type::None(), Type::Range(0, String::kMaxLength, zone()));
    default:
      break;
  }
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSForInNext(Node* node) {
  return Bounds(Type::None(zone()),
                Type::Union(Type::Name(), Type::Undefined(), zone()));
}


Bounds Typer::Visitor::TypeJSForInPrepare(Node* node) {
  // TODO(bmeurer): Return a tuple type here.
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSForInDone(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSForInStep(Node* node) {
  STATIC_ASSERT(Map::EnumLengthBits::kMax <= FixedArray::kMaxLength);
  return Bounds(Type::None(zone()),
                Type::Range(1, FixedArray::kMaxLength + 1, zone()));
}


Bounds Typer::Visitor::TypeJSStackCheck(Node* node) {
  return Bounds::Unbounded(zone());
}


// Simplified operators.


Bounds Typer::Visitor::TypeBooleanNot(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeBooleanToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}


Bounds Typer::Visitor::TypeNumberEqual(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberLessThan(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberLessThanOrEqual(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberAdd(Node* node) {
  return Bounds(Type::None(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberSubtract(Node* node) {
  return Bounds(Type::None(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberMultiply(Node* node) {
  return Bounds(Type::None(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberDivide(Node* node) {
  return Bounds(Type::None(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberModulus(Node* node) {
  return Bounds(Type::None(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberShiftLeft(Node* node) {
  return Bounds(Type::None(zone()), Type::Signed32(zone()));
}


Bounds Typer::Visitor::TypeNumberShiftRight(Node* node) {
  return Bounds(Type::None(zone()), Type::Signed32(zone()));
}


Bounds Typer::Visitor::TypeNumberShiftRightLogical(Node* node) {
  return Bounds(Type::None(zone()), Type::Unsigned32(zone()));
}


Bounds Typer::Visitor::TypeNumberToInt32(Node* node) {
  return TypeUnaryOp(node, NumberToInt32);
}


Bounds Typer::Visitor::TypeNumberToUint32(Node* node) {
  return TypeUnaryOp(node, NumberToUint32);
}


Bounds Typer::Visitor::TypePlainPrimitiveToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}


Bounds Typer::Visitor::TypeReferenceEqual(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringEqual(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringLessThan(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringLessThanOrEqual(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


namespace {

Type* ChangeRepresentation(Type* type, Type* rep, Zone* zone) {
  return Type::Union(Type::Semantic(type, zone),
                     Type::Representation(rep, zone), zone);
}

}  // namespace


Bounds Typer::Visitor::TypeChangeTaggedToInt32(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Signed32()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::UntaggedSigned32(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedSigned32(), zone()));
}


Bounds Typer::Visitor::TypeChangeTaggedToUint32(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Unsigned32()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::UntaggedUnsigned32(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedUnsigned32(), zone()));
}


Bounds Typer::Visitor::TypeChangeTaggedToFloat64(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Number()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::UntaggedFloat64(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedFloat64(), zone()));
}


Bounds Typer::Visitor::TypeChangeInt32ToTagged(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Signed32()));
  Type* lower_rep = arg.lower->Is(Type::SignedSmall()) ? Type::TaggedSigned()
                                                       : Type::Tagged();
  Type* upper_rep = arg.upper->Is(Type::SignedSmall()) ? Type::TaggedSigned()
                                                       : Type::Tagged();
  return Bounds(ChangeRepresentation(arg.lower, lower_rep, zone()),
                ChangeRepresentation(arg.upper, upper_rep, zone()));
}


Bounds Typer::Visitor::TypeChangeUint32ToTagged(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Unsigned32()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::Tagged(), zone()),
      ChangeRepresentation(arg.upper, Type::Tagged(), zone()));
}


Bounds Typer::Visitor::TypeChangeFloat64ToTagged(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): CHECK(arg.upper->Is(Type::Number()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::Tagged(), zone()),
      ChangeRepresentation(arg.upper, Type::Tagged(), zone()));
}


Bounds Typer::Visitor::TypeChangeBoolToBit(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Boolean()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::UntaggedBit(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedBit(), zone()));
}


Bounds Typer::Visitor::TypeChangeBitToBool(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Boolean()));
  return Bounds(ChangeRepresentation(arg.lower, Type::TaggedPointer(), zone()),
                ChangeRepresentation(arg.upper, Type::TaggedPointer(), zone()));
}


Bounds Typer::Visitor::TypeAllocate(Node* node) {
  return Bounds(Type::TaggedPointer());
}


Bounds Typer::Visitor::TypeLoadField(Node* node) {
  return Bounds(FieldAccessOf(node->op()).type);
}


Bounds Typer::Visitor::TypeLoadBuffer(Node* node) {
  // TODO(bmeurer): This typing is not yet correct. Since we can still access
  // out of bounds, the type in the general case has to include Undefined.
  switch (BufferAccessOf(node->op()).external_array_type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return Bounds(typer_->cache_.k##Type);
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeLoadElement(Node* node) {
  return Bounds(ElementAccessOf(node->op()).type);
}


Bounds Typer::Visitor::TypeStoreField(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeStoreBuffer(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeStoreElement(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeObjectIsSmi(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeObjectIsNonNegativeSmi(Node* node) {
  return Bounds(Type::Boolean());
}


// Machine operators.

Bounds Typer::Visitor::TypeLoad(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeStore(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeWord32And(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Or(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Xor(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Shl(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Shr(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Sar(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Ror(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord32Equal(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeWord32Clz(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeWord64And(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Or(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Xor(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Shl(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Shr(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Sar(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Ror(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeWord64Equal(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeInt32Add(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeInt32AddWithOverflow(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt32Sub(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeInt32SubWithOverflow(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt32Mul(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeInt32MulHigh(Node* node) {
  return Bounds(Type::Signed32());
}


Bounds Typer::Visitor::TypeInt32Div(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeInt32Mod(Node* node) {
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeInt32LessThan(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeInt32LessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeUint32Div(Node* node) {
  return Bounds(Type::Unsigned32());
}


Bounds Typer::Visitor::TypeUint32LessThan(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeUint32LessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeUint32Mod(Node* node) {
  return Bounds(Type::Unsigned32());
}


Bounds Typer::Visitor::TypeUint32MulHigh(Node* node) {
  return Bounds(Type::Unsigned32());
}


Bounds Typer::Visitor::TypeInt64Add(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt64Sub(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt64Mul(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt64Div(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt64Mod(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeInt64LessThan(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeInt64LessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeUint64Div(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeUint64LessThan(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeUint64LessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeUint64Mod(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeChangeFloat32ToFloat64(Node* node) {
  return Bounds(Type::Intersect(
      Type::Number(), Type::UntaggedFloat64(), zone()));
}


Bounds Typer::Visitor::TypeChangeFloat64ToInt32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Signed32(), Type::UntaggedSigned32(), zone()));
}


Bounds Typer::Visitor::TypeChangeFloat64ToUint32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Unsigned32(), Type::UntaggedUnsigned32(), zone()));
}


Bounds Typer::Visitor::TypeChangeInt32ToFloat64(Node* node) {
  return Bounds(Type::Intersect(
      Type::Signed32(), Type::UntaggedFloat64(), zone()));
}


Bounds Typer::Visitor::TypeChangeInt32ToInt64(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeChangeUint32ToFloat64(Node* node) {
  return Bounds(Type::Intersect(
      Type::Unsigned32(), Type::UntaggedFloat64(), zone()));
}


Bounds Typer::Visitor::TypeChangeUint32ToUint64(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeTruncateFloat64ToFloat32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Number(), Type::UntaggedFloat32(), zone()));
}


Bounds Typer::Visitor::TypeTruncateFloat64ToInt32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Signed32(), Type::UntaggedSigned32(), zone()));
}


Bounds Typer::Visitor::TypeTruncateInt64ToInt32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Signed32(), Type::UntaggedSigned32(), zone()));
}


Bounds Typer::Visitor::TypeFloat32Add(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Sub(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Mul(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Div(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Max(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Min(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Abs(Node* node) {
  // TODO(turbofan): We should be able to infer a better type here.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Sqrt(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat32Equal(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeFloat32LessThan(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeFloat32LessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeFloat64Add(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Sub(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Mul(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Div(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Mod(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Max(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Min(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Abs(Node* node) {
  // TODO(turbofan): We should be able to infer a better type here.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Sqrt(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Equal(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeFloat64LessThan(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeFloat64LessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeFloat64RoundDown(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64RoundTruncate(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64RoundTiesAway(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64ExtractLowWord32(Node* node) {
  return Bounds(Type::Signed32());
}


Bounds Typer::Visitor::TypeFloat64ExtractHighWord32(Node* node) {
  return Bounds(Type::Signed32());
}


Bounds Typer::Visitor::TypeFloat64InsertLowWord32(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64InsertHighWord32(Node* node) {
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeLoadStackPointer(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeLoadFramePointer(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeCheckedLoad(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeCheckedStore(Node* node) {
  UNREACHABLE();
  return Bounds();
}


// Heap constants.


Type* Typer::Visitor::TypeConstant(Handle<Object> value) {
  if (value->IsJSFunction()) {
    if (JSFunction::cast(*value)->shared()->HasBuiltinFunctionId()) {
      switch (JSFunction::cast(*value)->shared()->builtin_function_id()) {
        case kMathRandom:
          return typer_->cache_.kRandomFunc0;
        case kMathFloor:
        case kMathRound:
        case kMathCeil:
          return typer_->cache_.kWeakintFunc1;
        // Unary math functions.
        case kMathAbs:  // TODO(rossberg): can't express overloading
        case kMathLog:
        case kMathExp:
        case kMathSqrt:
        case kMathCos:
        case kMathSin:
        case kMathTan:
        case kMathAcos:
        case kMathAsin:
        case kMathAtan:
        case kMathFround:
          return typer_->cache_.kNumberFunc1;
        // Binary math functions.
        case kMathAtan2:
        case kMathPow:
        case kMathMax:
        case kMathMin:
          return typer_->cache_.kNumberFunc2;
        case kMathImul:
          return typer_->cache_.kImulFunc;
        case kMathClz32:
          return typer_->cache_.kClz32Func;
        default:
          break;
      }
    }
    int const arity =
        JSFunction::cast(*value)->shared()->internal_formal_parameter_count();
    switch (arity) {
      case SharedFunctionInfo::kDontAdaptArgumentsSentinel:
        // Some smart optimization at work... &%$!&@+$!
        break;
      case 0:
        return typer_->cache_.kAnyFunc0;
      case 1:
        return typer_->cache_.kAnyFunc1;
      case 2:
        return typer_->cache_.kAnyFunc2;
      case 3:
        return typer_->cache_.kAnyFunc3;
      default: {
        DCHECK_LT(3, arity);
        Type** const params = zone()->NewArray<Type*>(arity);
        std::fill(&params[0], &params[arity], Type::Any(zone()));
        return Type::Function(Type::Any(zone()), arity, params, zone());
      }
    }
  } else if (value->IsJSTypedArray()) {
    switch (JSTypedArray::cast(*value)->type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return typer_->cache_.k##Type##Array;
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    }
  }
  return Type::Constant(value, zone());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
