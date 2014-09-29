// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

Typer::Typer(Zone* zone) : zone_(zone) {
  Type* number = Type::Number(zone);
  Type* signed32 = Type::Signed32(zone);
  Type* unsigned32 = Type::Unsigned32(zone);
  Type* integral32 = Type::Integral32(zone);
  Type* object = Type::Object(zone);
  Type* undefined = Type::Undefined(zone);
  number_fun0_ = Type::Function(number, zone);
  number_fun1_ = Type::Function(number, number, zone);
  number_fun2_ = Type::Function(number, number, number, zone);
  imul_fun_ = Type::Function(signed32, integral32, integral32, zone);

#define NATIVE_TYPE(sem, rep) \
  Type::Intersect(Type::sem(zone), Type::rep(zone), zone)
  // TODO(rossberg): Use range types for more precision, once we have them.
  Type* int8 = NATIVE_TYPE(SignedSmall, UntaggedInt8);
  Type* int16 = NATIVE_TYPE(SignedSmall, UntaggedInt16);
  Type* int32 = NATIVE_TYPE(Signed32, UntaggedInt32);
  Type* uint8 = NATIVE_TYPE(UnsignedSmall, UntaggedInt8);
  Type* uint16 = NATIVE_TYPE(UnsignedSmall, UntaggedInt16);
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
}


class Typer::Visitor : public NullNodeVisitor {
 public:
  Visitor(Typer* typer, MaybeHandle<Context> context)
      : typer_(typer), context_(context) {}

  Bounds TypeNode(Node* node) {
    switch (node->opcode()) {
#define DECLARE_CASE(x) case IrOpcode::k##x: return Type##x(node);
      VALUE_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) case IrOpcode::k##x:
      CONTROL_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
      break;
    }
    return Bounds(Type::None(zone()));
  }

  Type* TypeConstant(Handle<Object> value);

 protected:
#define DECLARE_METHOD(x) inline Bounds Type##x(Node* node);
  VALUE_OP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  Bounds OperandType(Node* node, int i) {
    return NodeProperties::GetBounds(NodeProperties::GetValueInput(node, i));
  }

  Type* ContextType(Node* node) {
    Bounds result =
        NodeProperties::GetBounds(NodeProperties::GetContextInput(node));
    DCHECK(result.upper->Is(Type::Internal()));
    DCHECK(result.lower->Equals(result.upper));
    return result.upper;
  }

  Zone* zone() { return typer_->zone(); }
  Isolate* isolate() { return typer_->isolate(); }
  MaybeHandle<Context> context() { return context_; }

 private:
  Typer* typer_;
  MaybeHandle<Context> context_;
};


class Typer::RunVisitor : public Typer::Visitor {
 public:
  RunVisitor(Typer* typer, MaybeHandle<Context> context)
      : Visitor(typer, context),
        phis(NodeSet::key_compare(), NodeSet::allocator_type(typer->zone())) {}

  GenericGraphVisit::Control Pre(Node* node) {
    return NodeProperties::IsControl(node)
        && node->opcode() != IrOpcode::kEnd
        && node->opcode() != IrOpcode::kMerge
        && node->opcode() != IrOpcode::kReturn
        ? GenericGraphVisit::SKIP : GenericGraphVisit::CONTINUE;
  }

  GenericGraphVisit::Control Post(Node* node) {
    Bounds bounds = TypeNode(node);
    if (node->opcode() == IrOpcode::kPhi) {
      // Remember phis for least fixpoint iteration.
      phis.insert(node);
    } else {
      NodeProperties::SetBounds(node, bounds);
    }
    return GenericGraphVisit::CONTINUE;
  }

  NodeSet phis;
};


class Typer::NarrowVisitor : public Typer::Visitor {
 public:
  NarrowVisitor(Typer* typer, MaybeHandle<Context> context)
      : Visitor(typer, context) {}

  GenericGraphVisit::Control Pre(Node* node) {
    Bounds previous = NodeProperties::GetBounds(node);
    Bounds bounds = TypeNode(node);
    NodeProperties::SetBounds(node, Bounds::Both(bounds, previous, zone()));
    DCHECK(bounds.Narrows(previous));
    // Stop when nothing changed (but allow reentry in case it does later).
    return previous.Narrows(bounds)
        ? GenericGraphVisit::DEFER : GenericGraphVisit::REENTER;
  }

