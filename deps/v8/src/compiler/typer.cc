// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/typer.h"

namespace v8 {
namespace internal {
namespace compiler {

class Typer::Decorator : public GraphDecorator {
 public:
  explicit Decorator(Typer* typer) : typer_(typer) {}
  virtual void Decorate(Node* node);

 private:
  Typer* typer_;
};


Typer::Typer(Graph* graph, MaybeHandle<Context> context)
    : graph_(graph),
      context_(context),
      decorator_(NULL),
      weaken_min_limits_(graph->zone()),
      weaken_max_limits_(graph->zone()) {
  Zone* zone = this->zone();
  Factory* f = zone->isolate()->factory();

  Handle<Object> zero = f->NewNumber(0);
  Handle<Object> one = f->NewNumber(1);
  Handle<Object> infinity = f->NewNumber(+V8_INFINITY);
  Handle<Object> minusinfinity = f->NewNumber(-V8_INFINITY);

  negative_signed32 = Type::Union(
      Type::SignedSmall(), Type::OtherSigned32(), zone);
  non_negative_signed32 = Type::Union(
      Type::UnsignedSmall(), Type::OtherUnsigned31(), zone);
  undefined_or_null = Type::Union(Type::Undefined(), Type::Null(), zone);
  singleton_false = Type::Constant(f->false_value(), zone);
  singleton_true = Type::Constant(f->true_value(), zone);
  singleton_zero = Type::Range(zero, zero, zone);
  singleton_one = Type::Range(one, one, zone);
  zero_or_one = Type::Union(singleton_zero, singleton_one, zone);
  zeroish = Type::Union(
      singleton_zero, Type::Union(Type::NaN(), Type::MinusZero(), zone), zone);
  falsish = Type::Union(Type::Undetectable(),
      Type::Union(zeroish, undefined_or_null, zone), zone);
  integer = Type::Range(minusinfinity, infinity, zone);
  weakint = Type::Union(
      integer, Type::Union(Type::NaN(), Type::MinusZero(), zone), zone);

  Type* number = Type::Number();
  Type* signed32 = Type::Signed32();
  Type* unsigned32 = Type::Unsigned32();
  Type* integral32 = Type::Integral32();
  Type* object = Type::Object();
  Type* undefined = Type::Undefined();

  number_fun0_ = Type::Function(number, zone);
  number_fun1_ = Type::Function(number, number, zone);
  number_fun2_ = Type::Function(number, number, number, zone);
  weakint_fun1_ = Type::Function(weakint, number, zone);
  imul_fun_ = Type::Function(signed32, integral32, integral32, zone);
  clz32_fun_ = Type::Function(
      Type::Range(zero, f->NewNumber(32), zone), number, zone);
  random_fun_ = Type::Function(Type::Union(
      Type::UnsignedSmall(), Type::OtherNumber(), zone), zone);

  Type* int8 = Type::Intersect(
      Type::Range(f->NewNumber(-0x7F), f->NewNumber(0x7F-1), zone),
      Type::UntaggedInt8(), zone);
  Type* int16 = Type::Intersect(
      Type::Range(f->NewNumber(-0x7FFF), f->NewNumber(0x7FFF-1), zone),
      Type::UntaggedInt16(), zone);
  Type* uint8 = Type::Intersect(
      Type::Range(zero, f->NewNumber(0xFF-1), zone),
      Type::UntaggedInt8(), zone);
  Type* uint16 = Type::Intersect(
      Type::Range(zero, f->NewNumber(0xFFFF-1), zone),
      Type::UntaggedInt16(), zone);

#define NATIVE_TYPE(sem, rep) \
    Type::Intersect(Type::sem(), Type::rep(), zone)
  Type* int32 = NATIVE_TYPE(Signed32, UntaggedInt32);
  Type* uint32 = NATIVE_TYPE(Unsigned32, UntaggedInt32);
  Type* float32 = NATIVE_TYPE(Number, UntaggedFloat32);
  Type* float64 = NATIVE_TYPE(Number, UntaggedFloat64);
#undef NATIVE_TYPE

  Type* buffer = Type::Buffer(zone);
  Type* int8_array = Type::Array(int8, zone);
  Type* int16_array = Type::Array(int16, zone);
  Type* int32_array = Type::Array(int32, zone);
  Type* uint8_array = Type::Array(uint8, zone);
  Type* uint16_array = Type::Array(uint16, zone);
  Type* uint32_array = Type::Array(uint32, zone);
  Type* float32_array = Type::Array(float32, zone);
  Type* float64_array = Type::Array(float64, zone);
  Type* arg1 = Type::Union(unsigned32, object, zone);
  Type* arg2 = Type::Union(unsigned32, undefined, zone);
  Type* arg3 = arg2;
  array_buffer_fun_ = Type::Function(buffer, unsigned32, zone);
  int8_array_fun_ = Type::Function(int8_array, arg1, arg2, arg3, zone);
  int16_array_fun_ = Type::Function(int16_array, arg1, arg2, arg3, zone);
  int32_array_fun_ = Type::Function(int32_array, arg1, arg2, arg3, zone);
  uint8_array_fun_ = Type::Function(uint8_array, arg1, arg2, arg3, zone);
  uint16_array_fun_ = Type::Function(uint16_array, arg1, arg2, arg3, zone);
  uint32_array_fun_ = Type::Function(uint32_array, arg1, arg2, arg3, zone);
  float32_array_fun_ = Type::Function(float32_array, arg1, arg2, arg3, zone);
  float64_array_fun_ = Type::Function(float64_array, arg1, arg2, arg3, zone);

  const int limits_count = 20;

  weaken_min_limits_.reserve(limits_count + 1);
  weaken_max_limits_.reserve(limits_count + 1);

  double limit = 1 << 30;
  weaken_min_limits_.push_back(f->NewNumber(0));
  weaken_max_limits_.push_back(f->NewNumber(0));
  for (int i = 0; i < limits_count; i++) {
    weaken_min_limits_.push_back(f->NewNumber(-limit));
    weaken_max_limits_.push_back(f->NewNumber(limit - 1));
    limit *= 2;
  }

  decorator_ = new (zone) Decorator(this);
  graph_->AddDecorator(decorator_);
}


Typer::~Typer() {
  graph_->RemoveDecorator(decorator_);
}


class Typer::Visitor : public NullNodeVisitor {
 public:
  explicit Visitor(Typer* typer) : typer_(typer) {}

