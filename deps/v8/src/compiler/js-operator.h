// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_OPERATOR_H_
#define V8_COMPILER_JS_OPERATOR_H_

#include "src/compiler/linkage.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/unique.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// Defines the location of a context slot relative to a specific scope. This is
// used as a parameter by JSLoadContext and JSStoreContext operators and allows
// accessing a context-allocated variable without keeping track of the scope.
class ContextAccess {
 public:
  ContextAccess(int depth, int index, bool immutable)
      : immutable_(immutable), depth_(depth), index_(index) {
    DCHECK(0 <= depth && depth <= kMaxUInt16);
    DCHECK(0 <= index && static_cast<uint32_t>(index) <= kMaxUInt32);
  }
  int depth() const { return depth_; }
  int index() const { return index_; }
  bool immutable() const { return immutable_; }

 private:
  // For space reasons, we keep this tightly packed, otherwise we could just use
  // a simple int/int/bool POD.
  const bool immutable_;
  const uint16_t depth_;
  const uint32_t index_;
};

// Defines the property being loaded from an object by a named load. This is
// used as a parameter by JSLoadNamed operators.
struct LoadNamedParameters {
  PrintableUnique<Name> name;
  ContextualMode contextual_mode;
};

// Defines the arity and the call flags for a JavaScript function call. This is
// used as a parameter by JSCall operators.
struct CallParameters {
  int arity;
  CallFunctionFlags flags;
};