  GenericGraphVisit::Control Post(Node* node) {
    return GenericGraphVisit::REENTER;
  }
};


class Typer::WidenVisitor : public Typer::Visitor {
 public:
  WidenVisitor(Typer* typer, MaybeHandle<Context> context)
      : Visitor(typer, context) {}

  GenericGraphVisit::Control Pre(Node* node) {
    Bounds previous = NodeProperties::GetBounds(node);
    Bounds bounds = TypeNode(node);
    DCHECK(previous.lower->Is(bounds.lower));
    DCHECK(previous.upper->Is(bounds.upper));
    NodeProperties::SetBounds(node, bounds);  // TODO(rossberg): Either?
    // Stop when nothing changed (but allow reentry in case it does later).
    return bounds.Narrows(previous)
        ? GenericGraphVisit::DEFER : GenericGraphVisit::REENTER;
  }

  GenericGraphVisit::Control Post(Node* node) {
    return GenericGraphVisit::REENTER;
  }
};


void Typer::Run(Graph* graph, MaybeHandle<Context> context) {
  RunVisitor typing(this, context);
  graph->VisitNodeInputsFromEnd(&typing);
  // Find least fixpoint.
  for (NodeSetIter i = typing.phis.begin(); i != typing.phis.end(); ++i) {
    Widen(graph, *i, context);
  }
}


void Typer::Narrow(Graph* graph, Node* start, MaybeHandle<Context> context) {
  NarrowVisitor typing(this, context);
  graph->VisitNodeUsesFrom(start, &typing);
}


void Typer::Widen(Graph* graph, Node* start, MaybeHandle<Context> context) {
  WidenVisitor typing(this, context);
  graph->VisitNodeUsesFrom(start, &typing);
}


void Typer::Init(Node* node) {
  Visitor typing(this, MaybeHandle<Context>());
  Bounds bounds = typing.TypeNode(node);
  NodeProperties::SetBounds(node, bounds);
}


// Common operators.
Bounds Typer::Visitor::TypeParameter(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeInt32Constant(Node* node) {
  // TODO(titzer): only call Type::Of() if the type is not already known.
  return Bounds(Type::Of(ValueOf<int32_t>(node->op()), zone()));
}


Bounds Typer::Visitor::TypeInt64Constant(Node* node) {
  // TODO(titzer): only call Type::Of() if the type is not already known.
  return Bounds(
      Type::Of(static_cast<double>(ValueOf<int64_t>(node->op())), zone()));
}


Bounds Typer::Visitor::TypeFloat64Constant(Node* node) {
  // TODO(titzer): only call Type::Of() if the type is not already known.
  return Bounds(Type::Of(ValueOf<double>(node->op()), zone()));
}


Bounds Typer::Visitor::TypeNumberConstant(Node* node) {
  // TODO(titzer): only call Type::Of() if the type is not already known.
  return Bounds(Type::Of(ValueOf<double>(node->op()), zone()));
}


Bounds Typer::Visitor::TypeHeapConstant(Node* node) {
  return Bounds(TypeConstant(ValueOf<Handle<Object> >(node->op())));
}


Bounds Typer::Visitor::TypeExternalConstant(Node* node) {
  return Bounds(Type::Internal(zone()));
}


Bounds Typer::Visitor::TypePhi(Node* node) {
  int arity = OperatorProperties::GetValueInputCount(node->op());
  Bounds bounds = OperandType(node, 0);
  for (int i = 1; i < arity; ++i) {
    bounds = Bounds::Either(bounds, OperandType(node, i), zone());
  }
  return bounds;
}


Bounds Typer::Visitor::TypeEffectPhi(Node* node) {
  return Bounds(Type::None(zone()));
}


Bounds Typer::Visitor::TypeFrameState(Node* node) {
  return Bounds(Type::None(zone()));
}


Bounds Typer::Visitor::TypeStateValues(Node* node) {
  return Bounds(Type::None(zone()));
}


Bounds Typer::Visitor::TypeCall(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeProjection(Node* node) {
  // TODO(titzer): use the output type of the input to determine the bounds.
  return Bounds::Unbounded(zone());
}


// JS comparison operators.

#define DEFINE_METHOD(x)                       \
  Bounds Typer::Visitor::Type##x(Node* node) { \
    return Bounds(Type::Boolean(zone()));      \
  }
JS_COMPARE_BINOP_LIST(DEFINE_METHOD)
#undef DEFINE_METHOD


// JS bitwise operators.

Bounds Typer::Visitor::TypeJSBitwiseOr(Node* node) {
  Bounds left = OperandType(node, 0);
  Bounds right = OperandType(node, 1);
  Type* upper = Type::Union(left.upper, right.upper, zone());
  if (!upper->Is(Type::Signed32())) upper = Type::Signed32(zone());
  Type* lower = Type::Intersect(Type::SignedSmall(zone()), upper, zone());
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeJSBitwiseAnd(Node* node) {
  Bounds left = OperandType(node, 0);
  Bounds right = OperandType(node, 1);
  Type* upper = Type::Union(left.upper, right.upper, zone());
  if (!upper->Is(Type::Signed32())) upper = Type::Signed32(zone());
  Type* lower = Type::Intersect(Type::SignedSmall(zone()), upper, zone());
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeJSBitwiseXor(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Signed32(zone()));
}


Bounds Typer::Visitor::TypeJSShiftLeft(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Signed32(zone()));
}


Bounds Typer::Visitor::TypeJSShiftRight(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Signed32(zone()));
}