  Bounds TypeNode(Node* node) {
    switch (node->opcode()) {
#define DECLARE_CASE(x) \
      case IrOpcode::k##x: return TypeBinaryOp(node, x##Typer);
      JS_SIMPLE_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) case IrOpcode::k##x: return Type##x(node);
      DECLARE_CASE(Start)
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
      DECLARE_CASE(End)
      INNER_CONTROL_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
      break;
    }
    UNREACHABLE();
    return Bounds();
  }

  Type* TypeConstant(Handle<Object> value);

 protected:
#define DECLARE_METHOD(x) inline Bounds Type##x(Node* node);
  DECLARE_METHOD(Start)
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

  Bounds ContextOperand(Node* node) {
    Bounds result = BoundsOrNone(NodeProperties::GetContextInput(node));
    DCHECK(result.upper->Maybe(Type::Internal()));
    // TODO(rossberg): More precisely, instead of the above assertion, we should
    // back-propagate the constraint that it has to be a subtype of Internal.
    return result;
  }

  Type* Weaken(Type* current_type, Type* previous_type);

  Zone* zone() { return typer_->zone(); }
  Isolate* isolate() { return typer_->isolate(); }
  Graph* graph() { return typer_->graph(); }
  MaybeHandle<Context> context() { return typer_->context(); }

 private:
  Typer* typer_;
  MaybeHandle<Context> context_;

  typedef Type* (*UnaryTyperFun)(Type*, Typer* t);
  typedef Type* (*BinaryTyperFun)(Type*, Type*, Typer* t);

  Bounds TypeUnaryOp(Node* node, UnaryTyperFun);
  Bounds TypeBinaryOp(Node* node, BinaryTyperFun);

  static Type* Invert(Type*, Typer*);
  static Type* FalsifyUndefined(Type*, Typer*);
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

  static Type* JSCompareTyper(Type*, Type*, Typer*);

#define DECLARE_METHOD(x) static Type* x##Typer(Type*, Type*, Typer*);
  JS_SIMPLE_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  static Type* JSUnaryNotTyper(Type*, Typer*);
  static Type* JSLoadPropertyTyper(Type*, Type*, Typer*);
  static Type* JSCallFunctionTyper(Type*, Typer*);
};


class Typer::RunVisitor : public Typer::Visitor {
 public:
  explicit RunVisitor(Typer* typer)
      : Visitor(typer),
        redo(NodeSet::key_compare(), NodeSet::allocator_type(typer->zone())) {}

  void Post(Node* node) {
    if (node->op()->ValueOutputCount() > 0) {
      Bounds bounds = TypeNode(node);
      NodeProperties::SetBounds(node, bounds);
      // Remember incompletely typed nodes for least fixpoint iteration.
      if (!NodeProperties::AllValueInputsAreTyped(node)) redo.insert(node);
    }
  }

