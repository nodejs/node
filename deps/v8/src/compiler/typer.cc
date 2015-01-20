// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/typer.h"

namespace v8 {
namespace internal {
namespace compiler {

#define NATIVE_TYPES(V) \
  V(Int8)               \
  V(Uint8)              \
  V(Int16)              \
  V(Uint16)             \
  V(Int32)              \
  V(Uint32)             \
  V(Float32)            \
  V(Float64)

enum LazyCachedType {
  kNumberFunc0,
  kNumberFunc1,
  kNumberFunc2,
  kImulFunc,
  kClz32Func,
  kArrayBufferFunc,
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  k##Type, k##Type##Array, k##Type##ArrayFunc,
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      kNumLazyCachedTypes
};


// Constructs and caches types lazily.
// TODO(turbofan): these types could be globally cached or cached per isolate.
class LazyTypeCache FINAL : public ZoneObject {
 public:
  explicit LazyTypeCache(Zone* zone) : zone_(zone) {
    memset(cache_, 0, sizeof(cache_));
  }

  inline Type* Get(LazyCachedType type) {
    int index = static_cast<int>(type);
    DCHECK(index < kNumLazyCachedTypes);
    if (cache_[index] == NULL) cache_[index] = Create(type);
    return cache_[index];
  }

 private:
  Type* Create(LazyCachedType type) {
    switch (type) {
      case kInt8:
        return CreateNative(CreateRange<int8_t>(), Type::UntaggedSigned8());
      case kUint8:
        return CreateNative(CreateRange<uint8_t>(), Type::UntaggedUnsigned8());
      case kInt16:
        return CreateNative(CreateRange<int16_t>(), Type::UntaggedSigned16());
      case kUint16:
        return CreateNative(CreateRange<uint16_t>(),
                            Type::UntaggedUnsigned16());
      case kInt32:
        return CreateNative(Type::Signed32(), Type::UntaggedSigned32());
      case kUint32:
        return CreateNative(Type::Unsigned32(), Type::UntaggedUnsigned32());
      case kFloat32:
        return CreateNative(Type::Number(), Type::UntaggedFloat32());
      case kFloat64:
        return CreateNative(Type::Number(), Type::UntaggedFloat64());
      case kUint8Clamped:
        return Get(kUint8);
      case kNumberFunc0:
        return Type::Function(Type::Number(), zone());
      case kNumberFunc1:
        return Type::Function(Type::Number(), Type::Number(), zone());
      case kNumberFunc2:
        return Type::Function(Type::Number(), Type::Number(), Type::Number(),
                              zone());
      case kImulFunc:
        return Type::Function(Type::Signed32(), Type::Integral32(),
                              Type::Integral32(), zone());
      case kClz32Func:
        return Type::Function(CreateRange(0, 32), Type::Number(), zone());
      case kArrayBufferFunc:
        return Type::Function(Type::Object(zone()), Type::Unsigned32(), zone());
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case k##Type##Array:                                  \
    return CreateArray(Get(k##Type));                   \
  case k##Type##ArrayFunc:                              \
    return CreateArrayFunction(Get(k##Type##Array));
        TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      case kNumLazyCachedTypes:
        break;
    }
    UNREACHABLE();
    return NULL;
  }

  Type* CreateArray(Type* element) const {
    return Type::Array(element, zone());
  }

  Type* CreateArrayFunction(Type* array) const {
    Type* arg1 = Type::Union(Type::Unsigned32(), Type::Object(), zone());
    Type* arg2 = Type::Union(Type::Unsigned32(), Type::Undefined(), zone());
    Type* arg3 = arg2;
    return Type::Function(array, arg1, arg2, arg3, zone());
  }

  Type* CreateNative(Type* semantic, Type* representation) const {
    return Type::Intersect(semantic, representation, zone());
  }

  template <typename T>
  Type* CreateRange() const {
    return CreateRange(std::numeric_limits<T>::min(),
                       std::numeric_limits<T>::max());
  }

  Type* CreateRange(double min, double max) const {
    return Type::Range(factory()->NewNumber(min), factory()->NewNumber(max),
                       zone());
  }

  Factory* factory() const { return isolate()->factory(); }
  Isolate* isolate() const { return zone()->isolate(); }
  Zone* zone() const { return zone_; }

  Type* cache_[kNumLazyCachedTypes];
  Zone* zone_;
};


class Typer::Decorator FINAL : public GraphDecorator {
 public:
  explicit Decorator(Typer* typer) : typer_(typer) {}
  void Decorate(Node* node) FINAL;

 private:
  Typer* typer_;
};


Typer::Typer(Graph* graph, MaybeHandle<Context> context)
    : graph_(graph),
      context_(context),
      decorator_(NULL),
      cache_(new (graph->zone()) LazyTypeCache(graph->zone())),
      weaken_min_limits_(graph->zone()),
      weaken_max_limits_(graph->zone()) {
  Zone* zone = this->zone();
  Factory* f = zone->isolate()->factory();

  Handle<Object> zero = f->NewNumber(0);
  Handle<Object> one = f->NewNumber(1);
  Handle<Object> infinity = f->NewNumber(+V8_INFINITY);
  Handle<Object> minusinfinity = f->NewNumber(-V8_INFINITY);

  Type* number = Type::Number();
  Type* signed32 = Type::Signed32();
  Type* unsigned32 = Type::Unsigned32();
  Type* nan_or_minuszero = Type::Union(Type::NaN(), Type::MinusZero(), zone);
  Type* truncating_to_zero =
      Type::Union(Type::Union(Type::Constant(infinity, zone),
                              Type::Constant(minusinfinity, zone), zone),
                  nan_or_minuszero, zone);

  boolean_or_number = Type::Union(Type::Boolean(), Type::Number(), zone);
  undefined_or_null = Type::Union(Type::Undefined(), Type::Null(), zone);
  undefined_or_number = Type::Union(Type::Undefined(), Type::Number(), zone);
  singleton_false = Type::Constant(f->false_value(), zone);
  singleton_true = Type::Constant(f->true_value(), zone);
  singleton_zero = Type::Range(zero, zero, zone);
  singleton_one = Type::Range(one, one, zone);
  zero_or_one = Type::Union(singleton_zero, singleton_one, zone);
  zeroish = Type::Union(singleton_zero, nan_or_minuszero, zone);
  signed32ish = Type::Union(signed32, truncating_to_zero, zone);
  unsigned32ish = Type::Union(unsigned32, truncating_to_zero, zone);
  falsish = Type::Union(Type::Undetectable(),
                        Type::Union(Type::Union(singleton_false, zeroish, zone),
                                    undefined_or_null, zone),
                        zone);
  truish = Type::Union(
      singleton_true,
      Type::Union(Type::DetectableReceiver(), Type::Symbol(), zone), zone);
  integer = Type::Range(minusinfinity, infinity, zone);
  weakint = Type::Union(integer, nan_or_minuszero, zone);

  number_fun0_ = Type::Function(number, zone);
  number_fun1_ = Type::Function(number, number, zone);
  number_fun2_ = Type::Function(number, number, number, zone);

  weakint_fun1_ = Type::Function(weakint, number, zone);
  random_fun_ = Type::Function(Type::OrderedNumber(), zone);

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


class Typer::Visitor : public Reducer {
 public:
  explicit Visitor(Typer* typer) : typer_(typer) {}

  Reduction Reduce(Node* node) OVERRIDE {
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

 private:
  Typer* typer_;
  MaybeHandle<Context> context_;

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

  Reduction UpdateBounds(Node* node, Bounds current) {
    if (NodeProperties::IsTyped(node)) {
      // Widen the bounds of a previously typed node.
      Bounds previous = NodeProperties::GetBounds(node);
      // Speed up termination in the presence of range types:
      current.upper = Weaken(current.upper, previous.upper);
      current.lower = Weaken(current.lower, previous.lower);

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


void Typer::Run() {
  {
    // TODO(titzer): this is a hack. Reset types for interior nodes first.
    NodeDeque deque(zone());
    NodeMarker<bool> marked(graph(), 2);
    deque.push_front(graph()->end());
    marked.Set(graph()->end(), true);
    while (!deque.empty()) {
      Node* node = deque.front();
      deque.pop_front();
      // TODO(titzer): there shouldn't be a need to retype constants.
      if (node->op()->ValueOutputCount() > 0)
        NodeProperties::RemoveBounds(node);
      for (Node* input : node->inputs()) {
        if (!marked.Get(input)) {
          marked.Set(input, true);
          deque.push_back(input);
        }
      }
    }
  }

  Visitor visitor(this);
  GraphReducer graph_reducer(graph(), zone());
  graph_reducer.AddReducer(&visitor);
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
  if (type->Is(t->truish)) return t->singleton_true;
  if (type->Is(Type::PlainNumber()) && (type->Max() < 0 || 0 < type->Min())) {
    return t->singleton_true;  // Ruled out nan, -0 and +0.
  }
  return Type::Boolean();
}


Type* Typer::Visitor::ToNumber(Type* type, Typer* t) {
  if (type->Is(Type::Number())) return type;
  if (type->Is(Type::Null())) return t->singleton_zero;
  if (type->Is(Type::Undefined())) return Type::NaN();
  if (type->Is(t->undefined_or_null)) {
    return Type::Union(Type::NaN(), t->singleton_zero, t->zone());
  }
  if (type->Is(t->undefined_or_number)) {
    return Type::Union(Type::Intersect(type, Type::Number(), t->zone()),
                       Type::NaN(), t->zone());
  }
  if (type->Is(t->singleton_false)) return t->singleton_zero;
  if (type->Is(t->singleton_true)) return t->singleton_one;
  if (type->Is(Type::Boolean())) return t->zero_or_one;
  if (type->Is(t->boolean_or_number)) {
    return Type::Union(Type::Intersect(type, Type::Number(), t->zone()),
                       t->zero_or_one, t->zone());
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
  if (type->Is(t->zeroish)) return t->singleton_zero;
  if (type->Is(t->signed32ish)) {
    return Type::Intersect(Type::Union(type, t->singleton_zero, t->zone()),
                           Type::Signed32(), t->zone());
  }
  return Type::Signed32();
}


Type* Typer::Visitor::NumberToUint32(Type* type, Typer* t) {
  // TODO(neis): DCHECK(type->Is(Type::Number()));
  if (type->Is(Type::Unsigned32())) return type;
  if (type->Is(t->zeroish)) return t->singleton_zero;
  if (type->Is(t->unsigned32ish)) {
    return Type::Intersect(Type::Union(type, t->singleton_zero, t->zone()),
                           Type::Unsigned32(), t->zone());
  }
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
  return Type::Range(f->NewNumber(min), f->NewNumber(max), t->zone());
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
  return Type::Range(f->NewNumber(min), f->NewNumber(max), t->zone());
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
    return Type::NonNegativeSigned32();
  }
  if ((lmax < 0 && rmin >= 0) || (lmin >= 0 && rmax < 0)) {
    // Xor-ing a negative and a non-negative value results in a negative value.
    // TODO(jarin) Use a range here.
    return Type::NegativeSigned32();
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
  }
  if (lhs->Max() < 0) {
    // Right-shifting a negative value cannot make it non-negative, nor smaller.
    min = std::max(min, lhs->Min());
    max = std::min(max, -1.0);
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
    Factory* f = t->isolate()->factory();
    return Type::Range(f->NewNumber(min), f->NewNumber(max), t->zone());
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


Bounds Typer::Visitor::TypeJSTypeOf(Node* node) {
  return Bounds(Type::None(zone()), Type::InternalizedString(zone()));
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
  Type::RangeType* previous = previous_type->GetRange();
  Type::RangeType* current = current_type->GetRange();
  if (previous != NULL && current != NULL) {
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

    return Type::Union(current_type,
                       Type::Range(new_min, new_max, typer_->zone()),
                       typer_->zone());
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
  Type* context_type = outer.upper;
  if (context_type->Is(Type::None())) {
    // Upper bound of context is not yet known.
    return Bounds(Type::None(), Type::Any());
  }

  DCHECK(context_type->Maybe(Type::Internal()));
  // TODO(rossberg): More precisely, instead of the above assertion, we should
  // back-propagate the constraint that it has to be a subtype of Internal.

  ContextAccess access = OpParameter<ContextAccess>(node);
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


Bounds Typer::Visitor::TypeJSCreateScriptContext(Node* node) {
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


Bounds Typer::Visitor::TypeAnyToBoolean(Node* node) {
  return TypeUnaryOp(node, ToBoolean);
}


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
      ChangeRepresentation(arg.lower, Type::UntaggedBit(), zone()),
      ChangeRepresentation(arg.upper, Type::UntaggedBit(), zone()));
}


Bounds Typer::Visitor::TypeChangeBitToBool(Node* node) {
  Bounds arg = Operand(node, 0);
  // TODO(neis): DCHECK(arg.upper->Is(Type::Boolean()));
  return Bounds(
      ChangeRepresentation(arg.lower, Type::TaggedPointer(), zone()),
      ChangeRepresentation(arg.upper, Type::TaggedPointer(), zone()));
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
    return Bounds(typer_->cache_->Get(k##Type));
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
          return typer_->random_fun_;
        case kMathFloor:
          return typer_->weakint_fun1_;
        case kMathRound:
          return typer_->weakint_fun1_;
        case kMathCeil:
          return typer_->weakint_fun1_;
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
          return typer_->cache_->Get(kNumberFunc1);
        // Binary math functions.
        case kMathAtan2:
        case kMathPow:
        case kMathMax:
        case kMathMin:
          return typer_->cache_->Get(kNumberFunc2);
        case kMathImul:
          return typer_->cache_->Get(kImulFunc);
        case kMathClz32:
          return typer_->cache_->Get(kClz32Func);
        default:
          break;
      }
    } else if (JSFunction::cast(*value)->IsBuiltin() && !context().is_null()) {
      Handle<Context> native =
          handle(context().ToHandleChecked()->native_context(), isolate());
      if (*value == native->array_buffer_fun()) {
        return typer_->cache_->Get(kArrayBufferFunc);
      } else if (*value == native->int8_array_fun()) {
        return typer_->cache_->Get(kInt8ArrayFunc);
      } else if (*value == native->int16_array_fun()) {
        return typer_->cache_->Get(kInt16ArrayFunc);
      } else if (*value == native->int32_array_fun()) {
        return typer_->cache_->Get(kInt32ArrayFunc);
      } else if (*value == native->uint8_array_fun()) {
        return typer_->cache_->Get(kUint8ArrayFunc);
      } else if (*value == native->uint16_array_fun()) {
        return typer_->cache_->Get(kUint16ArrayFunc);
      } else if (*value == native->uint32_array_fun()) {
        return typer_->cache_->Get(kUint32ArrayFunc);
      } else if (*value == native->float32_array_fun()) {
        return typer_->cache_->Get(kFloat32ArrayFunc);
      } else if (*value == native->float64_array_fun()) {
        return typer_->cache_->Get(kFloat64ArrayFunc);
      }
    }
  } else if (value->IsJSTypedArray()) {
    switch (JSTypedArray::cast(*value)->type()) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    return typer_->cache_->Get(k##Type##Array);
      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    }
  }
  return Type::Constant(value, zone());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