// Interface for building JavaScript-level operators, e.g. directly from the
// AST. Most operators have no parameters, thus can be globally shared for all
// graphs.
class JSOperatorBuilder {
 public:
  explicit JSOperatorBuilder(Zone* zone) : zone_(zone) {}

#define SIMPLE(name, properties, inputs, outputs) \
  return new (zone_)                              \
      SimpleOperator(IrOpcode::k##name, properties, inputs, outputs, #name);

#define NOPROPS(name, inputs, outputs) \
  SIMPLE(name, Operator::kNoProperties, inputs, outputs)

#define OP1(name, ptype, pname, properties, inputs, outputs)                 \
  return new (zone_) Operator1<ptype>(IrOpcode::k##name, properties, inputs, \
                                      outputs, #name, pname)

#define BINOP(name) NOPROPS(name, 2, 1)
#define UNOP(name) NOPROPS(name, 1, 1)

#define PURE_BINOP(name) SIMPLE(name, Operator::kPure, 2, 1)

  Operator* Equal() { BINOP(JSEqual); }
  Operator* NotEqual() { BINOP(JSNotEqual); }
  Operator* StrictEqual() { PURE_BINOP(JSStrictEqual); }
  Operator* StrictNotEqual() { PURE_BINOP(JSStrictNotEqual); }
  Operator* LessThan() { BINOP(JSLessThan); }
  Operator* GreaterThan() { BINOP(JSGreaterThan); }
  Operator* LessThanOrEqual() { BINOP(JSLessThanOrEqual); }
  Operator* GreaterThanOrEqual() { BINOP(JSGreaterThanOrEqual); }
  Operator* BitwiseOr() { BINOP(JSBitwiseOr); }
  Operator* BitwiseXor() { BINOP(JSBitwiseXor); }
  Operator* BitwiseAnd() { BINOP(JSBitwiseAnd); }
  Operator* ShiftLeft() { BINOP(JSShiftLeft); }
  Operator* ShiftRight() { BINOP(JSShiftRight); }
  Operator* ShiftRightLogical() { BINOP(JSShiftRightLogical); }
  Operator* Add() { BINOP(JSAdd); }
  Operator* Subtract() { BINOP(JSSubtract); }
  Operator* Multiply() { BINOP(JSMultiply); }
  Operator* Divide() { BINOP(JSDivide); }
  Operator* Modulus() { BINOP(JSModulus); }

  Operator* UnaryNot() { UNOP(JSUnaryNot); }
  Operator* ToBoolean() { UNOP(JSToBoolean); }
  Operator* ToNumber() { UNOP(JSToNumber); }
  Operator* ToString() { UNOP(JSToString); }
  Operator* ToName() { UNOP(JSToName); }
  Operator* ToObject() { UNOP(JSToObject); }
  Operator* Yield() { UNOP(JSYield); }

  Operator* Create() { SIMPLE(JSCreate, Operator::kEliminatable, 0, 1); }

  Operator* Call(int arguments, CallFunctionFlags flags) {
    CallParameters parameters = {arguments, flags};
    OP1(JSCallFunction, CallParameters, parameters, Operator::kNoProperties,
        arguments, 1);
  }

  Operator* CallNew(int arguments) {
    return new (zone_)
        Operator1<int>(IrOpcode::kJSCallConstruct, Operator::kNoProperties,
                       arguments, 1, "JSCallConstruct", arguments);
  }

  Operator* LoadProperty() { BINOP(JSLoadProperty); }
  Operator* LoadNamed(PrintableUnique<Name> name,
                      ContextualMode contextual_mode = NOT_CONTEXTUAL) {
    LoadNamedParameters parameters = {name, contextual_mode};
    OP1(JSLoadNamed, LoadNamedParameters, parameters, Operator::kNoProperties,
        1, 1);
  }

  Operator* StoreProperty() { NOPROPS(JSStoreProperty, 3, 0); }
  Operator* StoreNamed(PrintableUnique<Name> name) {
    OP1(JSStoreNamed, PrintableUnique<Name>, name, Operator::kNoProperties, 2,
        0);
  }

  Operator* DeleteProperty(StrictMode strict_mode) {
    OP1(JSDeleteProperty, StrictMode, strict_mode, Operator::kNoProperties, 2,
        1);
  }

  Operator* HasProperty() { NOPROPS(JSHasProperty, 2, 1); }

  Operator* LoadContext(uint16_t depth, uint32_t index, bool immutable) {
    ContextAccess access(depth, index, immutable);
    OP1(JSLoadContext, ContextAccess, access,
        Operator::kEliminatable | Operator::kNoWrite, 1, 1);
  }
  Operator* StoreContext(uint16_t depth, uint32_t index) {
    ContextAccess access(depth, index, false);
    OP1(JSStoreContext, ContextAccess, access, Operator::kNoProperties, 2, 1);
  }

  Operator* TypeOf() { SIMPLE(JSTypeOf, Operator::kPure, 1, 1); }
  Operator* InstanceOf() { NOPROPS(JSInstanceOf, 2, 1); }
  Operator* Debugger() { NOPROPS(JSDebugger, 0, 0); }

  // TODO(titzer): nail down the static parts of each of these context flavors.
  Operator* CreateFunctionContext() { NOPROPS(JSCreateFunctionContext, 1, 1); }
  Operator* CreateCatchContext(PrintableUnique<String> name) {
    OP1(JSCreateCatchContext, PrintableUnique<String>, name,
        Operator::kNoProperties, 1, 1);
  }
  Operator* CreateWithContext() { NOPROPS(JSCreateWithContext, 2, 1); }
  Operator* CreateBlockContext() { NOPROPS(JSCreateBlockContext, 2, 1); }
  Operator* CreateModuleContext() { NOPROPS(JSCreateModuleContext, 2, 1); }
  Operator* CreateGlobalContext() { NOPROPS(JSCreateGlobalContext, 2, 1); }

  Operator* Runtime(Runtime::FunctionId function, int arguments) {
    const Runtime::Function* f = Runtime::FunctionForId(function);
    DCHECK(f->nargs == -1 || f->nargs == arguments);
    OP1(JSCallRuntime, Runtime::FunctionId, function, Operator::kNoProperties,
        arguments, f->result_size);
  }

#undef SIMPLE
#undef NOPROPS
#undef OP1
#undef BINOP
#undef UNOP

 private:
  Zone* zone_;
};

// Specialization for static parameters of type {ContextAccess}.
template <>
struct StaticParameterTraits<ContextAccess> {
  static OStream& PrintTo(OStream& os, ContextAccess val) {  // NOLINT
    return os << val.depth() << "," << val.index()
              << (val.immutable() ? ",imm" : "");
  }
  static int HashCode(ContextAccess val) {
    return (val.depth() << 16) | (val.index() & 0xffff);
  }
  static bool Equals(ContextAccess a, ContextAccess b) {
    return a.immutable() == b.immutable() && a.depth() == b.depth() &&
           a.index() == b.index();
  }
};

// Specialization for static parameters of type {Runtime::FunctionId}.
template <>
struct StaticParameterTraits<Runtime::FunctionId> {
  static OStream& PrintTo(OStream& os, Runtime::FunctionId val) {  // NOLINT
    const Runtime::Function* f = Runtime::FunctionForId(val);
    return os << (f->name ? f->name : "?Runtime?");
  }
  static int HashCode(Runtime::FunctionId val) { return static_cast<int>(val); }
  static bool Equals(Runtime::FunctionId a, Runtime::FunctionId b) {
    return a == b;
  }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_JS_OPERATOR_H_