  NodeSet redo;
};


class Typer::WidenVisitor : public Typer::Visitor {
 public:
  explicit WidenVisitor(Typer* typer)
      : Visitor(typer),
        local_zone_(zone()->isolate()),
        enabled_(graph()->NodeCount(), true, &local_zone_),
        queue_(&local_zone_) {}

  void Run(NodeSet* nodes) {
    // Queue all the roots.
    for (Node* node : *nodes) {
      Queue(node);
    }

    while (!queue_.empty()) {
      Node* node = queue_.front();
      queue_.pop();

      if (node->op()->ValueOutputCount() > 0) {
        // Enable future queuing (and thus re-typing) of this node.
        enabled_[node->id()] = true;

        // Compute the new type.
        Bounds previous = BoundsOrNone(node);
        Bounds current = TypeNode(node);

        // Speed up termination in the presence of range types:
        current.upper = Weaken(current.upper, previous.upper);
        current.lower = Weaken(current.lower, previous.lower);

        // Types should not get less precise.
        DCHECK(previous.lower->Is(current.lower));
        DCHECK(previous.upper->Is(current.upper));

        NodeProperties::SetBounds(node, current);
        // If something changed, push all uses into the queue.
        if (!(previous.Narrows(current) && current.Narrows(previous))) {
          for (Node* use : node->uses()) {
            Queue(use);
          }
        }
      }
      // If there is no value output, we deliberately leave the node disabled
      // for queuing - there is no need to type it.
    }
  }

  void Queue(Node* node) {
    // If the node is enabled for queuing, push it to the queue and disable it
    // (to avoid queuing it multiple times).
    if (enabled_[node->id()]) {
      queue_.push(node);
      enabled_[node->id()] = false;
    }
  }

