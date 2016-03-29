// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typer.h"

#include "src/base/flags.h"
#include "src/bootstrapper.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects-inl.h"
#include "src/type-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

class Typer::Decorator final : public GraphDecorator {
 public:
  explicit Decorator(Typer* typer) : typer_(typer) {}
  void Decorate(Node* node) final;

 private:
  Typer* const typer_;
};

Typer::Typer(Isolate* isolate, Graph* graph, Flags flags,
             CompilationDependencies* dependencies, FunctionType* function_type)
    : isolate_(isolate),
      graph_(graph),
      flags_(flags),
      dependencies_(dependencies),
      function_type_(function_type),
      decorator_(nullptr),
      cache_(TypeCache::Get()) {
  Zone* zone = this->zone();
  Factory* const factory = isolate->factory();

  Type* infinity = Type::Constant(factory->infinity_value(), zone);
  Type* minus_infinity = Type::Constant(factory->minus_infinity_value(), zone);
  // TODO(neis): Unfortunately, the infinities created in other places might
  // be different ones (eg the result of NewNumber in TypeNumberConstant).
  Type* truncating_to_zero =
      Type::Union(Type::Union(infinity, minus_infinity, zone),
                  Type::MinusZeroOrNaN(), zone);
  DCHECK(!truncating_to_zero->Maybe(Type::Integral32()));

  singleton_false_ = Type::Constant(factory->false_value(), zone);
  singleton_true_ = Type::Constant(factory->true_value(), zone);
  singleton_the_hole_ = Type::Constant(factory->the_hole_value(), zone);
  signed32ish_ = Type::Union(Type::Signed32(), truncating_to_zero, zone);
  unsigned32ish_ = Type::Union(Type::Unsigned32(), truncating_to_zero, zone);
  falsish_ = Type::Union(
      Type::Undetectable(),
      Type::Union(
          Type::Union(Type::Union(singleton_false_, cache_.kZeroish, zone),
                      Type::NullOrUndefined(), zone),
          singleton_the_hole_, zone),
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
    return UpdateType(node, TypeBinaryOp(node, x##Typer));
      JS_SIMPLE_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return UpdateType(node, Type##x(node));
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

  Type* TypeNode(Node* node) {
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
    return nullptr;
  }

  Type* TypeConstant(Handle<Object> value);

 private:
  Typer* typer_;
  ZoneSet<NodeId> weakened_nodes_;

#define DECLARE_METHOD(x) inline Type* Type##x(Node* node);
  DECLARE_METHOD(Start)
  DECLARE_METHOD(IfException)
  VALUE_OP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  Type* TypeOrNone(Node* node) {
    return NodeProperties::IsTyped(node) ? NodeProperties::GetType(node)
                                         : Type::None();
  }

  Type* Operand(Node* node, int i) {
    Node* operand_node = NodeProperties::GetValueInput(node, i);
    return TypeOrNone(operand_node);
  }

  Type* WrapContextTypeForInput(Node* node);
  Type* Weaken(Node* node, Type* current_type, Type* previous_type);

  Zone* zone() { return typer_->zone(); }
  Isolate* isolate() { return typer_->isolate(); }
  Graph* graph() { return typer_->graph(); }
  Typer::Flags flags() const { return typer_->flags(); }
  CompilationDependencies* dependencies() const {
    return typer_->dependencies();
  }

  void SetWeakened(NodeId node_id) { weakened_nodes_.insert(node_id); }
  bool IsWeakened(NodeId node_id) {
    return weakened_nodes_.find(node_id) != weakened_nodes_.end();
  }

  typedef Type* (*UnaryTyperFun)(Type*, Typer* t);
  typedef Type* (*BinaryTyperFun)(Type*, Type*, Typer* t);

  Type* TypeUnaryOp(Node* node, UnaryTyperFun);
  Type* TypeBinaryOp(Node* node, BinaryTyperFun);

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
  static Type* ToInteger(Type*, Typer*);
  static Type* ToLength(Type*, Typer*);
  static Type* ToName(Type*, Typer*);
  static Type* ToNumber(Type*, Typer*);
  static Type* ToObject(Type*, Typer*);
  static Type* ToString(Type*, Typer*);
  static Type* NumberToInt32(Type*, Typer*);
  static Type* NumberToUint32(Type*, Typer*);

  static Type* ObjectIsNumber(Type*, Typer*);
  static Type* ObjectIsReceiver(Type*, Typer*);
  static Type* ObjectIsSmi(Type*, Typer*);

  static Type* JSAddRanger(RangeType*, RangeType*, Typer*);
  static Type* JSSubtractRanger(RangeType*, RangeType*, Typer*);
  static Type* JSDivideRanger(RangeType*, RangeType*, Typer*);
  static Type* JSModulusRanger(RangeType*, RangeType*, Typer*);

  static ComparisonOutcome JSCompareTyper(Type*, Type*, Typer*);

#define DECLARE_METHOD(x) static Type* x##Typer(Type*, Type*, Typer*);
  JS_SIMPLE_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  static Type* JSTypeOfTyper(Type*, Typer*);
  static Type* JSLoadPropertyTyper(Type*, Type*, Typer*);
  static Type* JSCallFunctionTyper(Type*, Typer*);

  static Type* ReferenceEqualTyper(Type*, Type*, Typer*);

  Reduction UpdateType(Node* node, Type* current) {
    if (NodeProperties::IsTyped(node)) {
      // Widen the type of a previously typed node.
      Type* previous = NodeProperties::GetType(node);
      if (node->opcode() == IrOpcode::kPhi) {
        // Speed up termination in the presence of range types:
        current = Weaken(node, current, previous);
      }

      CHECK(previous->Is(current));

      NodeProperties::SetType(node, current);
      if (!current->Is(previous)) {
        // If something changed, revisit all uses.
        return Changed(node);
      }
      return NoChange();
    } else {
      // No previous type, simply update the type.
      NodeProperties::SetType(node, current);
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
      Type* type = typing.TypeNode(node);
      if (is_typed) {
        type = Type::Intersect(type, NodeProperties::GetType(node),
                               typer_->zone());
      }
      NodeProperties::SetType(node, type);
    }
  }
}


// -----------------------------------------------------------------------------

// Helper functions that lift a function f on types to a function on bounds,
// and uses that to type the given node.  Note that f is never called with None
// as an argument.


Type* Typer::Visitor::TypeUnaryOp(Node* node, UnaryTyperFun f) {
  Type* input = Operand(node, 0);
  return input->IsInhabited() ? f(input, typer_) : Type::None();
}


Type* Typer::Visitor::TypeBinaryOp(Node* node, BinaryTyperFun f) {
  Type* left = Operand(node, 0);
  Type* right = Operand(node, 1);
  return left->IsInhabited() && right->IsInhabited() ? f(left, right, typer_)
                                                     : Type::None();
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


// static
Type* Typer::Visitor::ToInteger(Type* type, Typer* t) {
  // ES6 section 7.1.4 ToInteger ( argument )
  type = ToNumber(type, t);
  if (type->Is(t->cache_.kIntegerOrMinusZero)) return type;
  return t->cache_.kIntegerOrMinusZero;
}


// static
Type* Typer::Visitor::ToLength(Type* type, Typer* t) {
  // ES6 section 7.1.15 ToLength ( argument )
  type = ToInteger(type, t);
  double min = type->Min();
  double max = type->Max();
  if (min <= 0.0) min = 0.0;
  if (max > kMaxSafeInteger) max = kMaxSafeInteger;
  if (max <= min) max = min;
  return Type::Range(min, max, t->zone());
}


// static
Type* Typer::Visitor::ToName(Type* type, Typer* t) {
  // ES6 section 7.1.14 ToPropertyKey ( argument )
  type = ToPrimitive(type, t);
  if (type->Is(Type::Name())) return type;
  if (type->Maybe(Type::Symbol())) return Type::Name();
  return ToString(type, t);
}


// static
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


// static
Type* Typer::Visitor::ToObject(Type* type, Typer* t) {
  // ES6 section 7.1.13 ToObject ( argument )
  if (type->Is(Type::Receiver())) return type;
  if (type->Is(Type::Primitive())) return Type::OtherObject();
  if (!type->Maybe(Type::Undetectable())) return Type::DetectableReceiver();
  return Type::Receiver();
}


// static
Type* Typer::Visitor::ToString(Type* type, Typer* t) {
  // ES6 section 7.1.12 ToString ( argument )
  type = ToPrimitive(type, t);
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


// Type checks.


Type* Typer::Visitor::ObjectIsNumber(Type* type, Typer* t) {
  if (type->Is(Type::Number())) return t->singleton_true_;
  if (!type->Maybe(Type::Number())) return t->singleton_false_;
  return Type::Boolean();
}


Type* Typer::Visitor::ObjectIsReceiver(Type* type, Typer* t) {
  if (type->Is(Type::Receiver())) return t->singleton_true_;
  if (!type->Maybe(Type::Receiver())) return t->singleton_false_;
  return Type::Boolean();
}


Type* Typer::Visitor::ObjectIsSmi(Type* type, Typer* t) {
  if (type->Is(Type::TaggedSigned())) return t->singleton_true_;
  if (type->Is(Type::TaggedPointer())) return t->singleton_false_;
  return Type::Boolean();
}


// -----------------------------------------------------------------------------


// Control operators.

Type* Typer::Visitor::TypeStart(Node* node) { return Type::Internal(); }

Type* Typer::Visitor::TypeIfException(Node* node) { return Type::Any(); }


// Common operators.


Type* Typer::Visitor::TypeParameter(Node* node) {
  if (FunctionType* function_type = typer_->function_type()) {
    int const index = ParameterIndexOf(node->op());
    if (index >= 0 && index < function_type->Arity()) {
      return function_type->Parameter(index);
    }
  }
  return Type::Any();
}


Type* Typer::Visitor::TypeOsrValue(Node* node) { return Type::Any(); }


Type* Typer::Visitor::TypeInt32Constant(Node* node) {
  double number = OpParameter<int32_t>(node);
  return Type::Intersect(Type::Range(number, number, zone()),
                         Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeInt64Constant(Node* node) {
  // TODO(rossberg): This actually seems to be a PointerConstant so far...
  return Type::Internal();  // TODO(rossberg): Add int64 bitset type?
}


Type* Typer::Visitor::TypeFloat32Constant(Node* node) {
  return Type::Intersect(Type::Of(OpParameter<float>(node), zone()),
                         Type::UntaggedFloat32(), zone());
}


Type* Typer::Visitor::TypeFloat64Constant(Node* node) {
  return Type::Intersect(Type::Of(OpParameter<double>(node), zone()),
                         Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeNumberConstant(Node* node) {
  Factory* f = isolate()->factory();
  double number = OpParameter<double>(node);
  if (Type::IsInteger(number)) {
    return Type::Range(number, number, zone());
  }
  return Type::Constant(f->NewNumber(number), zone());
}


Type* Typer::Visitor::TypeHeapConstant(Node* node) {
  return TypeConstant(OpParameter<Handle<HeapObject>>(node));
}


Type* Typer::Visitor::TypeExternalConstant(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeSelect(Node* node) {
  return Type::Union(Operand(node, 1), Operand(node, 2), zone());
}


Type* Typer::Visitor::TypePhi(Node* node) {
  int arity = node->op()->ValueInputCount();
  Type* type = Operand(node, 0);
  for (int i = 1; i < arity; ++i) {
    type = Type::Union(type, Operand(node, i), zone());
  }
  return type;
}


Type* Typer::Visitor::TypeEffectPhi(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeEffectSet(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeGuard(Node* node) {
  Type* input_type = Operand(node, 0);
  Type* guard_type = OpParameter<Type*>(node);
  return Type::Intersect(input_type, guard_type, zone());
}


Type* Typer::Visitor::TypeBeginRegion(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeFinishRegion(Node* node) { return Operand(node, 0); }


Type* Typer::Visitor::TypeFrameState(Node* node) {
  // TODO(rossberg): Ideally FrameState wouldn't have a value output.
  return Type::Internal();
}

Type* Typer::Visitor::TypeStateValues(Node* node) { return Type::Internal(); }

Type* Typer::Visitor::TypeObjectState(Node* node) { return Type::Internal(); }

Type* Typer::Visitor::TypeTypedStateValues(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeCall(Node* node) { return Type::Any(); }


Type* Typer::Visitor::TypeProjection(Node* node) {
  Type* const type = Operand(node, 0);
  if (type->Is(Type::None())) return Type::None();
  int const index = static_cast<int>(ProjectionIndexOf(node->op()));
  if (type->IsTuple() && index < type->AsTuple()->Arity()) {
    return type->AsTuple()->Element(index);
  }
  return Type::Any();
}


Type* Typer::Visitor::TypeDead(Node* node) { return Type::Any(); }


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
  if ((lhs->Is(t->singleton_the_hole_) || rhs->Is(t->singleton_the_hole_)) &&
      !lhs->Maybe(rhs)) {
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

Type* Typer::Visitor::JSAddRanger(RangeType* lhs, RangeType* rhs, Typer* t) {
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

Type* Typer::Visitor::JSSubtractRanger(RangeType* lhs, RangeType* rhs,
                                       Typer* t) {
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


Type* Typer::Visitor::JSMultiplyTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = Rangify(ToNumber(lhs, t), t);
  rhs = Rangify(ToNumber(rhs, t), t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();
  if (lhs->IsRange() && rhs->IsRange()) {
    double results[4];
    double lmin = lhs->AsRange()->Min();
    double lmax = lhs->AsRange()->Max();
    double rmin = rhs->AsRange()->Min();
    double rmax = rhs->AsRange()->Max();
    results[0] = lmin * rmin;
    results[1] = lmin * rmax;
    results[2] = lmax * rmin;
    results[3] = lmax * rmax;
    // If the result may be nan, we give up on calculating a precise type,
    // because
    // the discontinuity makes it too complicated.  Note that even if none of
    // the
    // "results" above is nan, the actual result may still be, so we have to do
    // a
    // different check:
    bool maybe_nan = (lhs->Maybe(t->cache_.kSingletonZero) &&
                      (rmin == -V8_INFINITY || rmax == +V8_INFINITY)) ||
                     (rhs->Maybe(t->cache_.kSingletonZero) &&
                      (lmin == -V8_INFINITY || lmax == +V8_INFINITY));
    if (maybe_nan) return t->cache_.kIntegerOrMinusZeroOrNaN;  // Giving up.
    bool maybe_minuszero = (lhs->Maybe(t->cache_.kSingletonZero) && rmin < 0) ||
                           (rhs->Maybe(t->cache_.kSingletonZero) && lmin < 0);
    Type* range =
        Type::Range(array_min(results, 4), array_max(results, 4), t->zone());
    return maybe_minuszero ? Type::Union(range, Type::MinusZero(), t->zone())
                           : range;
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

Type* Typer::Visitor::JSModulusRanger(RangeType* lhs, RangeType* rhs,
                                      Typer* t) {
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


Type* Typer::Visitor::JSTypeOfTyper(Type* type, Typer* t) {
  Factory* const f = t->isolate()->factory();
  if (type->Is(Type::Boolean())) {
    return Type::Constant(f->boolean_string(), t->zone());
  } else if (type->Is(Type::Number())) {
    return Type::Constant(f->number_string(), t->zone());
  } else if (type->Is(Type::String())) {
    return Type::Constant(f->string_string(), t->zone());
  } else if (type->Is(Type::Symbol())) {
    return Type::Constant(f->symbol_string(), t->zone());
  } else if (type->Is(Type::Union(Type::Undefined(), Type::Undetectable(),
                                  t->zone()))) {
    return Type::Constant(f->undefined_string(), t->zone());
  } else if (type->Is(Type::Null())) {
    return Type::Constant(f->object_string(), t->zone());
  } else if (type->Is(Type::Function())) {
    return Type::Constant(f->function_string(), t->zone());
  } else if (type->IsConstant()) {
    return Type::Constant(
        Object::TypeOf(t->isolate(), type->AsConstant()->Value()), t->zone());
  }
  return Type::InternalizedString();
}


Type* Typer::Visitor::TypeJSTypeOf(Node* node) {
  return TypeUnaryOp(node, JSTypeOfTyper);
}


// JS conversion operators.


Type* Typer::Visitor::TypeJSToBoolean(Node* node) {
  return TypeUnaryOp(node, ToBoolean);
}


Type* Typer::Visitor::TypeJSToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}


Type* Typer::Visitor::TypeJSToString(Node* node) {
  return TypeUnaryOp(node, ToString);
}


Type* Typer::Visitor::TypeJSToName(Node* node) {
  return TypeUnaryOp(node, ToName);
}


Type* Typer::Visitor::TypeJSToObject(Node* node) {
  return TypeUnaryOp(node, ToObject);
}


// JS object operators.


Type* Typer::Visitor::TypeJSCreate(Node* node) { return Type::Object(); }


Type* Typer::Visitor::TypeJSCreateArguments(Node* node) {
  return Type::OtherObject();
}


Type* Typer::Visitor::TypeJSCreateArray(Node* node) {
  return Type::OtherObject();
}


Type* Typer::Visitor::TypeJSCreateClosure(Node* node) {
  return Type::Function();
}


Type* Typer::Visitor::TypeJSCreateIterResultObject(Node* node) {
  return Type::OtherObject();
}


Type* Typer::Visitor::TypeJSCreateLiteralArray(Node* node) {
  return Type::OtherObject();
}


Type* Typer::Visitor::TypeJSCreateLiteralObject(Node* node) {
  return Type::OtherObject();
}


Type* Typer::Visitor::TypeJSCreateLiteralRegExp(Node* node) {
  return Type::OtherObject();
}


Type* Typer::Visitor::JSLoadPropertyTyper(Type* object, Type* name, Typer* t) {
  // TODO(rossberg): Use range types and sized array types to filter undefined.
  if (object->IsArray() && name->Is(Type::Integral32())) {
    return Type::Union(
        object->AsArray()->Element(), Type::Undefined(), t->zone());
  }
  return Type::Any();
}


Type* Typer::Visitor::TypeJSLoadProperty(Node* node) {
  return TypeBinaryOp(node, JSLoadPropertyTyper);
}


Type* Typer::Visitor::TypeJSLoadNamed(Node* node) {
  Factory* const f = isolate()->factory();
  Handle<Name> name = NamedAccessOf(node->op()).name();
  if (name.is_identical_to(f->prototype_string())) {
    Type* receiver = Operand(node, 0);
    if (receiver->Is(Type::None())) return Type::None();
    if (receiver->IsConstant() &&
        receiver->AsConstant()->Value()->IsJSFunction()) {
      Handle<JSFunction> function =
          Handle<JSFunction>::cast(receiver->AsConstant()->Value());
      if (function->has_prototype()) {
        // We need to add a code dependency on the initial map of the {function}
        // in order to be notified about changes to "prototype" of {function},
        // so we can only infer a constant type if deoptimization is enabled.
        if (flags() & kDeoptimizationEnabled) {
          JSFunction::EnsureHasInitialMap(function);
          Handle<Map> initial_map(function->initial_map(), isolate());
          dependencies()->AssumeInitialMapCantChange(initial_map);
          return Type::Constant(handle(initial_map->prototype(), isolate()),
                                zone());
        }
      }
    } else if (receiver->IsClass() &&
               receiver->AsClass()->Map()->IsJSFunctionMap()) {
      Handle<Map> map = receiver->AsClass()->Map();
      return map->has_non_instance_prototype() ? Type::Primitive()
                                               : Type::Receiver();
    }
  }
  return Type::Any();
}


Type* Typer::Visitor::TypeJSLoadGlobal(Node* node) { return Type::Any(); }


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
    Type* previous = previous_integer->GetRange();
    Type* current = current_integer->GetRange();
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


Type* Typer::Visitor::TypeJSStoreProperty(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeJSStoreNamed(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeJSStoreGlobal(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeJSDeleteProperty(Node* node) {
  return Type::Boolean();
}

Type* Typer::Visitor::TypeJSHasProperty(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeJSInstanceOf(Node* node) { return Type::Boolean(); }

// JS context operators.


Type* Typer::Visitor::TypeJSLoadContext(Node* node) {
  ContextAccess const& access = ContextAccessOf(node->op());
  if (access.index() == Context::EXTENSION_INDEX) {
    return Type::TaggedPointer();
  }
  // Since contexts are mutable, we just return the top.
  return Type::Any();
}


Type* Typer::Visitor::TypeJSStoreContext(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::WrapContextTypeForInput(Node* node) {
  Type* outer = TypeOrNone(NodeProperties::GetContextInput(node));
  if (outer->Is(Type::None())) {
    return Type::None();
  } else {
    DCHECK(outer->Maybe(Type::Internal()));
    return Type::Context(outer, zone());
  }
}


Type* Typer::Visitor::TypeJSCreateFunctionContext(Node* node) {
  return WrapContextTypeForInput(node);
}


Type* Typer::Visitor::TypeJSCreateCatchContext(Node* node) {
  return WrapContextTypeForInput(node);
}


Type* Typer::Visitor::TypeJSCreateWithContext(Node* node) {
  return WrapContextTypeForInput(node);
}


Type* Typer::Visitor::TypeJSCreateBlockContext(Node* node) {
  return WrapContextTypeForInput(node);
}


Type* Typer::Visitor::TypeJSCreateModuleContext(Node* node) {
  // TODO(rossberg): this is probably incorrect
  return WrapContextTypeForInput(node);
}


Type* Typer::Visitor::TypeJSCreateScriptContext(Node* node) {
  return WrapContextTypeForInput(node);
}


// JS other operators.


Type* Typer::Visitor::TypeJSYield(Node* node) { return Type::Any(); }


Type* Typer::Visitor::TypeJSCallConstruct(Node* node) {
  return Type::Receiver();
}


Type* Typer::Visitor::JSCallFunctionTyper(Type* fun, Typer* t) {
  if (fun->IsFunction()) {
    return fun->AsFunction()->Result();
  }
  if (fun->IsConstant() && fun->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(fun->AsConstant()->Value());
    if (function->shared()->HasBuiltinFunctionId()) {
      switch (function->shared()->builtin_function_id()) {
        case kMathRandom:
          return Type::OrderedNumber();
        case kMathFloor:
        case kMathRound:
        case kMathCeil:
          return t->cache_.kIntegerOrMinusZeroOrNaN;
        // Unary math functions.
        case kMathAbs:
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
          return Type::Number();
        // Binary math functions.
        case kMathAtan2:
        case kMathPow:
        case kMathMax:
        case kMathMin:
          return Type::Number();
        case kMathImul:
          return Type::Signed32();
        case kMathClz32:
          return t->cache_.kZeroToThirtyTwo;
        // String functions.
        case kStringCharCodeAt:
          return Type::Union(Type::Range(0, kMaxUInt16, t->zone()), Type::NaN(),
                             t->zone());
        case kStringCharAt:
        case kStringConcat:
        case kStringFromCharCode:
        case kStringToLowerCase:
        case kStringToUpperCase:
          return Type::String();
        // Array functions.
        case kArrayIndexOf:
        case kArrayLastIndexOf:
          return Type::Number();
        default:
          break;
      }
    }
  }
  return Type::Any();
}


Type* Typer::Visitor::TypeJSCallFunction(Node* node) {
  // TODO(bmeurer): We could infer better types if we wouldn't ignore the
  // argument types for the JSCallFunctionTyper above.
  return TypeUnaryOp(node, JSCallFunctionTyper);
}


Type* Typer::Visitor::TypeJSCallRuntime(Node* node) {
  switch (CallRuntimeParametersOf(node->op()).id()) {
    case Runtime::kInlineIsJSReceiver:
      return TypeUnaryOp(node, ObjectIsReceiver);
    case Runtime::kInlineIsSmi:
      return TypeUnaryOp(node, ObjectIsSmi);
    case Runtime::kInlineIsArray:
    case Runtime::kInlineIsDate:
    case Runtime::kInlineIsTypedArray:
    case Runtime::kInlineIsRegExp:
      return Type::Boolean();
    case Runtime::kInlineDoubleLo:
    case Runtime::kInlineDoubleHi:
      return Type::Signed32();
    case Runtime::kInlineConstructDouble:
    case Runtime::kInlineMathFloor:
    case Runtime::kInlineMathSqrt:
    case Runtime::kInlineMathAcos:
    case Runtime::kInlineMathAsin:
    case Runtime::kInlineMathAtan:
    case Runtime::kInlineMathAtan2:
      return Type::Number();
    case Runtime::kInlineMathClz32:
      return Type::Range(0, 32, zone());
    case Runtime::kInlineCreateIterResultObject:
    case Runtime::kInlineRegExpConstructResult:
      return Type::OtherObject();
    case Runtime::kInlineSubString:
    case Runtime::kInlineStringCharFromCode:
      return Type::String();
    case Runtime::kInlineToInteger:
      return TypeUnaryOp(node, ToInteger);
    case Runtime::kInlineToLength:
      return TypeUnaryOp(node, ToLength);
    case Runtime::kInlineToName:
      return TypeUnaryOp(node, ToName);
    case Runtime::kInlineToNumber:
      return TypeUnaryOp(node, ToNumber);
    case Runtime::kInlineToObject:
      return TypeUnaryOp(node, ToObject);
    case Runtime::kInlineToPrimitive:
    case Runtime::kInlineToPrimitive_Number:
    case Runtime::kInlineToPrimitive_String:
      return TypeUnaryOp(node, ToPrimitive);
    case Runtime::kInlineToString:
      return TypeUnaryOp(node, ToString);
    case Runtime::kHasInPrototypeChain:
      return Type::Boolean();
    default:
      break;
  }
  return Type::Any();
}


Type* Typer::Visitor::TypeJSConvertReceiver(Node* node) {
  return Type::Receiver();
}


Type* Typer::Visitor::TypeJSForInNext(Node* node) {
  return Type::Union(Type::Name(), Type::Undefined(), zone());
}


Type* Typer::Visitor::TypeJSForInPrepare(Node* node) {
  STATIC_ASSERT(Map::EnumLengthBits::kMax <= FixedArray::kMaxLength);
  Factory* const f = isolate()->factory();
  Type* const cache_type = Type::Union(
      typer_->cache_.kSmi, Type::Class(f->meta_map(), zone()), zone());
  Type* const cache_array = Type::Class(f->fixed_array_map(), zone());
  Type* const cache_length = typer_->cache_.kFixedArrayLengthType;
  return Type::Tuple(cache_type, cache_array, cache_length, zone());
}

Type* Typer::Visitor::TypeJSForInDone(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeJSForInStep(Node* node) {
  STATIC_ASSERT(Map::EnumLengthBits::kMax <= FixedArray::kMaxLength);
  return Type::Range(1, FixedArray::kMaxLength + 1, zone());
}


Type* Typer::Visitor::TypeJSLoadMessage(Node* node) { return Type::Any(); }


Type* Typer::Visitor::TypeJSStoreMessage(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeJSStackCheck(Node* node) { return Type::Any(); }


// Simplified operators.

Type* Typer::Visitor::TypeBooleanNot(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeBooleanToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}

Type* Typer::Visitor::TypeNumberEqual(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeNumberLessThan(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeNumberLessThanOrEqual(Node* node) {
  return Type::Boolean();
}

Type* Typer::Visitor::TypeNumberAdd(Node* node) { return Type::Number(); }

Type* Typer::Visitor::TypeNumberSubtract(Node* node) { return Type::Number(); }

Type* Typer::Visitor::TypeNumberMultiply(Node* node) { return Type::Number(); }

Type* Typer::Visitor::TypeNumberDivide(Node* node) { return Type::Number(); }

Type* Typer::Visitor::TypeNumberModulus(Node* node) { return Type::Number(); }

Type* Typer::Visitor::TypeNumberBitwiseOr(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeNumberBitwiseXor(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeNumberBitwiseAnd(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeNumberShiftLeft(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeNumberShiftRight(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeNumberShiftRightLogical(Node* node) {
  return Type::Unsigned32();
}


Type* Typer::Visitor::TypeNumberToInt32(Node* node) {
  return TypeUnaryOp(node, NumberToInt32);
}


Type* Typer::Visitor::TypeNumberToUint32(Node* node) {
  return TypeUnaryOp(node, NumberToUint32);
}


Type* Typer::Visitor::TypeNumberIsHoleNaN(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypePlainPrimitiveToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}


// static
Type* Typer::Visitor::ReferenceEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    return t->singleton_true_;
  }
  return Type::Boolean();
}


Type* Typer::Visitor::TypeReferenceEqual(Node* node) {
  return TypeBinaryOp(node, ReferenceEqualTyper);
}

Type* Typer::Visitor::TypeStringEqual(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeStringLessThan(Node* node) { return Type::Boolean(); }

Type* Typer::Visitor::TypeStringLessThanOrEqual(Node* node) {
  return Type::Boolean();
}


namespace {

Type* ChangeRepresentation(Type* type, Type* rep, Zone* zone) {
  return Type::Union(Type::Semantic(type, zone),
                     Type::Representation(rep, zone), zone);
}

}  // namespace


Type* Typer::Visitor::TypeChangeTaggedToInt32(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg->Is(Type::Signed32()));
  return ChangeRepresentation(arg, Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeChangeTaggedToUint32(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg->Is(Type::Unsigned32()));
  return ChangeRepresentation(arg, Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeChangeTaggedToFloat64(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg->Is(Type::Number()));
  return ChangeRepresentation(arg, Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeChangeInt32ToTagged(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg->Is(Type::Signed32()));
  Type* rep =
      arg->Is(Type::SignedSmall()) ? Type::TaggedSigned() : Type::Tagged();
  return ChangeRepresentation(arg, rep, zone());
}


Type* Typer::Visitor::TypeChangeUint32ToTagged(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg->Is(Type::Unsigned32()));
  return ChangeRepresentation(arg, Type::Tagged(), zone());
}


Type* Typer::Visitor::TypeChangeFloat64ToTagged(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): CHECK(arg.upper->Is(Type::Number()));
  return ChangeRepresentation(arg, Type::Tagged(), zone());
}


Type* Typer::Visitor::TypeChangeBoolToBit(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Boolean()));
  return ChangeRepresentation(arg, Type::UntaggedBit(), zone());
}


Type* Typer::Visitor::TypeChangeBitToBool(Node* node) {
  Type* arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Boolean()));
  return ChangeRepresentation(arg, Type::TaggedPointer(), zone());
}


Type* Typer::Visitor::TypeAllocate(Node* node) { return Type::TaggedPointer(); }


namespace {

MaybeHandle<Map> GetStableMapFromObjectType(Type* object_type) {
  if (object_type->IsConstant() &&
      object_type->AsConstant()->Value()->IsHeapObject()) {
    Handle<Map> object_map(
        Handle<HeapObject>::cast(object_type->AsConstant()->Value())->map());
    if (object_map->is_stable()) return object_map;
  } else if (object_type->IsClass()) {
    Handle<Map> object_map = object_type->AsClass()->Map();
    if (object_map->is_stable()) return object_map;
  }
  return MaybeHandle<Map>();
}

}  // namespace


Type* Typer::Visitor::TypeLoadField(Node* node) {
  FieldAccess const& access = FieldAccessOf(node->op());
  if (access.base_is_tagged == kTaggedBase &&
      access.offset == HeapObject::kMapOffset) {
    // The type of LoadField[Map](o) is Constant(map) if map is stable and
    // either
    //  (a) o has type Constant(object) and map == object->map, or
    //  (b) o has type Class(map),
    // and either
    //  (1) map cannot transition further, or
    //  (2) deoptimization is enabled and we can add a code dependency on the
    //      stability of map (to guard the Constant type information).
    Type* const object = Operand(node, 0);
    if (object->Is(Type::None())) return Type::None();
    Handle<Map> object_map;
    if (GetStableMapFromObjectType(object).ToHandle(&object_map)) {
      if (object_map->CanTransition()) {
        if (flags() & kDeoptimizationEnabled) {
          dependencies()->AssumeMapStable(object_map);
        } else {
          return access.type;
        }
      }
      Type* object_map_type = Type::Constant(object_map, zone());
      DCHECK(object_map_type->Is(access.type));
      return object_map_type;
    }
  }
  return access.type;
}


Type* Typer::Visitor::TypeLoadBuffer(Node* node) {
  // TODO(bmeurer): This typing is not yet correct. Since we can still access
  // out of bounds, the type in the general case has to include Undefined.
  switch (BufferAccessOf(node->op()).external_array_type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return typer_->cache_.k##Type;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeLoadElement(Node* node) {
  return ElementAccessOf(node->op()).type;
}


Type* Typer::Visitor::TypeStoreField(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeStoreBuffer(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeStoreElement(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeObjectIsNumber(Node* node) {
  return TypeUnaryOp(node, ObjectIsNumber);
}


Type* Typer::Visitor::TypeObjectIsReceiver(Node* node) {
  return TypeUnaryOp(node, ObjectIsReceiver);
}


Type* Typer::Visitor::TypeObjectIsSmi(Node* node) {
  return TypeUnaryOp(node, ObjectIsSmi);
}


// Machine operators.

Type* Typer::Visitor::TypeLoad(Node* node) { return Type::Any(); }

Type* Typer::Visitor::TypeStackSlot(Node* node) { return Type::Any(); }

Type* Typer::Visitor::TypeStore(Node* node) {
  UNREACHABLE();
  return nullptr;
}


Type* Typer::Visitor::TypeWord32And(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Or(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Xor(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Shl(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Shr(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Sar(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Ror(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Equal(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeWord32Clz(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32Ctz(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeWord32ReverseBits(Node* node) {
  return Type::Integral32();
}


Type* Typer::Visitor::TypeWord32Popcnt(Node* node) {
  return Type::Integral32();
}


Type* Typer::Visitor::TypeWord64And(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Or(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Xor(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Shl(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Shr(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Sar(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Ror(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Clz(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Ctz(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64ReverseBits(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeWord64Popcnt(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeWord64Equal(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeInt32Add(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeInt32AddWithOverflow(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeInt32Sub(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeInt32SubWithOverflow(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeInt32Mul(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeInt32MulHigh(Node* node) { return Type::Signed32(); }


Type* Typer::Visitor::TypeInt32Div(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeInt32Mod(Node* node) { return Type::Integral32(); }


Type* Typer::Visitor::TypeInt32LessThan(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeInt32LessThanOrEqual(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeUint32Div(Node* node) { return Type::Unsigned32(); }


Type* Typer::Visitor::TypeUint32LessThan(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeUint32LessThanOrEqual(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeUint32Mod(Node* node) { return Type::Unsigned32(); }


Type* Typer::Visitor::TypeUint32MulHigh(Node* node) {
  return Type::Unsigned32();
}


Type* Typer::Visitor::TypeInt64Add(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeInt64AddWithOverflow(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeInt64Sub(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeInt64SubWithOverflow(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeInt64Mul(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeInt64Div(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeInt64Mod(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeInt64LessThan(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeInt64LessThanOrEqual(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeUint64Div(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeUint64LessThan(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeUint64LessThanOrEqual(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeUint64Mod(Node* node) { return Type::Internal(); }


Type* Typer::Visitor::TypeChangeFloat32ToFloat64(Node* node) {
  return Type::Intersect(Type::Number(), Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeChangeFloat64ToInt32(Node* node) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeChangeFloat64ToUint32(Node* node) {
  return Type::Intersect(Type::Unsigned32(), Type::UntaggedIntegral32(),
                         zone());
}


Type* Typer::Visitor::TypeTruncateFloat32ToInt32(Node* node) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeTruncateFloat32ToUint32(Node* node) {
  return Type::Intersect(Type::Unsigned32(), Type::UntaggedIntegral32(),
                         zone());
}


Type* Typer::Visitor::TypeTryTruncateFloat32ToInt64(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeTryTruncateFloat64ToInt64(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeTryTruncateFloat32ToUint64(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeTryTruncateFloat64ToUint64(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeChangeInt32ToFloat64(Node* node) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeChangeInt32ToInt64(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeChangeUint32ToFloat64(Node* node) {
  return Type::Intersect(Type::Unsigned32(), Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeChangeUint32ToUint64(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeTruncateFloat64ToFloat32(Node* node) {
  return Type::Intersect(Type::Number(), Type::UntaggedFloat32(), zone());
}


Type* Typer::Visitor::TypeTruncateFloat64ToInt32(Node* node) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeTruncateInt64ToInt32(Node* node) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedIntegral32(), zone());
}


Type* Typer::Visitor::TypeRoundInt32ToFloat32(Node* node) {
  return Type::Intersect(Type::PlainNumber(), Type::UntaggedFloat32(), zone());
}


Type* Typer::Visitor::TypeRoundInt64ToFloat32(Node* node) {
  return Type::Intersect(Type::PlainNumber(), Type::UntaggedFloat32(), zone());
}


Type* Typer::Visitor::TypeRoundInt64ToFloat64(Node* node) {
  return Type::Intersect(Type::PlainNumber(), Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeRoundUint32ToFloat32(Node* node) {
  return Type::Intersect(Type::PlainNumber(), Type::UntaggedFloat32(), zone());
}


Type* Typer::Visitor::TypeRoundUint64ToFloat32(Node* node) {
  return Type::Intersect(Type::PlainNumber(), Type::UntaggedFloat32(), zone());
}


Type* Typer::Visitor::TypeRoundUint64ToFloat64(Node* node) {
  return Type::Intersect(Type::PlainNumber(), Type::UntaggedFloat64(), zone());
}


Type* Typer::Visitor::TypeBitcastFloat32ToInt32(Node* node) {
  return Type::Number();
}


Type* Typer::Visitor::TypeBitcastFloat64ToInt64(Node* node) {
  return Type::Number();
}


Type* Typer::Visitor::TypeBitcastInt32ToFloat32(Node* node) {
  return Type::Number();
}


Type* Typer::Visitor::TypeBitcastInt64ToFloat64(Node* node) {
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat32Add(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Sub(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Mul(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Div(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Max(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Min(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Abs(Node* node) {
  // TODO(turbofan): We should be able to infer a better type here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat32Sqrt(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat32Equal(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeFloat32LessThan(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeFloat32LessThanOrEqual(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeFloat64Add(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Sub(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Mul(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Div(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Mod(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Max(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Min(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Abs(Node* node) {
  // TODO(turbofan): We should be able to infer a better type here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64Sqrt(Node* node) { return Type::Number(); }


Type* Typer::Visitor::TypeFloat64Equal(Node* node) { return Type::Boolean(); }


Type* Typer::Visitor::TypeFloat64LessThan(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeFloat64LessThanOrEqual(Node* node) {
  return Type::Boolean();
}


Type* Typer::Visitor::TypeFloat32RoundDown(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64RoundDown(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat32RoundUp(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64RoundUp(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat32RoundTruncate(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64RoundTruncate(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64RoundTiesAway(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat32RoundTiesEven(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64RoundTiesEven(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64ExtractLowWord32(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeFloat64ExtractHighWord32(Node* node) {
  return Type::Signed32();
}


Type* Typer::Visitor::TypeFloat64InsertLowWord32(Node* node) {
  return Type::Number();
}


Type* Typer::Visitor::TypeFloat64InsertHighWord32(Node* node) {
  return Type::Number();
}


Type* Typer::Visitor::TypeLoadStackPointer(Node* node) {
  return Type::Internal();
}


Type* Typer::Visitor::TypeLoadFramePointer(Node* node) {
  return Type::Internal();
}

Type* Typer::Visitor::TypeLoadParentFramePointer(Node* node) {
  return Type::Internal();
}

Type* Typer::Visitor::TypeCheckedLoad(Node* node) { return Type::Any(); }


Type* Typer::Visitor::TypeCheckedStore(Node* node) {
  UNREACHABLE();
  return nullptr;
}


// Heap constants.


Type* Typer::Visitor::TypeConstant(Handle<Object> value) {
  if (value->IsJSTypedArray()) {
    switch (JSTypedArray::cast(*value)->type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return typer_->cache_.k##Type##Array;
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    }
  }
  if (Type::IsInteger(*value)) {
    return Type::Range(value->Number(), value->Number(), zone());
  }
  return Type::Constant(value, zone());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