Bounds Typer::Visitor::TypeJSShiftRightLogical(Node* node) {
  return Bounds(Type::UnsignedSmall(zone()), Type::Unsigned32(zone()));
}


// JS arithmetic operators.

Bounds Typer::Visitor::TypeJSAdd(Node* node) {
  Bounds left = OperandType(node, 0);
  Bounds right = OperandType(node, 1);
  Type* lower =
      left.lower->Is(Type::None()) || right.lower->Is(Type::None()) ?
          Type::None(zone()) :
      left.lower->Is(Type::Number()) && right.lower->Is(Type::Number()) ?
          Type::SignedSmall(zone()) :
      left.lower->Is(Type::String()) || right.lower->Is(Type::String()) ?
          Type::String(zone()) : Type::None(zone());
  Type* upper =
      left.upper->Is(Type::None()) && right.upper->Is(Type::None()) ?
          Type::None(zone()) :
      left.upper->Is(Type::Number()) && right.upper->Is(Type::Number()) ?
          Type::Number(zone()) :
      left.upper->Is(Type::String()) || right.upper->Is(Type::String()) ?
          Type::String(zone()) : Type::NumberOrString(zone());
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeJSSubtract(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeJSMultiply(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeJSDivide(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeJSModulus(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Number(zone()));
}


// JS unary operators.

Bounds Typer::Visitor::TypeJSUnaryNot(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSTypeOf(Node* node) {
  return Bounds(Type::InternalizedString(zone()));
}


// JS conversion operators.

Bounds Typer::Visitor::TypeJSToBoolean(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSToNumber(Node* node) {
  return Bounds(Type::SignedSmall(zone()), Type::Number(zone()));
}


Bounds Typer::Visitor::TypeJSToString(Node* node) {
  return Bounds(Type::None(zone()), Type::String(zone()));
}


Bounds Typer::Visitor::TypeJSToName(Node* node) {
  return Bounds(Type::None(zone()), Type::Name(zone()));
}


Bounds Typer::Visitor::TypeJSToObject(Node* node) {
  return Bounds(Type::None(zone()), Type::Object(zone()));
}


// JS object operators.

Bounds Typer::Visitor::TypeJSCreate(Node* node) {
  return Bounds(Type::None(zone()), Type::Object(zone()));
}


Bounds Typer::Visitor::TypeJSLoadProperty(Node* node) {
  Bounds object = OperandType(node, 0);
  Bounds name = OperandType(node, 1);
  Bounds result = Bounds::Unbounded(zone());
  // TODO(rossberg): Use range types and sized array types to filter undefined.
  if (object.lower->IsArray() && name.lower->Is(Type::Integral32())) {
    result.lower = Type::Union(
        object.lower->AsArray()->Element(), Type::Undefined(zone()), zone());
  }
  if (object.upper->IsArray() && name.upper->Is(Type::Integral32())) {
    result.upper = Type::Union(
        object.upper->AsArray()->Element(),  Type::Undefined(zone()), zone());
  }
  return result;
}


Bounds Typer::Visitor::TypeJSLoadNamed(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSStoreProperty(Node* node) {
  return Bounds(Type::None(zone()));
}


Bounds Typer::Visitor::TypeJSStoreNamed(Node* node) {
  return Bounds(Type::None(zone()));
}


Bounds Typer::Visitor::TypeJSDeleteProperty(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSHasProperty(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeJSInstanceOf(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


// JS context operators.

Bounds Typer::Visitor::TypeJSLoadContext(Node* node) {
  Bounds outer = OperandType(node, 0);
  DCHECK(outer.upper->Is(Type::Internal()));
  DCHECK(outer.lower->Equals(outer.upper));
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
  for (int i = access.depth(); i > 0; --i) {
    if (context_type->IsContext()) {
      context_type = context_type->AsContext()->Outer();
      if (context_type->IsConstant()) {
        context = Handle<Context>::cast(context_type->AsConstant()->Value());
      }
    } else {
      context = handle(context.ToHandleChecked()->previous(), isolate());
    }
  }
  if (context.is_null()) {
    return Bounds::Unbounded(zone());
  } else {
    Handle<Object> value =
        handle(context.ToHandleChecked()->get(access.index()), isolate());
    Type* lower = TypeConstant(value);
    return Bounds(lower, Type::Any(zone()));
  }
}


Bounds Typer::Visitor::TypeJSStoreContext(Node* node) {
  return Bounds(Type::None(zone()));
}


Bounds Typer::Visitor::TypeJSCreateFunctionContext(Node* node) {
  Type* outer = ContextType(node);
  return Bounds(Type::Context(outer, zone()));
}


Bounds Typer::Visitor::TypeJSCreateCatchContext(Node* node) {
  Type* outer = ContextType(node);
  return Bounds(Type::Context(outer, zone()));
}


Bounds Typer::Visitor::TypeJSCreateWithContext(Node* node) {
  Type* outer = ContextType(node);
  return Bounds(Type::Context(outer, zone()));
}


Bounds Typer::Visitor::TypeJSCreateBlockContext(Node* node) {
  Type* outer = ContextType(node);
  return Bounds(Type::Context(outer, zone()));
}


Bounds Typer::Visitor::TypeJSCreateModuleContext(Node* node) {
  // TODO(rossberg): this is probably incorrect
  Type* outer = ContextType(node);
  return Bounds(Type::Context(outer, zone()));
}


Bounds Typer::Visitor::TypeJSCreateGlobalContext(Node* node) {
  Type* outer = ContextType(node);
  return Bounds(Type::Context(outer, zone()));
}


// JS other operators.

Bounds Typer::Visitor::TypeJSYield(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSCallConstruct(Node* node) {
  return Bounds(Type::None(zone()), Type::Receiver(zone()));
}


Bounds Typer::Visitor::TypeJSCallFunction(Node* node) {
  Bounds fun = OperandType(node, 0);
  Type* lower = fun.lower->IsFunction()
      ? fun.lower->AsFunction()->Result() : Type::None(zone());
  Type* upper = fun.upper->IsFunction()
      ? fun.upper->AsFunction()->Result() : Type::Any(zone());
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeJSCallRuntime(Node* node) {
  return Bounds::Unbounded(zone());
}


Bounds Typer::Visitor::TypeJSDebugger(Node* node) {
  return Bounds::Unbounded(zone());
}


// Simplified operators.

Bounds Typer::Visitor::TypeBooleanNot(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberEqual(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberLessThan(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberLessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeNumberAdd(Node* node) {
  return Bounds(Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberSubtract(Node* node) {
  return Bounds(Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberMultiply(Node* node) {
  return Bounds(Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberDivide(Node* node) {
  return Bounds(Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberModulus(Node* node) {
  return Bounds(Type::Number(zone()));
}


Bounds Typer::Visitor::TypeNumberToInt32(Node* node) {
  Bounds arg = OperandType(node, 0);
  Type* s32 = Type::Signed32(zone());
  Type* lower = arg.lower->Is(s32) ? arg.lower : s32;
  Type* upper = arg.upper->Is(s32) ? arg.upper : s32;
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeNumberToUint32(Node* node) {
  Bounds arg = OperandType(node, 0);
  Type* u32 = Type::Unsigned32(zone());
  Type* lower = arg.lower->Is(u32) ? arg.lower : u32;
  Type* upper = arg.upper->Is(u32) ? arg.upper : u32;
  return Bounds(lower, upper);
}


Bounds Typer::Visitor::TypeReferenceEqual(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringEqual(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringLessThan(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringLessThanOrEqual(Node* node) {
  return Bounds(Type::Boolean(zone()));
}


Bounds Typer::Visitor::TypeStringAdd(Node* node) {
  return Bounds(Type::String(zone()));
}


Bounds Typer::Visitor::TypeChangeTaggedToInt32(Node* node) {
  // TODO(titzer): type is type of input, representation is Word32.
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeChangeTaggedToUint32(Node* node) {
  return Bounds(Type::Integral32());  // TODO(titzer): add appropriate rep
}


Bounds Typer::Visitor::TypeChangeTaggedToFloat64(Node* node) {
  // TODO(titzer): type is type of input, representation is Float64.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeChangeInt32ToTagged(Node* node) {
  // TODO(titzer): type is type of input, representation is Tagged.
  return Bounds(Type::Integral32());
}


Bounds Typer::Visitor::TypeChangeUint32ToTagged(Node* node) {
  // TODO(titzer): type is type of input, representation is Tagged.
  return Bounds(Type::Unsigned32());
}


Bounds Typer::Visitor::TypeChangeFloat64ToTagged(Node* node) {
  // TODO(titzer): type is type of input, representation is Tagged.
  return Bounds(Type::Number());
}


Bounds Typer::Visitor::TypeChangeBoolToBit(Node* node) {
  // TODO(titzer): type is type of input, representation is Bit.
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeChangeBitToBool(Node* node) {
  // TODO(titzer): type is type of input, representation is Tagged.
  return Bounds(Type::Boolean());
}


Bounds Typer::Visitor::TypeLoadField(Node* node) {
  return Bounds(FieldAccessOf(node->op()).type);
}


Bounds Typer::Visitor::TypeLoadElement(Node* node) {
  return Bounds(ElementAccessOf(node->op()).type);
}


Bounds Typer::Visitor::TypeStoreField(Node* node) {
  return Bounds(Type::None());
}


Bounds Typer::Visitor::TypeStoreElement(Node* node) {
  return Bounds(Type::None());
}


// Machine operators.

// TODO(rossberg): implement
#define DEFINE_METHOD(x) \
    Bounds Typer::Visitor::Type##x(Node* node) { return Bounds(Type::None()); }
MACHINE_OP_LIST(DEFINE_METHOD)
#undef DEFINE_METHOD


// Heap constants.

Type* Typer::Visitor::TypeConstant(Handle<Object> value) {
  if (value->IsJSFunction() && JSFunction::cast(*value)->IsBuiltin() &&
      !context().is_null()) {
    Handle<Context> native =
        handle(context().ToHandleChecked()->native_context(), isolate());
    if (*value == native->math_abs_fun()) {
      return typer_->number_fun1_;  // TODO(rossberg): can't express overloading
    } else if (*value == native->math_acos_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_asin_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_atan_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_atan2_fun()) {
      return typer_->number_fun2_;
    } else if (*value == native->math_ceil_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_cos_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_exp_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_floor_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_imul_fun()) {
      return typer_->imul_fun_;
    } else if (*value == native->math_log_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_pow_fun()) {
      return typer_->number_fun2_;
    } else if (*value == native->math_random_fun()) {
      return typer_->number_fun0_;
    } else if (*value == native->math_round_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_sin_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_sqrt_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->math_tan_fun()) {
      return typer_->number_fun1_;
    } else if (*value == native->array_buffer_fun()) {
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
  return Type::Constant(value, zone());
}


namespace {

class TyperDecorator : public GraphDecorator {
 public:
  explicit TyperDecorator(Typer* typer) : typer_(typer) {}
  virtual void Decorate(Node* node) { typer_->Init(node); }

 private:
  Typer* typer_;
};

}


void Typer::DecorateGraph(Graph* graph) {
  graph->AddDecorator(new (zone()) TyperDecorator(this));
}

}
}
}  // namespace v8::internal::compiler