 private:
  Zone local_zone_;
  BoolVector enabled_;
  ZoneQueue<Node*> queue_;
};


void Typer::Run() {
  RunVisitor typing(this);
  graph_->VisitNodeInputsFromEnd(&typing);
  // Find least fixpoint.
  WidenVisitor widen(this);
  widen.Run(&typing.redo);
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
  Type* upper = input.upper->Is(Type::None())
      ? Type::None()
      : f(input.upper, typer_);
  Type* lower = input.lower->Is(Type::None())
      ? Type::None()
      : (input.lower == input.upper || upper->IsConstant())
      ? upper  // TODO(neis): Extend this to Range(x,x), NaN, MinusZero, ...?
      : f(input.lower, typer_);
  // TODO(neis): Figure out what to do with lower bound.
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeBinaryOp(Node* node, BinaryTyperFun f) {
  Bounds left = Operand(node, 0);
  Bounds right = Operand(node, 1);
  Type* upper = left.upper->Is(Type::None()) || right.upper->Is(Type::None())
      ? Type::None()
      : f(left.upper, right.upper, typer_);
  Type* lower = left.lower->Is(Type::None()) || right.lower->Is(Type::None())
      ? Type::None()
      : ((left.lower == left.upper && right.lower == right.upper) ||
         upper->IsConstant())
      ? upper
      : f(left.lower, right.lower, typer_);
  // TODO(neis): Figure out what to do with lower bound.
  return Bounds(lower, upper);
}


Type* Typer::Visitor::Invert(Type* type, Typer* t) {
  if (type->Is(t->singleton_false)) return t->singleton_true;
  if (type->Is(t->singleton_true)) return t->singleton_false;
  return type;
}


Type* Typer::Visitor::FalsifyUndefined(Type* type, Typer* t) {
  if (type->Is(Type::Undefined())) return t->singleton_false;
  return type;
}


Type* Typer::Visitor::Rangify(Type* type, Typer* t) {
  if (type->IsRange()) return type;        // Shortcut.
  if (!type->Is(t->integer) && !type->Is(Type::Integral32())) {
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
  Factory* f = t->isolate()->factory();
  return Type::Range(f->NewNumber(min), f->NewNumber(max), t->zone());
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
  if (type->Is(t->falsish)) return t->singleton_false;
  if (type->Is(Type::DetectableReceiver())) return t->singleton_true;
  if (type->Is(Type::OrderedNumber()) && (type->Max() < 0 || 0 < type->Min())) {
    return t->singleton_true;  // Ruled out nan, -0 and +0.
  }
  return Type::Boolean();
}


Type* Typer::Visitor::ToNumber(Type* type, Typer* t) {
  if (type->Is(Type::Number())) return type;
  if (type->Is(Type::Undefined())) return Type::NaN();
  if (type->Is(t->singleton_false)) return t->singleton_zero;
  if (type->Is(t->singleton_true)) return t->singleton_one;
  if (type->Is(Type::Boolean())) return t->zero_or_one;
  return Type::Number();
}


Type* Typer::Visitor::ToString(Type* type, Typer* t) {
  if (type->Is(Type::String())) return type;
  return Type::String();
}


Type* Typer::Visitor::NumberToInt32(Type* type, Typer* t) {
  // TODO(neis): DCHECK(type->Is(Type::Number()));
  if (type->Is(Type::Signed32())) return type;
  if (type->Is(t->zeroish)) return t->singleton_zero;
  return Type::Signed32();
}


Type* Typer::Visitor::NumberToUint32(Type* type, Typer* t) {
  // TODO(neis): DCHECK(type->Is(Type::Number()));
  if (type->Is(Type::Unsigned32())) return type;
  if (type->Is(t->zeroish)) return t->singleton_zero;
  return Type::Unsigned32();
}


// -----------------------------------------------------------------------------


// Control operators.


Bounds Typer::Visitor::TypeStart(Node* node) {
  return Bounds(Type::None(zone()), Type::Internal(zone()));
}


// Common operators.


Bounds Typer::Visitor::TypeParameter(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeInt32Constant(Node* node) {
  Factory* f = isolate()->factory();
  Handle<Object> number = f->NewNumber(OpParameter<int32_t>(node));
  return Bounds(Type::Intersect(
      Type::Range(number, number, zone()), Type::UntaggedInt32(), zone()));
}


Bounds Typer::Visitor::TypeInt64Constant(Node* node) {
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
  return Bounds(TypeConstant(OpParameter<Unique<Object> >(node).handle()));
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


Bounds Typer::Visitor::TypeCall(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeProjection(Node* node) {
  // TODO(titzer): use the output type of the input to determine the bounds.
  return Bounds::Unbounded(zone());
}


// JS comparison operators.


Type* Typer::Visitor::JSEqualTyper(Type* lhs, Type* rhs, Typer* t) {
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return t->singleton_false;
  if (lhs->Is(t->undefined_or_null) && rhs->Is(t->undefined_or_null)) {
    return t->singleton_true;
  }
  if (lhs->Is(Type::Number()) && rhs->Is(Type::Number()) &&
      (lhs->Max() < rhs->Min() || lhs->Min() > rhs->Max())) {
      return t->singleton_false;
  }
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value,
    // which is not nan due to the earlier check.
    // TODO(neis): Extend this to Range(x,x), MinusZero, ...?
    return t->singleton_true;
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
  if (!JSType(lhs)->Maybe(JSType(rhs))) return t->singleton_false;
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return t->singleton_false;
  if (lhs->Is(Type::Number()) && rhs->Is(Type::Number()) &&
      (lhs->Max() < rhs->Min() || lhs->Min() > rhs->Max())) {
      return t->singleton_false;
  }
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value,
    // which is not nan due to the earlier check.
    return t->singleton_true;
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
Type* Typer::Visitor::JSCompareTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = ToPrimitive(lhs, t);
  rhs = ToPrimitive(rhs, t);
  if (lhs->Maybe(Type::String()) && rhs->Maybe(Type::String())) {
    return Type::Boolean();
  }
  lhs = ToNumber(lhs, t);
  rhs = ToNumber(rhs, t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::Undefined();
  if (lhs->IsConstant() && rhs->Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value,
    // which is not NaN due to the previous check.
    return t->singleton_false;
  }
  if (lhs->Min() >= rhs->Max()) return t->singleton_false;
  if (lhs->Max() < rhs->Min() &&
      !lhs->Maybe(Type::NaN()) && !rhs->Maybe(Type::NaN())) {
    return t->singleton_true;
  }
  return Type::Boolean();
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
  Factory* f = t->isolate()->factory();
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  rhs = NumberToInt32(ToNumber(rhs, t), t);
  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  // Or-ing any two values results in a value no smaller than their minimum.
  // Even no smaller than their maximum if both values are non-negative.
  Handle<Object> min = f->NewNumber(
      lmin >= 0 && rmin >= 0 ? std::max(lmin, rmin) : std::min(lmin, rmin));
  if (lmax < 0 || rmax < 0) {
    // Or-ing two values of which at least one is negative results in a negative
    // value.
    Handle<Object> max = f->NewNumber(-1);
    return Type::Range(min, max, t->zone());
  }
  Handle<Object> max = f->NewNumber(Type::Signed32()->Max());
  return Type::Range(min, max, t->zone());
  // TODO(neis): Be precise for singleton inputs, here and elsewhere.
}


Type* Typer::Visitor::JSBitwiseAndTyper(Type* lhs, Type* rhs, Typer* t) {
  Factory* f = t->isolate()->factory();
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  rhs = NumberToInt32(ToNumber(rhs, t), t);
  double lmin = lhs->Min();
  double rmin = rhs->Min();
  double lmax = lhs->Max();
  double rmax = rhs->Max();
  // And-ing any two values results in a value no larger than their maximum.
  // Even no larger than their minimum if both values are non-negative.
  Handle<Object> max = f->NewNumber(
      lmin >= 0 && rmin >= 0 ? std::min(lmax, rmax) : std::max(lmax, rmax));
  if (lmin >= 0 || rmin >= 0) {
    // And-ing two values of which at least one is non-negative results in a
    // non-negative value.
    Handle<Object> min = f->NewNumber(0);
    return Type::Range(min, max, t->zone());
  }
  Handle<Object> min = f->NewNumber(Type::Signed32()->Min());
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
    return t->non_negative_signed32;
  }
  if ((lmax < 0 && rmin >= 0) || (lmin >= 0 && rmax < 0)) {
    // Xor-ing a negative and a non-negative value results in a negative value.
    return t->negative_signed32;
  }
  return Type::Signed32();
}


Type* Typer::Visitor::JSShiftLeftTyper(Type* lhs, Type* rhs, Typer* t) {
  return Type::Signed32();
}


Type* Typer::Visitor::JSShiftRightTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToInt32(ToNumber(lhs, t), t);
  Factory* f = t->isolate()->factory();
  if (lhs->Min() >= 0) {
    // Right-shifting a non-negative value cannot make it negative, nor larger.
    Handle<Object> min = f->NewNumber(0);
    Handle<Object> max = f->NewNumber(lhs->Max());
    return Type::Range(min, max, t->zone());
  }
  if (lhs->Max() < 0) {
    // Right-shifting a negative value cannot make it non-negative, nor smaller.
    Handle<Object> min = f->NewNumber(lhs->Min());
    Handle<Object> max = f->NewNumber(-1);
    return Type::Range(min, max, t->zone());
  }
  return Type::Signed32();
}


Type* Typer::Visitor::JSShiftRightLogicalTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = NumberToUint32(ToNumber(lhs, t), t);
  Factory* f = t->isolate()->factory();
  // Logical right-shifting any value cannot make it larger.
  Handle<Object> min = f->NewNumber(0);
  Handle<Object> max = f->NewNumber(lhs->Max());
  return Type::Range(min, max, t->zone());
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
  results[0] = lhs->Min()->Number() + rhs->Min()->Number();
  results[1] = lhs->Min()->Number() + rhs->Max()->Number();
  results[2] = lhs->Max()->Number() + rhs->Min()->Number();
  results[3] = lhs->Max()->Number() + rhs->Max()->Number();
  // Since none of the inputs can be -0, the result cannot be -0 either.
  // However, it can be nan (the sum of two infinities of opposite sign).
  // On the other hand, if none of the "results" above is nan, then the actual
  // result cannot be nan either.
  int nans = 0;
  for (int i = 0; i < 4; ++i) {
    if (std::isnan(results[i])) ++nans;
  }
  if (nans == 4) return Type::NaN();  // [-inf..-inf] + [inf..inf] or vice versa
  Factory* f = t->isolate()->factory();
  Type* range = Type::Range(f->NewNumber(array_min(results, 4)),
                            f->NewNumber(array_max(results, 4)), t->zone());
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
  results[0] = lhs->Min()->Number() - rhs->Min()->Number();
  results[1] = lhs->Min()->Number() - rhs->Max()->Number();
  results[2] = lhs->Max()->Number() - rhs->Min()->Number();
  results[3] = lhs->Max()->Number() - rhs->Max()->Number();
  // Since none of the inputs can be -0, the result cannot be -0.
  // However, it can be nan (the subtraction of two infinities of same sign).
  // On the other hand, if none of the "results" above is nan, then the actual
  // result cannot be nan either.
  int nans = 0;
  for (int i = 0; i < 4; ++i) {
    if (std::isnan(results[i])) ++nans;
  }
  if (nans == 4) return Type::NaN();  // [inf..inf] - [inf..inf] (all same sign)
  Factory* f = t->isolate()->factory();
  Type* range = Type::Range(f->NewNumber(array_min(results, 4)),
                            f->NewNumber(array_max(results, 4)), t->zone());
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
  double lmin = lhs->Min()->Number();
  double lmax = lhs->Max()->Number();
  double rmin = rhs->Min()->Number();
  double rmax = rhs->Max()->Number();
  results[0] = lmin * rmin;
  results[1] = lmin * rmax;
  results[2] = lmax * rmin;
  results[3] = lmax * rmax;
  // If the result may be nan, we give up on calculating a precise type, because
  // the discontinuity makes it too complicated.  Note that even if none of the
  // "results" above is nan, the actual result may still be, so we have to do a
  // different check:
  bool maybe_nan = (lhs->Maybe(t->singleton_zero) &&
                    (rmin == -V8_INFINITY || rmax == +V8_INFINITY)) ||
                   (rhs->Maybe(t->singleton_zero) &&
                    (lmin == -V8_INFINITY || lmax == +V8_INFINITY));
  if (maybe_nan) return t->weakint;  // Giving up.
  bool maybe_minuszero = (lhs->Maybe(t->singleton_zero) && rmin < 0) ||
                         (rhs->Maybe(t->singleton_zero) && lmin < 0);
  Factory* f = t->isolate()->factory();
  Type* range = Type::Range(f->NewNumber(array_min(results, 4)),
                            f->NewNumber(array_max(results, 4)), t->zone());
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
      lhs->Maybe(Type::NaN()) || rhs->Maybe(t->zeroish) ||
      ((lhs->Min() == -V8_INFINITY || lhs->Max() == +V8_INFINITY) &&
       (rhs->Min() == -V8_INFINITY || rhs->Max() == +V8_INFINITY));
  return maybe_nan ? Type::Number() : Type::OrderedNumber();
}


Type* Typer::Visitor::JSModulusRanger(Type::RangeType* lhs,
                                      Type::RangeType* rhs, Typer* t) {
  double lmin = lhs->Min()->Number();
  double lmax = lhs->Max()->Number();
  double rmin = rhs->Min()->Number();
  double rmax = rhs->Max()->Number();

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

  Factory* f = t->isolate()->factory();
  Type* result = Type::Range(f->NewNumber(omin), f->NewNumber(omax), t->zone());
  if (maybe_minus_zero)
    result = Type::Union(result, Type::MinusZero(), t->zone());
  return result;
}


Type* Typer::Visitor::JSModulusTyper(Type* lhs, Type* rhs, Typer* t) {
  lhs = ToNumber(lhs, t);
  rhs = ToNumber(rhs, t);
  if (lhs->Is(Type::NaN()) || rhs->Is(Type::NaN())) return Type::NaN();

  if (lhs->Maybe(Type::NaN()) || rhs->Maybe(t->zeroish) ||
      lhs->Min() == -V8_INFINITY || lhs->Max() == +V8_INFINITY) {
    // Result maybe NaN.
    return Type::Number();
  }

  lhs = Rangify(lhs, t);
  rhs = Rangify(rhs, t);
  if (lhs->IsRange() && rhs->IsRange()) {
    // TODO(titzer): fix me.
    //    return JSModulusRanger(lhs->AsRange(), rhs->AsRange(), t);
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


Bounds Typer::Visitor::TypeJSTypeOf(Node* node) {
  return Bounds(Type::None(zone()), Type::InternalizedString(zone()));
}


// JS conversion operators.


Bounds Typer::Visitor::TypeJSToBoolean(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSToNumber(Node* node) {
  return Bounds(Type::None(zone()), Type::Number(zone()));
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


// Returns a somewhat larger range if we previously assigned
// a (smaller) range to this node. This is used  to speed up
// the fixpoint calculation in case there appears to be a loop
// in the graph. In the current implementation, we are
// increasing the limits to the closest power of two.
Type* Typer::Visitor::Weaken(Type* current_type, Type* previous_type) {
  if (current_type->IsRange() && previous_type->IsRange()) {
    Type::RangeType* previous = previous_type->AsRange();
    Type::RangeType* current = current_type->AsRange();

    double current_min = current->Min()->Number();
    Handle<Object> new_min = current->Min();

    // Find the closest lower entry in the list of allowed
    // minima (or negative infinity if there is no such entry).
    if (current_min != previous->Min()->Number()) {
      new_min = typer_->integer->AsRange()->Min();
      for (const auto val : typer_->weaken_min_limits_) {
        if (val->Number() <= current_min) {
          new_min = val;
          break;
        }
      }
    }

    double current_max = current->Max()->Number();
    Handle<Object> new_max = current->Max();
    // Find the closest greater entry in the list of allowed
    // maxima (or infinity if there is no such entry).
    if (current_max != previous->Max()->Number()) {
      new_max = typer_->integer->AsRange()->Max();
      for (const auto val : typer_->weaken_max_limits_) {
        if (val->Number() >= current_max) {
          new_max = val;
          break;
        }
      }
    }

    return Type::Range(new_min, new_max, typer_->zone());
  }
  return current_type;
}


Bounds Typer::Visitor::TypeJSStoreProperty(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeJSStoreNamed(Node* node) {
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
  Bounds outer = Operand(node, 0);
  DCHECK(outer.upper->Maybe(Type::Internal()));
  // TODO(rossberg): More precisely, instead of the above assertion, we should
  // back-propagate the constraint that it has to be a subtype of Internal.

  ContextAccess access = OpParameter<ContextAccess>(node);
  Type* context_type = outer.upper;
  MaybeHandle<Context> context;
  if (context_type->IsConstant()) {
    context = Handle<Context>::cast(context_type->AsConstant()->Value());
  }
  // Walk context chain (as far as known), mirroring dynamic lookup.
  // Since contexts are mutable, the information is only useful as a lower
  // bound.
  // TODO(rossberg): Could use scope info to fix upper bounds for constant
  // bindings if we know that this code is never shared.
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
  if (context.is_null()) {
    return Bounds::Unbounded(zone());
  } else {
    Handle<Object> value =
        handle(context.ToHandleChecked()->get(static_cast<int>(access.index())),
               isolate());
    Type* lower = TypeConstant(value);
    return Bounds(lower, Type::Any());
  }
}


Bounds Typer::Visitor::TypeJSStoreContext(Node* node) {
  UNREACHABLE();
  return Bounds();
}


Bounds Typer::Visitor::TypeJSCreateFunctionContext(Node* node) {
  Bounds outer = ContextOperand(node);
  return Bounds(Type::Context(outer.upper, zone()));
}


Bounds Typer::Visitor::TypeJSCreateCatchContext(Node* node) {
  Bounds outer = ContextOperand(node);
  return Bounds(Type::Context(outer.upper, zone()));
}


Bounds Typer::Visitor::TypeJSCreateWithContext(Node* node) {
  Bounds outer = ContextOperand(node);
  return Bounds(Type::Context(outer.upper, zone()));
}


Bounds Typer::Visitor::TypeJSCreateBlockContext(Node* node) {
  Bounds outer = ContextOperand(node);
  return Bounds(Type::Context(outer.upper, zone()));
}


Bounds Typer::Visitor::TypeJSCreateModuleContext(Node* node) {
  // TODO(rossberg): this is probably incorrect
  Bounds outer = ContextOperand(node);
  return Bounds(Type::Context(outer.upper, zone()));
}


Bounds Typer::Visitor::TypeJSCreateGlobalContext(Node* node) {
  Bounds outer = ContextOperand(node);
  return Bounds(Type::Context(outer.upper, zone()));
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
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSDebugger(Node* node) {
  return Bounds::Unbounded(zone());
}


// Simplified operators.


Bounds Typer::Visitor::TypeBooleanNot(Node* node) {
  return Bounds(Type::None(zone()), Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeBooleanToNumber(Node* node) {
  return Bounds(Type::None(zone()), typer_->zero_or_one);
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


Bounds Typer::Visitor::TypeNumberToInt32(Node* node) {
  return TypeUnaryOp(node, NumberToInt32);
}


Bounds Typer::Visitor::TypeNumberToUint32(Node* node) {
  return TypeUnaryOp(node, NumberToUint32);
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


Bounds Typer::Visitor::TypeStringAdd(Node* node) {
  return Bounds(Type::None(zone()), Type::String(zone()));
}


static Type* ChangeRepresentation(Type* type, Type* rep, Zone* zone) {
  // TODO(neis): Enable when expressible.
  /*
  return Type::Union(
      Type::Intersect(type, Type::Semantic(), zone),
      Type::Intersect(rep, Type::Representation(), zone), zone);
  */
  return type;
}


Bounds Typer::Visitor::TypeChangeTaggedToInt32(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Signed32()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::UntaggedInt32(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedInt32(), zone()));
}


Bounds Typer::Visitor::TypeChangeTaggedToUint32(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Unsigned32()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::UntaggedInt32(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedInt32(), zone()));
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
  return Bounds(
      ChangeRepresentation(arg.lower, Type::Tagged(), zone()),
      ChangeRepresentation(arg.upper, Type::Tagged(), zone()));
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
      ChangeRepresentation(arg.lower, Type::UntaggedInt1(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedInt1(), zone()));
}


Bounds Typer::Visitor::TypeChangeBitToBool(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Boolean()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::TaggedPtr(), zone()),
      ChangeRepresentation(arg.upper, Type::TaggedPtr(), zone()));
}


Bounds Typer::Visitor::TypeLoadField(Node* node) {
  return Bounds(FieldAccessOf(node->op()).type);
}


Bounds Typer::Visitor::TypeLoadElement(Node* node) {
  return Bounds(ElementAccessOf(node->op()).type);
}


Bounds Typer::Visitor::TypeStoreField(Node* node) {
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


Bounds Typer::Visitor::TypeUint64Mod(Node* node) {
  return Bounds(Type::Internal());
}


Bounds Typer::Visitor::TypeChangeFloat32ToFloat64(Node* node) {
  return Bounds(Type::Intersect(
      Type::Number(), Type::UntaggedFloat64(), zone()));
}


Bounds Typer::Visitor::TypeChangeFloat64ToInt32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Signed32(), Type::UntaggedInt32(), zone()));
}


Bounds Typer::Visitor::TypeChangeFloat64ToUint32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Unsigned32(), Type::UntaggedInt32(), zone()));
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
      Type::Signed32(), Type::UntaggedInt32(), zone()));
}


Bounds Typer::Visitor::TypeTruncateInt64ToInt32(Node* node) {
  return Bounds(Type::Intersect(
      Type::Signed32(), Type::UntaggedInt32(), zone()));
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


Bounds Typer::Visitor::TypeFloat64Floor(Node* node) {
  // TODO(sigurds): We could have a tighter bound here.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeFloat64Ceil(Node* node) {
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


Bounds Typer::Visitor::TypeLoadStackPointer(Node* node) {
  return Bounds(Type::Internal());
}


// Heap constants.


Type* Typer::Visitor::TypeConstant(Handle<Object> value) {
  if (value->IsJSFunction()) {
    if (JSFunction::cast(*value)->shared()->HasBuiltinFunctionId()) {
      switch (JSFunction::cast(*value)->shared()->builtin_function_id()) {
        case kMathRandom:
          return typer_->random_fun_;
        case kMathFloor:
          return typer_->weakint_fun1_;
        case kMathRound:
          return typer_->weakint_fun1_;
        case kMathCeil:
          return typer_->weakint_fun1_;
        case kMathAbs:
          // TODO(rossberg): can't express overloading
          return typer_->number_fun1_;
        case kMathLog:
          return typer_->number_fun1_;
        case kMathExp:
          return typer_->number_fun1_;
        case kMathSqrt:
          return typer_->number_fun1_;
        case kMathPow:
          return typer_->number_fun2_;
        case kMathMax:
          return typer_->number_fun2_;
        case kMathMin:
          return typer_->number_fun2_;
        case kMathCos:
          return typer_->number_fun1_;
        case kMathSin:
          return typer_->number_fun1_;
        case kMathTan:
          return typer_->number_fun1_;
        case kMathAcos:
          return typer_->number_fun1_;
        case kMathAsin:
          return typer_->number_fun1_;
        case kMathAtan:
          return typer_->number_fun1_;
        case kMathAtan2:
          return typer_->number_fun2_;
        case kMathImul:
          return typer_->imul_fun_;
        case kMathClz32:
          return typer_->clz32_fun_;
        case kMathFround:
          return typer_->number_fun1_;
        default:
          break;
      }
    } else if (JSFunction::cast(*value)->IsBuiltin() && !context().is_null()) {
      Handle<Context> native =
          handle(context().ToHandleChecked()->native_context(), isolate());
      if (*value == native->array_buffer_fun()) {
        return typer_->array_buffer_fun_;
      } else if (*value == native->int8_array_fun()) {
        return typer_->int8_array_fun_;
      } else if (*value == native->int16_array_fun()) {
        return typer_->int16_array_fun_;
      } else if (*value == native->int32_array_fun()) {
        return typer_->int32_array_fun_;
      } else if (*value == native->uint8_array_fun()) {
        return typer_->uint8_array_fun_;
      } else if (*value == native->uint16_array_fun()) {
        return typer_->uint16_array_fun_;
      } else if (*value == native->uint32_array_fun()) {
        return typer_->uint32_array_fun_;
      } else if (*value == native->float32_array_fun()) {
        return typer_->float32_array_fun_;
      } else if (*value == native->float64_array_fun()) {
        return typer_->float64_array_fun_;
      }
    }
  }
  return Type::Constant(value, zone());
}

}
}
}  // namespace v8::internal::compiler
