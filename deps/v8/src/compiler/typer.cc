// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typer.h"

#include <iomanip>

#include "src/base/flags.h"
#include "src/bootstrapper.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/loop-variable-optimizer.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/operation-typer.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/objects-inl.h"
#include "src/objects/builtin-function-id.h"

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

Typer::Typer(JSHeapBroker* broker, Flags flags, Graph* graph)
    : flags_(flags),
      graph_(graph),
      decorator_(nullptr),
      cache_(TypeCache::Get()),
      broker_(broker),
      operation_typer_(broker, zone()) {
  singleton_false_ = operation_typer_.singleton_false();
  singleton_true_ = operation_typer_.singleton_true();

  decorator_ = new (zone()) Decorator(this);
  graph_->AddDecorator(decorator_);
}


Typer::~Typer() {
  graph_->RemoveDecorator(decorator_);
}


class Typer::Visitor : public Reducer {
 public:
  explicit Visitor(Typer* typer, LoopVariableOptimizer* induction_vars)
      : typer_(typer),
        induction_vars_(induction_vars),
        weakened_nodes_(typer->zone()),
        remembered_types_(typer->zone()) {}

  const char* reducer_name() const override { return "Typer"; }

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
      SIMPLIFIED_COMPARE_BINOP_LIST(DECLARE_CASE)
      SIMPLIFIED_OTHER_OP_LIST(DECLARE_CASE)
      JS_SIMPLE_UNOP_LIST(DECLARE_CASE)
      JS_OBJECT_OP_LIST(DECLARE_CASE)
      JS_CONTEXT_OP_LIST(DECLARE_CASE)
      JS_OTHER_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return UpdateType(node, TypeBinaryOp(node, x));
      SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_CASE)
      SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return UpdateType(node, TypeUnaryOp(node, x));
      SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_CASE)
      SIMPLIFIED_SPECULATIVE_NUMBER_UNOP_LIST(DECLARE_CASE)
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
      DECLARE_CASE(DeoptimizeIf)
      DECLARE_CASE(DeoptimizeUnless)
      DECLARE_CASE(TrapIf)
      DECLARE_CASE(TrapUnless)
      DECLARE_CASE(Return)
      DECLARE_CASE(TailCall)
      DECLARE_CASE(Terminate)
      DECLARE_CASE(OsrNormalEntry)
      DECLARE_CASE(OsrLoopEntry)
      DECLARE_CASE(Throw)
      DECLARE_CASE(End)
      SIMPLIFIED_CHANGE_OP_LIST(DECLARE_CASE)
      SIMPLIFIED_CHECKED_OP_LIST(DECLARE_CASE)
      MACHINE_SIMD_OP_LIST(DECLARE_CASE)
      MACHINE_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
      break;
    }
    return NoChange();
  }

  Type TypeNode(Node* node) {
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
      SIMPLIFIED_COMPARE_BINOP_LIST(DECLARE_CASE)
      SIMPLIFIED_OTHER_OP_LIST(DECLARE_CASE)
      JS_SIMPLE_UNOP_LIST(DECLARE_CASE)
      JS_OBJECT_OP_LIST(DECLARE_CASE)
      JS_CONTEXT_OP_LIST(DECLARE_CASE)
      JS_OTHER_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return TypeBinaryOp(node, x);
      SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_CASE)
      SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_CASE)
#undef DECLARE_CASE

#define DECLARE_CASE(x) \
  case IrOpcode::k##x:  \
    return TypeUnaryOp(node, x);
      SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_CASE)
      SIMPLIFIED_SPECULATIVE_NUMBER_UNOP_LIST(DECLARE_CASE)
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
      DECLARE_CASE(DeoptimizeIf)
      DECLARE_CASE(DeoptimizeUnless)
      DECLARE_CASE(TrapIf)
      DECLARE_CASE(TrapUnless)
      DECLARE_CASE(Return)
      DECLARE_CASE(TailCall)
      DECLARE_CASE(Terminate)
      DECLARE_CASE(OsrNormalEntry)
      DECLARE_CASE(OsrLoopEntry)
      DECLARE_CASE(Throw)
      DECLARE_CASE(End)
      SIMPLIFIED_CHANGE_OP_LIST(DECLARE_CASE)
      SIMPLIFIED_CHECKED_OP_LIST(DECLARE_CASE)
      MACHINE_SIMD_OP_LIST(DECLARE_CASE)
      MACHINE_OP_LIST(DECLARE_CASE)
#undef DECLARE_CASE
      break;
    }
    UNREACHABLE();
  }

  Type TypeConstant(Handle<Object> value);

 private:
  Typer* typer_;
  LoopVariableOptimizer* induction_vars_;
  ZoneSet<NodeId> weakened_nodes_;
  // TODO(tebbi): remove once chromium:906567 is resolved.
  ZoneUnorderedMap<std::pair<Node*, int>, Type> remembered_types_;

#define DECLARE_METHOD(x) inline Type Type##x(Node* node);
  DECLARE_METHOD(Start)
  DECLARE_METHOD(IfException)
  COMMON_OP_LIST(DECLARE_METHOD)
  SIMPLIFIED_COMPARE_BINOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_OTHER_OP_LIST(DECLARE_METHOD)
  JS_OP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  Type TypeOrNone(Node* node) {
    return NodeProperties::IsTyped(node) ? NodeProperties::GetType(node)
                                         : Type::None();
  }

  Type Operand(Node* node, int i) {
    Node* operand_node = NodeProperties::GetValueInput(node, i);
    return TypeOrNone(operand_node);
  }

  Type Weaken(Node* node, Type current_type, Type previous_type);

  Zone* zone() { return typer_->zone(); }
  Graph* graph() { return typer_->graph(); }

  void SetWeakened(NodeId node_id) { weakened_nodes_.insert(node_id); }
  bool IsWeakened(NodeId node_id) {
    return weakened_nodes_.find(node_id) != weakened_nodes_.end();
  }

  typedef Type (*UnaryTyperFun)(Type, Typer* t);
  typedef Type (*BinaryTyperFun)(Type, Type, Typer* t);

  Type TypeUnaryOp(Node* node, UnaryTyperFun);
  Type TypeBinaryOp(Node* node, BinaryTyperFun);

  static Type BinaryNumberOpTyper(Type lhs, Type rhs, Typer* t,
                                  BinaryTyperFun f);

  enum ComparisonOutcomeFlags {
    kComparisonTrue = 1,
    kComparisonFalse = 2,
    kComparisonUndefined = 4
  };
  typedef base::Flags<ComparisonOutcomeFlags> ComparisonOutcome;

  static ComparisonOutcome Invert(ComparisonOutcome, Typer*);
  static Type FalsifyUndefined(ComparisonOutcome, Typer*);

  static Type BitwiseNot(Type, Typer*);
  static Type Decrement(Type, Typer*);
  static Type Increment(Type, Typer*);
  static Type Negate(Type, Typer*);

  static Type ToPrimitive(Type, Typer*);
  static Type ToBoolean(Type, Typer*);
  static Type ToInteger(Type, Typer*);
  static Type ToLength(Type, Typer*);
  static Type ToName(Type, Typer*);
  static Type ToNumber(Type, Typer*);
  static Type ToNumberConvertBigInt(Type, Typer*);
  static Type ToNumeric(Type, Typer*);
  static Type ToObject(Type, Typer*);
  static Type ToString(Type, Typer*);
#define DECLARE_METHOD(Name)               \
  static Type Name(Type type, Typer* t) {  \
    return t->operation_typer_.Name(type); \
  }
  SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_NUMBER_UNOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD
#define DECLARE_METHOD(Name)                       \
  static Type Name(Type lhs, Type rhs, Typer* t) { \
    return t->operation_typer_.Name(lhs, rhs);     \
  }
  SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  static Type ObjectIsArrayBufferView(Type, Typer*);
  static Type ObjectIsBigInt(Type, Typer*);
  static Type ObjectIsCallable(Type, Typer*);
  static Type ObjectIsConstructor(Type, Typer*);
  static Type ObjectIsDetectableCallable(Type, Typer*);
  static Type ObjectIsMinusZero(Type, Typer*);
  static Type NumberIsMinusZero(Type, Typer*);
  static Type ObjectIsNaN(Type, Typer*);
  static Type NumberIsNaN(Type, Typer*);
  static Type ObjectIsNonCallable(Type, Typer*);
  static Type ObjectIsNumber(Type, Typer*);
  static Type ObjectIsReceiver(Type, Typer*);
  static Type ObjectIsSmi(Type, Typer*);
  static Type ObjectIsString(Type, Typer*);
  static Type ObjectIsSymbol(Type, Typer*);
  static Type ObjectIsUndetectable(Type, Typer*);

  static ComparisonOutcome JSCompareTyper(Type, Type, Typer*);
  static ComparisonOutcome NumberCompareTyper(Type, Type, Typer*);

#define DECLARE_METHOD(x) static Type x##Typer(Type, Type, Typer*);
  JS_SIMPLE_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  static Type JSCallTyper(Type, Typer*);

  static Type NumberEqualTyper(Type, Type, Typer*);
  static Type NumberLessThanTyper(Type, Type, Typer*);
  static Type NumberLessThanOrEqualTyper(Type, Type, Typer*);
  static Type ReferenceEqualTyper(Type, Type, Typer*);
  static Type SameValueTyper(Type, Type, Typer*);
  static Type StringFromSingleCharCodeTyper(Type, Typer*);
  static Type StringFromSingleCodePointTyper(Type, Typer*);

  Reduction UpdateType(Node* node, Type current) {
    if (NodeProperties::IsTyped(node)) {
      // Widen the type of a previously typed node.
      Type previous = NodeProperties::GetType(node);
      if (node->opcode() == IrOpcode::kPhi ||
          node->opcode() == IrOpcode::kInductionVariablePhi) {
        // Speed up termination in the presence of range types:
        current = Weaken(node, current, previous);
      }

      if (V8_UNLIKELY(!previous.Is(current))) {
        AllowHandleDereference allow;
        std::ostringstream ostream;
        node->Print(ostream);

        if (V8_UNLIKELY(node->opcode() == IrOpcode::kNumberAdd)) {
          ostream << "Previous UpdateType run (inputs first):";
          for (int i = 0; i < 3; ++i) {
            ostream << "  ";
            if (remembered_types_[{node, i}].IsInvalid()) {
              ostream << "untyped";
            } else {
              remembered_types_[{node, i}].PrintTo(ostream);
            }
          }

          ostream << "\nCurrent (output) type:  ";
          previous.PrintTo(ostream);

          ostream << "\nThis UpdateType run (inputs first):";
          for (int i = 0; i < 2; ++i) {
            ostream << "  ";
            Node* input = NodeProperties::GetValueInput(node, i);
            if (NodeProperties::IsTyped(input)) {
              NodeProperties::GetType(input).PrintTo(ostream);
            } else {
              ostream << "untyped";
            }
          }
          ostream << "  ";
          current.PrintTo(ostream);
          ostream << "\n";
        }

        FATAL("UpdateType error for node %s", ostream.str().c_str());
      }

      if (V8_UNLIKELY(node->opcode() == IrOpcode::kNumberAdd)) {
        for (int i = 0; i < 2; ++i) {
          Node* input = NodeProperties::GetValueInput(node, i);
          remembered_types_[{node, i}] = NodeProperties::IsTyped(input)
                                             ? NodeProperties::GetType(input)
                                             : Type::Invalid();
        }
        remembered_types_[{node, 2}] = current;
      }

      NodeProperties::SetType(node, current);
      if (!current.Is(previous)) {
        // If something changed, revisit all uses.
        return Changed(node);
      }
      return NoChange();
    } else {
      if (V8_UNLIKELY(node->opcode() == IrOpcode::kNumberAdd)) {
        for (int i = 0; i < 2; ++i) {
          Node* input = NodeProperties::GetValueInput(node, i);
          remembered_types_[{node, i}] = NodeProperties::IsTyped(input)
                                             ? NodeProperties::GetType(input)
                                             : Type::Invalid();
        }
        remembered_types_[{node, 2}] = current;
      }

      // No previous type, simply update the type.
      NodeProperties::SetType(node, current);
      return Changed(node);
    }
  }
};

void Typer::Run() { Run(NodeVector(zone()), nullptr); }

void Typer::Run(const NodeVector& roots,
                LoopVariableOptimizer* induction_vars) {
  if (induction_vars != nullptr) {
    induction_vars->ChangeToInductionVariablePhis();
  }
  Visitor visitor(this, induction_vars);
  GraphReducer graph_reducer(zone(), graph());
  graph_reducer.AddReducer(&visitor);
  for (Node* const root : roots) graph_reducer.ReduceNode(root);
  graph_reducer.ReduceGraph();

  if (induction_vars != nullptr) {
    induction_vars->ChangeToPhisAndInsertGuards();
  }
}

void Typer::Decorator::Decorate(Node* node) {
  if (node->op()->ValueOutputCount() > 0) {
    // Only eagerly type-decorate nodes with known input types.
    // Other cases will generally require a proper fixpoint iteration with Run.
    bool is_typed = NodeProperties::IsTyped(node);
    if (is_typed || NodeProperties::AllValueInputsAreTyped(node)) {
      Visitor typing(typer_, nullptr);
      Type type = typing.TypeNode(node);
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

Type Typer::Visitor::TypeUnaryOp(Node* node, UnaryTyperFun f) {
  Type input = Operand(node, 0);
  return input.IsNone() ? Type::None() : f(input, typer_);
}

Type Typer::Visitor::TypeBinaryOp(Node* node, BinaryTyperFun f) {
  Type left = Operand(node, 0);
  Type right = Operand(node, 1);
  return left.IsNone() || right.IsNone() ? Type::None()
                                         : f(left, right, typer_);
}

Type Typer::Visitor::BinaryNumberOpTyper(Type lhs, Type rhs, Typer* t,
                                         BinaryTyperFun f) {
  lhs = ToNumeric(lhs, t);
  rhs = ToNumeric(rhs, t);
  bool lhs_is_number = lhs.Is(Type::Number());
  bool rhs_is_number = rhs.Is(Type::Number());
  if (lhs_is_number && rhs_is_number) {
    return f(lhs, rhs, t);
  }
  // In order to maintain monotonicity, the following two conditions are
  // intentionally asymmetric.
  if (lhs_is_number) {
    return Type::Number();
  }
  if (lhs.Is(Type::BigInt())) {
    return Type::BigInt();
  }
  return Type::Numeric();
}

Typer::Visitor::ComparisonOutcome Typer::Visitor::Invert(
    ComparisonOutcome outcome, Typer* t) {
  ComparisonOutcome result(0);
  if ((outcome & kComparisonUndefined) != 0) result |= kComparisonUndefined;
  if ((outcome & kComparisonTrue) != 0) result |= kComparisonFalse;
  if ((outcome & kComparisonFalse) != 0) result |= kComparisonTrue;
  return result;
}

Type Typer::Visitor::FalsifyUndefined(ComparisonOutcome outcome, Typer* t) {
  if ((outcome & kComparisonFalse) != 0 ||
      (outcome & kComparisonUndefined) != 0) {
    return (outcome & kComparisonTrue) != 0 ? Type::Boolean()
                                            : t->singleton_false_;
  }
  // Type should be non empty, so we know it should be true.
  DCHECK_NE(0, outcome & kComparisonTrue);
  return t->singleton_true_;
}

Type Typer::Visitor::BitwiseNot(Type type, Typer* t) {
  type = ToNumeric(type, t);
  if (type.Is(Type::Number())) {
    return NumberBitwiseXor(type, t->cache_->kSingletonMinusOne, t);
  }
  return Type::Numeric();
}

Type Typer::Visitor::Decrement(Type type, Typer* t) {
  type = ToNumeric(type, t);
  if (type.Is(Type::Number())) {
    return NumberSubtract(type, t->cache_->kSingletonOne, t);
  }
  return Type::Numeric();
}

Type Typer::Visitor::Increment(Type type, Typer* t) {
  type = ToNumeric(type, t);
  if (type.Is(Type::Number())) {
    return NumberAdd(type, t->cache_->kSingletonOne, t);
  }
  return Type::Numeric();
}

Type Typer::Visitor::Negate(Type type, Typer* t) {
  type = ToNumeric(type, t);
  if (type.Is(Type::Number())) {
    return NumberMultiply(type, t->cache_->kSingletonMinusOne, t);
  }
  return Type::Numeric();
}

// Type conversion.

Type Typer::Visitor::ToPrimitive(Type type, Typer* t) {
  if (type.Is(Type::Primitive()) && !type.Maybe(Type::Receiver())) {
    return type;
  }
  return Type::Primitive();
}

Type Typer::Visitor::ToBoolean(Type type, Typer* t) {
  return t->operation_typer()->ToBoolean(type);
}


// static
Type Typer::Visitor::ToInteger(Type type, Typer* t) {
  // ES6 section 7.1.4 ToInteger ( argument )
  type = ToNumber(type, t);
  if (type.Is(t->cache_->kIntegerOrMinusZero)) return type;
  if (type.Is(t->cache_->kIntegerOrMinusZeroOrNaN)) {
    return Type::Union(
        Type::Intersect(type, t->cache_->kIntegerOrMinusZero, t->zone()),
        t->cache_->kSingletonZero, t->zone());
  }
  return t->cache_->kIntegerOrMinusZero;
}


// static
Type Typer::Visitor::ToLength(Type type, Typer* t) {
  // ES6 section 7.1.15 ToLength ( argument )
  type = ToInteger(type, t);
  if (type.IsNone()) return type;
  double min = type.Min();
  double max = type.Max();
  if (max <= 0.0) {
    return Type::NewConstant(0, t->zone());
  }
  if (min >= kMaxSafeInteger) {
    return Type::NewConstant(kMaxSafeInteger, t->zone());
  }
  if (min <= 0.0) min = 0.0;
  if (max >= kMaxSafeInteger) max = kMaxSafeInteger;
  return Type::Range(min, max, t->zone());
}


// static
Type Typer::Visitor::ToName(Type type, Typer* t) {
  // ES6 section 7.1.14 ToPropertyKey ( argument )
  type = ToPrimitive(type, t);
  if (type.Is(Type::Name())) return type;
  if (type.Maybe(Type::Symbol())) return Type::Name();
  return ToString(type, t);
}


// static
Type Typer::Visitor::ToNumber(Type type, Typer* t) {
  return t->operation_typer_.ToNumber(type);
}

// static
Type Typer::Visitor::ToNumberConvertBigInt(Type type, Typer* t) {
  return t->operation_typer_.ToNumberConvertBigInt(type);
}

// static
Type Typer::Visitor::ToNumeric(Type type, Typer* t) {
  return t->operation_typer_.ToNumeric(type);
}

// static
Type Typer::Visitor::ToObject(Type type, Typer* t) {
  // ES6 section 7.1.13 ToObject ( argument )
  if (type.Is(Type::Receiver())) return type;
  if (type.Is(Type::Primitive())) return Type::OtherObject();
  if (!type.Maybe(Type::OtherUndetectable())) {
    return Type::DetectableReceiver();
  }
  return Type::Receiver();
}


// static
Type Typer::Visitor::ToString(Type type, Typer* t) {
  // ES6 section 7.1.12 ToString ( argument )
  type = ToPrimitive(type, t);
  if (type.Is(Type::String())) return type;
  return Type::String();
}

// Type checks.

Type Typer::Visitor::ObjectIsArrayBufferView(Type type, Typer* t) {
  // TODO(turbofan): Introduce a Type::ArrayBufferView?
  if (!type.Maybe(Type::OtherObject())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsBigInt(Type type, Typer* t) {
  if (type.Is(Type::BigInt())) return t->singleton_true_;
  if (!type.Maybe(Type::BigInt())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsCallable(Type type, Typer* t) {
  if (type.Is(Type::Callable())) return t->singleton_true_;
  if (!type.Maybe(Type::Callable())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsConstructor(Type type, Typer* t) {
  // TODO(turbofan): Introduce a Type::Constructor?
  if (!type.Maybe(Type::Callable())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsDetectableCallable(Type type, Typer* t) {
  if (type.Is(Type::DetectableCallable())) return t->singleton_true_;
  if (!type.Maybe(Type::DetectableCallable())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsMinusZero(Type type, Typer* t) {
  if (type.Is(Type::MinusZero())) return t->singleton_true_;
  if (!type.Maybe(Type::MinusZero())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::NumberIsMinusZero(Type type, Typer* t) {
  if (type.Is(Type::MinusZero())) return t->singleton_true_;
  if (!type.Maybe(Type::MinusZero())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsNaN(Type type, Typer* t) {
  if (type.Is(Type::NaN())) return t->singleton_true_;
  if (!type.Maybe(Type::NaN())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::NumberIsNaN(Type type, Typer* t) {
  if (type.Is(Type::NaN())) return t->singleton_true_;
  if (!type.Maybe(Type::NaN())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsNonCallable(Type type, Typer* t) {
  if (type.Is(Type::NonCallable())) return t->singleton_true_;
  if (!type.Maybe(Type::NonCallable())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsNumber(Type type, Typer* t) {
  if (type.Is(Type::Number())) return t->singleton_true_;
  if (!type.Maybe(Type::Number())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsReceiver(Type type, Typer* t) {
  if (type.Is(Type::Receiver())) return t->singleton_true_;
  if (!type.Maybe(Type::Receiver())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsSmi(Type type, Typer* t) {
  if (!type.Maybe(Type::SignedSmall())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsString(Type type, Typer* t) {
  if (type.Is(Type::String())) return t->singleton_true_;
  if (!type.Maybe(Type::String())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsSymbol(Type type, Typer* t) {
  if (type.Is(Type::Symbol())) return t->singleton_true_;
  if (!type.Maybe(Type::Symbol())) return t->singleton_false_;
  return Type::Boolean();
}

Type Typer::Visitor::ObjectIsUndetectable(Type type, Typer* t) {
  if (type.Is(Type::Undetectable())) return t->singleton_true_;
  if (!type.Maybe(Type::Undetectable())) return t->singleton_false_;
  return Type::Boolean();
}


// -----------------------------------------------------------------------------


// Control operators.

Type Typer::Visitor::TypeStart(Node* node) { return Type::Internal(); }

Type Typer::Visitor::TypeIfException(Node* node) { return Type::NonInternal(); }

// Common operators.

Type Typer::Visitor::TypeParameter(Node* node) {
  Node* const start = node->InputAt(0);
  DCHECK_EQ(IrOpcode::kStart, start->opcode());
  int const parameter_count = start->op()->ValueOutputCount() - 4;
  DCHECK_LE(1, parameter_count);
  int const index = ParameterIndexOf(node->op());
  if (index == Linkage::kJSCallClosureParamIndex) {
    return Type::Function();
  } else if (index == 0) {
    if (typer_->flags() & Typer::kThisIsReceiver) {
      return Type::Receiver();
    } else {
      // Parameter[this] can be the_hole for derived class constructors.
      return Type::Union(Type::Hole(), Type::NonInternal(), typer_->zone());
    }
  } else if (index == Linkage::GetJSCallNewTargetParamIndex(parameter_count)) {
    if (typer_->flags() & Typer::kNewTargetIsReceiver) {
      return Type::Receiver();
    } else {
      return Type::Union(Type::Receiver(), Type::Undefined(), typer_->zone());
    }
  } else if (index == Linkage::GetJSCallArgCountParamIndex(parameter_count)) {
    return Type::Range(0.0, FixedArray::kMaxLength, typer_->zone());
  } else if (index == Linkage::GetJSCallContextParamIndex(parameter_count)) {
    return Type::OtherInternal();
  }
  return Type::NonInternal();
}

Type Typer::Visitor::TypeOsrValue(Node* node) { return Type::Any(); }

Type Typer::Visitor::TypeRetain(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeInt32Constant(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeInt64Constant(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeRelocatableInt32Constant(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeRelocatableInt64Constant(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeFloat32Constant(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeFloat64Constant(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeNumberConstant(Node* node) {
  double number = OpParameter<double>(node->op());
  return Type::NewConstant(number, zone());
}

Type Typer::Visitor::TypeHeapConstant(Node* node) {
  return TypeConstant(HeapConstantOf(node->op()));
}

Type Typer::Visitor::TypeExternalConstant(Node* node) {
  return Type::ExternalPointer();
}

Type Typer::Visitor::TypePointerConstant(Node* node) {
  return Type::ExternalPointer();
}

Type Typer::Visitor::TypeSelect(Node* node) {
  return Type::Union(Operand(node, 1), Operand(node, 2), zone());
}

Type Typer::Visitor::TypePhi(Node* node) {
  int arity = node->op()->ValueInputCount();
  Type type = Operand(node, 0);
  for (int i = 1; i < arity; ++i) {
    type = Type::Union(type, Operand(node, i), zone());
  }
  return type;
}

Type Typer::Visitor::TypeInductionVariablePhi(Node* node) {
  int arity = NodeProperties::GetControlInput(node)->op()->ControlInputCount();
  DCHECK_EQ(IrOpcode::kLoop, NodeProperties::GetControlInput(node)->opcode());
  DCHECK_EQ(2, NodeProperties::GetControlInput(node)->InputCount());

  Type initial_type = Operand(node, 0);
  Type increment_type = Operand(node, 2);

  // We only handle integer induction variables (otherwise ranges
  // do not apply and we cannot do anything).
  if (!initial_type.Is(typer_->cache_->kInteger) ||
      !increment_type.Is(typer_->cache_->kInteger)) {
    // Fallback to normal phi typing, but ensure monotonicity.
    // (Unfortunately, without baking in the previous type, monotonicity might
    // be violated because we might not yet have retyped the incrementing
    // operation even though the increment's type might been already reflected
    // in the induction variable phi.)
    Type type = NodeProperties::IsTyped(node) ? NodeProperties::GetType(node)
                                              : Type::None();
    for (int i = 0; i < arity; ++i) {
      type = Type::Union(type, Operand(node, i), zone());
    }
    return type;
  }
  // If we do not have enough type information for the initial value or
  // the increment, just return the initial value's type.
  if (initial_type.IsNone() ||
      increment_type.Is(typer_->cache_->kSingletonZero)) {
    return initial_type;
  }

  // Now process the bounds.
  auto res = induction_vars_->induction_variables().find(node->id());
  DCHECK(res != induction_vars_->induction_variables().end());
  InductionVariable* induction_var = res->second;

  InductionVariable::ArithmeticType arithmetic_type = induction_var->Type();

  double min = -V8_INFINITY;
  double max = V8_INFINITY;

  double increment_min;
  double increment_max;
  if (arithmetic_type == InductionVariable::ArithmeticType::kAddition) {
    increment_min = increment_type.Min();
    increment_max = increment_type.Max();
  } else {
    DCHECK_EQ(InductionVariable::ArithmeticType::kSubtraction, arithmetic_type);
    increment_min = -increment_type.Max();
    increment_max = -increment_type.Min();
  }

  if (increment_min >= 0) {
    // increasing sequence
    min = initial_type.Min();
    for (auto bound : induction_var->upper_bounds()) {
      Type bound_type = TypeOrNone(bound.bound);
      // If the type is not an integer, just skip the bound.
      if (!bound_type.Is(typer_->cache_->kInteger)) continue;
      // If the type is not inhabited, then we can take the initial value.
      if (bound_type.IsNone()) {
        max = initial_type.Max();
        break;
      }
      double bound_max = bound_type.Max();
      if (bound.kind == InductionVariable::kStrict) {
        bound_max -= 1;
      }
      max = std::min(max, bound_max + increment_max);
    }
    // The upper bound must be at least the initial value's upper bound.
    max = std::max(max, initial_type.Max());
  } else if (increment_max <= 0) {
    // decreasing sequence
    max = initial_type.Max();
    for (auto bound : induction_var->lower_bounds()) {
      Type bound_type = TypeOrNone(bound.bound);
      // If the type is not an integer, just skip the bound.
      if (!bound_type.Is(typer_->cache_->kInteger)) continue;
      // If the type is not inhabited, then we can take the initial value.
      if (bound_type.IsNone()) {
        min = initial_type.Min();
        break;
      }
      double bound_min = bound_type.Min();
      if (bound.kind == InductionVariable::kStrict) {
        bound_min += 1;
      }
      min = std::max(min, bound_min + increment_min);
    }
    // The lower bound must be at most the initial value's lower bound.
    min = std::min(min, initial_type.Min());
  } else {
    // Shortcut: If the increment can be both positive and negative,
    // the variable can go arbitrarily far, so just return integer.
    return typer_->cache_->kInteger;
  }
  if (FLAG_trace_turbo_loop) {
    StdoutStream{} << std::setprecision(10) << "Loop ("
                   << NodeProperties::GetControlInput(node)->id()
                   << ") variable bounds in "
                   << (arithmetic_type ==
                               InductionVariable::ArithmeticType::kAddition
                           ? "addition"
                           : "subtraction")
                   << " for phi " << node->id() << ": (" << min << ", " << max
                   << ")\n";
  }
  return Type::Range(min, max, typer_->zone());
}

Type Typer::Visitor::TypeEffectPhi(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeLoopExit(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeLoopExitValue(Node* node) { return Operand(node, 0); }

Type Typer::Visitor::TypeLoopExitEffect(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeEnsureWritableFastElements(Node* node) {
  return Operand(node, 1);
}

Type Typer::Visitor::TypeMaybeGrowFastElements(Node* node) {
  return Operand(node, 1);
}

Type Typer::Visitor::TypeTransitionElementsKind(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeCheckpoint(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeBeginRegion(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeFinishRegion(Node* node) { return Operand(node, 0); }

Type Typer::Visitor::TypeFrameState(Node* node) {
  // TODO(rossberg): Ideally FrameState wouldn't have a value output.
  return Type::Internal();
}

Type Typer::Visitor::TypeStateValues(Node* node) { return Type::Internal(); }

Type Typer::Visitor::TypeTypedStateValues(Node* node) {
  return Type::Internal();
}

Type Typer::Visitor::TypeObjectId(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeArgumentsElementsState(Node* node) {
  return Type::Internal();
}

Type Typer::Visitor::TypeArgumentsLengthState(Node* node) {
  return Type::Internal();
}

Type Typer::Visitor::TypeObjectState(Node* node) { return Type::Internal(); }

Type Typer::Visitor::TypeTypedObjectState(Node* node) {
  return Type::Internal();
}

Type Typer::Visitor::TypeCall(Node* node) { return Type::Any(); }

Type Typer::Visitor::TypeCallWithCallerSavedRegisters(Node* node) {
  UNREACHABLE();
}

Type Typer::Visitor::TypeProjection(Node* node) {
  Type const type = Operand(node, 0);
  if (type.Is(Type::None())) return Type::None();
  int const index = static_cast<int>(ProjectionIndexOf(node->op()));
  if (type.IsTuple() && index < type.AsTuple()->Arity()) {
    return type.AsTuple()->Element(index);
  }
  return Type::Any();
}

Type Typer::Visitor::TypeMapGuard(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeTypeGuard(Node* node) {
  Type const type = Operand(node, 0);
  return typer_->operation_typer()->TypeTypeGuard(node->op(), type);
}

Type Typer::Visitor::TypeDead(Node* node) { return Type::None(); }

Type Typer::Visitor::TypeDeadValue(Node* node) { return Type::None(); }

Type Typer::Visitor::TypeUnreachable(Node* node) { return Type::None(); }

// JS comparison operators.

Type Typer::Visitor::JSEqualTyper(Type lhs, Type rhs, Typer* t) {
  if (lhs.Is(Type::NaN()) || rhs.Is(Type::NaN())) return t->singleton_false_;
  if (lhs.Is(Type::NullOrUndefined()) && rhs.Is(Type::NullOrUndefined())) {
    return t->singleton_true_;
  }
  if (lhs.Is(Type::Number()) && rhs.Is(Type::Number()) &&
      (lhs.Max() < rhs.Min() || lhs.Min() > rhs.Max())) {
    return t->singleton_false_;
  }
  if (lhs.IsHeapConstant() && rhs.Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value,
    // which is not nan due to the earlier check.
    return t->singleton_true_;
  }
  return Type::Boolean();
}

Type Typer::Visitor::JSStrictEqualTyper(Type lhs, Type rhs, Typer* t) {
  return t->operation_typer()->StrictEqual(lhs, rhs);
}

// The EcmaScript specification defines the four relational comparison operators
// (<, <=, >=, >) with the help of a single abstract one.  It behaves like <
// but returns undefined when the inputs cannot be compared.
// We implement the typing analogously.
Typer::Visitor::ComparisonOutcome Typer::Visitor::JSCompareTyper(Type lhs,
                                                                 Type rhs,
                                                                 Typer* t) {
  lhs = ToPrimitive(lhs, t);
  rhs = ToPrimitive(rhs, t);
  if (lhs.Maybe(Type::String()) && rhs.Maybe(Type::String())) {
    return ComparisonOutcome(kComparisonTrue) |
           ComparisonOutcome(kComparisonFalse);
  }
  lhs = ToNumeric(lhs, t);
  rhs = ToNumeric(rhs, t);
  if (lhs.Is(Type::Number()) && rhs.Is(Type::Number())) {
    return NumberCompareTyper(lhs, rhs, t);
  }
  return ComparisonOutcome(kComparisonTrue) |
         ComparisonOutcome(kComparisonFalse) |
         ComparisonOutcome(kComparisonUndefined);
}

Typer::Visitor::ComparisonOutcome Typer::Visitor::NumberCompareTyper(Type lhs,
                                                                     Type rhs,
                                                                     Typer* t) {
  DCHECK(lhs.Is(Type::Number()));
  DCHECK(rhs.Is(Type::Number()));

  // Shortcut for NaNs.
  if (lhs.Is(Type::NaN()) || rhs.Is(Type::NaN())) return kComparisonUndefined;

  ComparisonOutcome result;
  if (lhs.IsHeapConstant() && rhs.Is(lhs)) {
    // Types are equal and are inhabited only by a single semantic value.
    result = kComparisonFalse;
  } else if (lhs.Min() >= rhs.Max()) {
    result = kComparisonFalse;
  } else if (lhs.Max() < rhs.Min()) {
    result = kComparisonTrue;
  } else {
    // We cannot figure out the result, return both true and false. (We do not
    // have to return undefined because that cannot affect the result of
    // FalsifyUndefined.)
    return ComparisonOutcome(kComparisonTrue) |
           ComparisonOutcome(kComparisonFalse);
  }
  // Add the undefined if we could see NaN.
  if (lhs.Maybe(Type::NaN()) || rhs.Maybe(Type::NaN())) {
    result |= kComparisonUndefined;
  }
  return result;
}

Type Typer::Visitor::JSLessThanTyper(Type lhs, Type rhs, Typer* t) {
  return FalsifyUndefined(JSCompareTyper(lhs, rhs, t), t);
}

Type Typer::Visitor::JSGreaterThanTyper(Type lhs, Type rhs, Typer* t) {
  return FalsifyUndefined(JSCompareTyper(rhs, lhs, t), t);
}

Type Typer::Visitor::JSLessThanOrEqualTyper(Type lhs, Type rhs, Typer* t) {
  return FalsifyUndefined(Invert(JSCompareTyper(rhs, lhs, t), t), t);
}

Type Typer::Visitor::JSGreaterThanOrEqualTyper(Type lhs, Type rhs, Typer* t) {
  return FalsifyUndefined(Invert(JSCompareTyper(lhs, rhs, t), t), t);
}

// JS bitwise operators.

Type Typer::Visitor::JSBitwiseOrTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberBitwiseOr);
}

Type Typer::Visitor::JSBitwiseAndTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberBitwiseAnd);
}

Type Typer::Visitor::JSBitwiseXorTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberBitwiseXor);
}

Type Typer::Visitor::JSShiftLeftTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberShiftLeft);
}

Type Typer::Visitor::JSShiftRightTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberShiftRight);
}

Type Typer::Visitor::JSShiftRightLogicalTyper(Type lhs, Type rhs, Typer* t) {
  return NumberShiftRightLogical(ToNumber(lhs, t), ToNumber(rhs, t), t);
}


// JS arithmetic operators.

Type Typer::Visitor::JSAddTyper(Type lhs, Type rhs, Typer* t) {
  lhs = ToPrimitive(lhs, t);
  rhs = ToPrimitive(rhs, t);
  if (lhs.Maybe(Type::String()) || rhs.Maybe(Type::String())) {
    if (lhs.Is(Type::String()) || rhs.Is(Type::String())) {
      return Type::String();
    } else {
      return Type::NumericOrString();
    }
  }
  // The addition must be numeric.
  return BinaryNumberOpTyper(lhs, rhs, t, NumberAdd);
}

Type Typer::Visitor::JSSubtractTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberSubtract);
}

Type Typer::Visitor::JSMultiplyTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberMultiply);
}

Type Typer::Visitor::JSDivideTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberDivide);
}

Type Typer::Visitor::JSModulusTyper(Type lhs, Type rhs, Typer* t) {
  return BinaryNumberOpTyper(lhs, rhs, t, NumberModulus);
}

Type Typer::Visitor::JSExponentiateTyper(Type lhs, Type rhs, Typer* t) {
  // TODO(neis): Refine using BinaryNumberOpTyper?
  return Type::Numeric();
}

// JS unary operators.

Type Typer::Visitor::TypeJSBitwiseNot(Node* node) {
  return TypeUnaryOp(node, BitwiseNot);
}

Type Typer::Visitor::TypeJSDecrement(Node* node) {
  return TypeUnaryOp(node, Decrement);
}

Type Typer::Visitor::TypeJSIncrement(Node* node) {
  return TypeUnaryOp(node, Increment);
}

Type Typer::Visitor::TypeJSNegate(Node* node) {
  return TypeUnaryOp(node, Negate);
}

Type Typer::Visitor::TypeTypeOf(Node* node) {
  return Type::InternalizedString();
}


// JS conversion operators.

Type Typer::Visitor::TypeToBoolean(Node* node) {
  return TypeUnaryOp(node, ToBoolean);
}

Type Typer::Visitor::TypeJSToLength(Node* node) {
  return TypeUnaryOp(node, ToLength);
}

Type Typer::Visitor::TypeJSToName(Node* node) {
  return TypeUnaryOp(node, ToName);
}

Type Typer::Visitor::TypeJSToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}

Type Typer::Visitor::TypeJSToNumberConvertBigInt(Node* node) {
  return TypeUnaryOp(node, ToNumberConvertBigInt);
}

Type Typer::Visitor::TypeJSToNumeric(Node* node) {
  return TypeUnaryOp(node, ToNumeric);
}

Type Typer::Visitor::TypeJSToObject(Node* node) {
  return TypeUnaryOp(node, ToObject);
}

Type Typer::Visitor::TypeJSToString(Node* node) {
  return TypeUnaryOp(node, ToString);
}

// JS object operators.

Type Typer::Visitor::TypeJSCreate(Node* node) { return Type::Object(); }

Type Typer::Visitor::TypeJSCreateArguments(Node* node) {
  switch (CreateArgumentsTypeOf(node->op())) {
    case CreateArgumentsType::kRestParameter:
      return Type::Array();
    case CreateArgumentsType::kMappedArguments:
    case CreateArgumentsType::kUnmappedArguments:
      return Type::OtherObject();
  }
  UNREACHABLE();
}

Type Typer::Visitor::TypeJSCreateArray(Node* node) { return Type::Array(); }

Type Typer::Visitor::TypeJSCreateArrayIterator(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateAsyncFunctionObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateCollectionIterator(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateBoundFunction(Node* node) {
  return Type::BoundFunction();
}

Type Typer::Visitor::TypeJSCreateGeneratorObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateClosure(Node* node) {
  return Type::Function();
}

Type Typer::Visitor::TypeJSCreateIterResultObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateStringIterator(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateKeyValueArray(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreatePromise(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateTypedArray(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateLiteralArray(Node* node) {
  return Type::Array();
}

Type Typer::Visitor::TypeJSCreateEmptyLiteralArray(Node* node) {
  return Type::Array();
}

Type Typer::Visitor::TypeJSCreateArrayFromIterable(Node* node) {
  return Type::Array();
}

Type Typer::Visitor::TypeJSCreateLiteralObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateEmptyLiteralObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCloneObject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSCreateLiteralRegExp(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSLoadProperty(Node* node) {
  return Type::NonInternal();
}

Type Typer::Visitor::TypeJSLoadNamed(Node* node) { return Type::NonInternal(); }

Type Typer::Visitor::TypeJSLoadGlobal(Node* node) {
  return Type::NonInternal();
}

Type Typer::Visitor::TypeJSParseInt(Node* node) { return Type::Number(); }

Type Typer::Visitor::TypeJSRegExpTest(Node* node) { return Type::Boolean(); }

// Returns a somewhat larger range if we previously assigned
// a (smaller) range to this node. This is used  to speed up
// the fixpoint calculation in case there appears to be a loop
// in the graph. In the current implementation, we are
// increasing the limits to the closest power of two.
Type Typer::Visitor::Weaken(Node* node, Type current_type, Type previous_type) {
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
  Type const integer = typer_->cache_->kInteger;
  if (!previous_type.Maybe(integer)) {
    return current_type;
  }
  DCHECK(current_type.Maybe(integer));

  Type current_integer = Type::Intersect(current_type, integer, zone());
  Type previous_integer = Type::Intersect(previous_type, integer, zone());

  // Once we start weakening a node, we should always weaken.
  if (!IsWeakened(node->id())) {
    // Only weaken if there is range involved; we should converge quickly
    // for all other types (the exception is a union of many constants,
    // but we currently do not increase the number of constants in unions).
    Type previous = previous_integer.GetRange();
    Type current = current_integer.GetRange();
    if (current.IsInvalid() || previous.IsInvalid()) {
      return current_type;
    }
    // Range is involved => we are weakening.
    SetWeakened(node->id());
  }

  double current_min = current_integer.Min();
  double new_min = current_min;
  // Find the closest lower entry in the list of allowed
  // minima (or negative infinity if there is no such entry).
  if (current_min != previous_integer.Min()) {
    new_min = -V8_INFINITY;
    for (double const min : kWeakenMinLimits) {
      if (min <= current_min) {
        new_min = min;
        break;
      }
    }
  }

  double current_max = current_integer.Max();
  double new_max = current_max;
  // Find the closest greater entry in the list of allowed
  // maxima (or infinity if there is no such entry).
  if (current_max != previous_integer.Max()) {
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

Type Typer::Visitor::TypeJSStoreProperty(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSStoreNamed(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSStoreGlobal(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSStoreNamedOwn(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSStoreDataPropertyInLiteral(Node* node) {
  UNREACHABLE();
}

Type Typer::Visitor::TypeJSStoreInArrayLiteral(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSDeleteProperty(Node* node) {
  return Type::Boolean();
}

Type Typer::Visitor::TypeJSHasProperty(Node* node) { return Type::Boolean(); }

// JS instanceof operator.

Type Typer::Visitor::JSHasInPrototypeChainTyper(Type lhs, Type rhs, Typer* t) {
  return Type::Boolean();
}

Type Typer::Visitor::JSInstanceOfTyper(Type lhs, Type rhs, Typer* t) {
  return Type::Boolean();
}

Type Typer::Visitor::JSOrdinaryHasInstanceTyper(Type lhs, Type rhs, Typer* t) {
  return Type::Boolean();
}

Type Typer::Visitor::TypeJSGetSuperConstructor(Node* node) {
  return Type::Callable();
}

// JS context operators.

Type Typer::Visitor::TypeJSLoadContext(Node* node) {
  ContextAccess const& access = ContextAccessOf(node->op());
  switch (access.index()) {
    case Context::PREVIOUS_INDEX:
    case Context::NATIVE_CONTEXT_INDEX:
    case Context::SCOPE_INFO_INDEX:
      return Type::OtherInternal();
    default:
      return Type::Any();
  }
}

Type Typer::Visitor::TypeJSStoreContext(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSCreateFunctionContext(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeJSCreateCatchContext(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeJSCreateWithContext(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeJSCreateBlockContext(Node* node) {
  return Type::OtherInternal();
}

// JS other operators.

Type Typer::Visitor::TypeJSConstructForwardVarargs(Node* node) {
  return Type::Receiver();
}

Type Typer::Visitor::TypeJSConstruct(Node* node) { return Type::Receiver(); }

Type Typer::Visitor::TypeJSConstructWithArrayLike(Node* node) {
  return Type::Receiver();
}

Type Typer::Visitor::TypeJSConstructWithSpread(Node* node) {
  return Type::Receiver();
}

Type Typer::Visitor::TypeJSObjectIsArray(Node* node) { return Type::Boolean(); }

Type Typer::Visitor::TypeDateNow(Node* node) { return Type::Number(); }

Type Typer::Visitor::JSCallTyper(Type fun, Typer* t) {
  if (!fun.IsHeapConstant() || !fun.AsHeapConstant()->Ref().IsJSFunction()) {
    return Type::NonInternal();
  }
  JSFunctionRef function = fun.AsHeapConstant()->Ref().AsJSFunction();
  if (!function.shared().HasBuiltinFunctionId()) {
    return Type::NonInternal();
  }
  switch (function.shared().builtin_function_id()) {
    case BuiltinFunctionId::kMathRandom:
      return Type::PlainNumber();
    case BuiltinFunctionId::kMathFloor:
    case BuiltinFunctionId::kMathCeil:
    case BuiltinFunctionId::kMathRound:
    case BuiltinFunctionId::kMathTrunc:
      return t->cache_->kIntegerOrMinusZeroOrNaN;
    // Unary math functions.
    case BuiltinFunctionId::kMathAbs:
    case BuiltinFunctionId::kMathExp:
      return Type::Union(Type::PlainNumber(), Type::NaN(), t->zone());
    case BuiltinFunctionId::kMathAcos:
    case BuiltinFunctionId::kMathAcosh:
    case BuiltinFunctionId::kMathAsin:
    case BuiltinFunctionId::kMathAsinh:
    case BuiltinFunctionId::kMathAtan:
    case BuiltinFunctionId::kMathAtanh:
    case BuiltinFunctionId::kMathCbrt:
    case BuiltinFunctionId::kMathCos:
    case BuiltinFunctionId::kMathExpm1:
    case BuiltinFunctionId::kMathFround:
    case BuiltinFunctionId::kMathLog:
    case BuiltinFunctionId::kMathLog1p:
    case BuiltinFunctionId::kMathLog10:
    case BuiltinFunctionId::kMathLog2:
    case BuiltinFunctionId::kMathSin:
    case BuiltinFunctionId::kMathSqrt:
    case BuiltinFunctionId::kMathTan:
      return Type::Number();
    case BuiltinFunctionId::kMathSign:
      return t->cache_->kMinusOneToOneOrMinusZeroOrNaN;
    // Binary math functions.
    case BuiltinFunctionId::kMathAtan2:
    case BuiltinFunctionId::kMathPow:
    case BuiltinFunctionId::kMathMax:
    case BuiltinFunctionId::kMathMin:
      return Type::Number();
    case BuiltinFunctionId::kMathImul:
      return Type::Signed32();
    case BuiltinFunctionId::kMathClz32:
      return t->cache_->kZeroToThirtyTwo;
    // Date functions.
    case BuiltinFunctionId::kDateNow:
      return t->cache_->kTimeValueType;
    case BuiltinFunctionId::kDateGetDate:
      return t->cache_->kJSDateDayType;
    case BuiltinFunctionId::kDateGetDay:
      return t->cache_->kJSDateWeekdayType;
    case BuiltinFunctionId::kDateGetFullYear:
      return t->cache_->kJSDateYearType;
    case BuiltinFunctionId::kDateGetHours:
      return t->cache_->kJSDateHourType;
    case BuiltinFunctionId::kDateGetMilliseconds:
      return Type::Union(Type::Range(0.0, 999.0, t->zone()), Type::NaN(),
                         t->zone());
    case BuiltinFunctionId::kDateGetMinutes:
      return t->cache_->kJSDateMinuteType;
    case BuiltinFunctionId::kDateGetMonth:
      return t->cache_->kJSDateMonthType;
    case BuiltinFunctionId::kDateGetSeconds:
      return t->cache_->kJSDateSecondType;
    case BuiltinFunctionId::kDateGetTime:
      return t->cache_->kJSDateValueType;

    // Symbol functions.
    case BuiltinFunctionId::kSymbolConstructor:
      return Type::Symbol();
    case BuiltinFunctionId::kSymbolPrototypeToString:
      return Type::String();
    case BuiltinFunctionId::kSymbolPrototypeValueOf:
      return Type::Symbol();

    // BigInt functions.
    case BuiltinFunctionId::kBigIntConstructor:
      return Type::BigInt();

    // Number functions.
    case BuiltinFunctionId::kNumberConstructor:
      return Type::Number();
    case BuiltinFunctionId::kNumberIsFinite:
    case BuiltinFunctionId::kNumberIsInteger:
    case BuiltinFunctionId::kNumberIsNaN:
    case BuiltinFunctionId::kNumberIsSafeInteger:
      return Type::Boolean();
    case BuiltinFunctionId::kNumberParseFloat:
      return Type::Number();
    case BuiltinFunctionId::kNumberParseInt:
      return t->cache_->kIntegerOrMinusZeroOrNaN;
    case BuiltinFunctionId::kNumberToString:
      return Type::String();

    // String functions.
    case BuiltinFunctionId::kStringConstructor:
      return Type::String();
    case BuiltinFunctionId::kStringCharCodeAt:
      return Type::Union(Type::Range(0, kMaxUInt16, t->zone()), Type::NaN(),
                         t->zone());
    case BuiltinFunctionId::kStringCharAt:
      return Type::String();
    case BuiltinFunctionId::kStringCodePointAt:
      return Type::Union(Type::Range(0.0, String::kMaxCodePoint, t->zone()),
                         Type::Undefined(), t->zone());
    case BuiltinFunctionId::kStringConcat:
    case BuiltinFunctionId::kStringFromCharCode:
    case BuiltinFunctionId::kStringFromCodePoint:
      return Type::String();
    case BuiltinFunctionId::kStringIndexOf:
    case BuiltinFunctionId::kStringLastIndexOf:
      return Type::Range(-1.0, String::kMaxLength, t->zone());
    case BuiltinFunctionId::kStringEndsWith:
    case BuiltinFunctionId::kStringIncludes:
      return Type::Boolean();
    case BuiltinFunctionId::kStringRaw:
    case BuiltinFunctionId::kStringRepeat:
    case BuiltinFunctionId::kStringSlice:
      return Type::String();
    case BuiltinFunctionId::kStringStartsWith:
      return Type::Boolean();
    case BuiltinFunctionId::kStringSubstr:
    case BuiltinFunctionId::kStringSubstring:
    case BuiltinFunctionId::kStringToLowerCase:
    case BuiltinFunctionId::kStringToString:
    case BuiltinFunctionId::kStringToUpperCase:
    case BuiltinFunctionId::kStringTrim:
    case BuiltinFunctionId::kStringTrimEnd:
    case BuiltinFunctionId::kStringTrimStart:
    case BuiltinFunctionId::kStringValueOf:
      return Type::String();

    case BuiltinFunctionId::kStringIterator:
    case BuiltinFunctionId::kStringIteratorNext:
      return Type::OtherObject();

    case BuiltinFunctionId::kArrayEntries:
    case BuiltinFunctionId::kArrayKeys:
    case BuiltinFunctionId::kArrayValues:
    case BuiltinFunctionId::kTypedArrayEntries:
    case BuiltinFunctionId::kTypedArrayKeys:
    case BuiltinFunctionId::kTypedArrayValues:
    case BuiltinFunctionId::kArrayIteratorNext:
    case BuiltinFunctionId::kMapIteratorNext:
    case BuiltinFunctionId::kSetIteratorNext:
      return Type::OtherObject();
    case BuiltinFunctionId::kTypedArrayToStringTag:
      return Type::Union(Type::InternalizedString(), Type::Undefined(),
                         t->zone());

    // Array functions.
    case BuiltinFunctionId::kArrayIsArray:
      return Type::Boolean();
    case BuiltinFunctionId::kArrayConcat:
      return Type::Receiver();
    case BuiltinFunctionId::kArrayEvery:
      return Type::Boolean();
    case BuiltinFunctionId::kArrayFill:
    case BuiltinFunctionId::kArrayFilter:
      return Type::Receiver();
    case BuiltinFunctionId::kArrayFindIndex:
      return Type::Range(-1, kMaxSafeInteger, t->zone());
    case BuiltinFunctionId::kArrayForEach:
      return Type::Undefined();
    case BuiltinFunctionId::kArrayIncludes:
      return Type::Boolean();
    case BuiltinFunctionId::kArrayIndexOf:
      return Type::Range(-1, kMaxSafeInteger, t->zone());
    case BuiltinFunctionId::kArrayJoin:
      return Type::String();
    case BuiltinFunctionId::kArrayLastIndexOf:
      return Type::Range(-1, kMaxSafeInteger, t->zone());
    case BuiltinFunctionId::kArrayMap:
      return Type::Receiver();
    case BuiltinFunctionId::kArrayPush:
      return t->cache_->kPositiveSafeInteger;
    case BuiltinFunctionId::kArrayReverse:
    case BuiltinFunctionId::kArraySlice:
      return Type::Receiver();
    case BuiltinFunctionId::kArraySome:
      return Type::Boolean();
    case BuiltinFunctionId::kArraySplice:
      return Type::Receiver();
    case BuiltinFunctionId::kArrayUnshift:
      return t->cache_->kPositiveSafeInteger;

    // ArrayBuffer functions.
    case BuiltinFunctionId::kArrayBufferIsView:
      return Type::Boolean();

    // Object functions.
    case BuiltinFunctionId::kObjectAssign:
      return Type::Receiver();
    case BuiltinFunctionId::kObjectCreate:
      return Type::OtherObject();
    case BuiltinFunctionId::kObjectIs:
    case BuiltinFunctionId::kObjectHasOwnProperty:
    case BuiltinFunctionId::kObjectIsPrototypeOf:
      return Type::Boolean();
    case BuiltinFunctionId::kObjectToString:
      return Type::String();

    case BuiltinFunctionId::kPromiseAll:
      return Type::Receiver();
    case BuiltinFunctionId::kPromisePrototypeThen:
      return Type::Receiver();
    case BuiltinFunctionId::kPromiseRace:
      return Type::Receiver();
    case BuiltinFunctionId::kPromiseReject:
      return Type::Receiver();
    case BuiltinFunctionId::kPromiseResolve:
      return Type::Receiver();

    // RegExp functions.
    case BuiltinFunctionId::kRegExpCompile:
      return Type::OtherObject();
    case BuiltinFunctionId::kRegExpExec:
      return Type::Union(Type::Array(), Type::Null(), t->zone());
    case BuiltinFunctionId::kRegExpTest:
      return Type::Boolean();
    case BuiltinFunctionId::kRegExpToString:
      return Type::String();

    // Function functions.
    case BuiltinFunctionId::kFunctionBind:
      return Type::BoundFunction();
    case BuiltinFunctionId::kFunctionHasInstance:
      return Type::Boolean();

    // Global functions.
    case BuiltinFunctionId::kGlobalDecodeURI:
    case BuiltinFunctionId::kGlobalDecodeURIComponent:
    case BuiltinFunctionId::kGlobalEncodeURI:
    case BuiltinFunctionId::kGlobalEncodeURIComponent:
    case BuiltinFunctionId::kGlobalEscape:
    case BuiltinFunctionId::kGlobalUnescape:
      return Type::String();
    case BuiltinFunctionId::kGlobalIsFinite:
    case BuiltinFunctionId::kGlobalIsNaN:
      return Type::Boolean();

    // Map functions.
    case BuiltinFunctionId::kMapClear:
    case BuiltinFunctionId::kMapForEach:
      return Type::Undefined();
    case BuiltinFunctionId::kMapDelete:
    case BuiltinFunctionId::kMapHas:
      return Type::Boolean();
    case BuiltinFunctionId::kMapEntries:
    case BuiltinFunctionId::kMapKeys:
    case BuiltinFunctionId::kMapSet:
    case BuiltinFunctionId::kMapValues:
      return Type::OtherObject();

    // Set functions.
    case BuiltinFunctionId::kSetAdd:
    case BuiltinFunctionId::kSetEntries:
    case BuiltinFunctionId::kSetValues:
      return Type::OtherObject();
    case BuiltinFunctionId::kSetClear:
    case BuiltinFunctionId::kSetForEach:
      return Type::Undefined();
    case BuiltinFunctionId::kSetDelete:
    case BuiltinFunctionId::kSetHas:
      return Type::Boolean();

    // WeakMap functions.
    case BuiltinFunctionId::kWeakMapDelete:
    case BuiltinFunctionId::kWeakMapHas:
      return Type::Boolean();
    case BuiltinFunctionId::kWeakMapSet:
      return Type::OtherObject();

    // WeakSet functions.
    case BuiltinFunctionId::kWeakSetAdd:
      return Type::OtherObject();
    case BuiltinFunctionId::kWeakSetDelete:
    case BuiltinFunctionId::kWeakSetHas:
      return Type::Boolean();
    default:
      return Type::NonInternal();
  }
}

Type Typer::Visitor::TypeJSCallForwardVarargs(Node* node) {
  return TypeUnaryOp(node, JSCallTyper);
}

Type Typer::Visitor::TypeJSCall(Node* node) {
  // TODO(bmeurer): We could infer better types if we wouldn't ignore the
  // argument types for the JSCallTyper above.
  return TypeUnaryOp(node, JSCallTyper);
}

Type Typer::Visitor::TypeJSCallWithArrayLike(Node* node) {
  return TypeUnaryOp(node, JSCallTyper);
}

Type Typer::Visitor::TypeJSCallWithSpread(Node* node) {
  return TypeUnaryOp(node, JSCallTyper);
}

Type Typer::Visitor::TypeJSCallRuntime(Node* node) {
  switch (CallRuntimeParametersOf(node->op()).id()) {
    case Runtime::kInlineIsJSReceiver:
      return TypeUnaryOp(node, ObjectIsReceiver);
    case Runtime::kInlineIsSmi:
      return TypeUnaryOp(node, ObjectIsSmi);
    case Runtime::kInlineIsArray:
    case Runtime::kInlineIsTypedArray:
    case Runtime::kInlineIsRegExp:
      return Type::Boolean();
    case Runtime::kInlineCreateIterResultObject:
      return Type::OtherObject();
    case Runtime::kInlineToLength:
      return TypeUnaryOp(node, ToLength);
    case Runtime::kInlineToNumber:
      return TypeUnaryOp(node, ToNumber);
    case Runtime::kInlineToObject:
      return TypeUnaryOp(node, ToObject);
    case Runtime::kInlineToString:
      return TypeUnaryOp(node, ToString);
    case Runtime::kHasInPrototypeChain:
      return Type::Boolean();
    default:
      break;
  }
  // TODO(turbofan): This should be Type::NonInternal(), but unfortunately we
  // have a few weird runtime calls that return the hole or even FixedArrays;
  // change this once those weird runtime calls have been removed.
  return Type::Any();
}

Type Typer::Visitor::TypeJSForInEnumerate(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeJSForInNext(Node* node) {
  return Type::Union(Type::String(), Type::Undefined(), zone());
}

Type Typer::Visitor::TypeJSForInPrepare(Node* node) {
  STATIC_ASSERT(Map::EnumLengthBits::kMax <= FixedArray::kMaxLength);
  Type const cache_type =
      Type::Union(Type::SignedSmall(), Type::OtherInternal(), zone());
  Type const cache_array = Type::OtherInternal();
  Type const cache_length = typer_->cache_->kFixedArrayLengthType;
  return Type::Tuple(cache_type, cache_array, cache_length, zone());
}

Type Typer::Visitor::TypeJSLoadMessage(Node* node) { return Type::Any(); }

Type Typer::Visitor::TypeJSStoreMessage(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSLoadModule(Node* node) { return Type::Any(); }

Type Typer::Visitor::TypeJSStoreModule(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSGeneratorStore(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeJSGeneratorRestoreContinuation(Node* node) {
  return Type::SignedSmall();
}

Type Typer::Visitor::TypeJSGeneratorRestoreContext(Node* node) {
  return Type::Any();
}

Type Typer::Visitor::TypeJSGeneratorRestoreRegister(Node* node) {
  return Type::Any();
}

Type Typer::Visitor::TypeJSGeneratorRestoreInputOrDebugPos(Node* node) {
  return Type::Any();
}

Type Typer::Visitor::TypeJSStackCheck(Node* node) { return Type::Any(); }

Type Typer::Visitor::TypeJSDebugger(Node* node) { return Type::Any(); }

Type Typer::Visitor::TypeJSAsyncFunctionEnter(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSAsyncFunctionReject(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSAsyncFunctionResolve(Node* node) {
  return Type::OtherObject();
}

Type Typer::Visitor::TypeJSFulfillPromise(Node* node) {
  return Type::Undefined();
}

Type Typer::Visitor::TypeJSPerformPromiseThen(Node* node) {
  return Type::Receiver();
}

Type Typer::Visitor::TypeJSPromiseResolve(Node* node) {
  return Type::Receiver();
}

Type Typer::Visitor::TypeJSRejectPromise(Node* node) {
  return Type::Undefined();
}

Type Typer::Visitor::TypeJSResolvePromise(Node* node) {
  return Type::Undefined();
}

// Simplified operators.

Type Typer::Visitor::TypeBooleanNot(Node* node) { return Type::Boolean(); }

// static
Type Typer::Visitor::NumberEqualTyper(Type lhs, Type rhs, Typer* t) {
  return JSEqualTyper(ToNumber(lhs, t), ToNumber(rhs, t), t);
}

// static
Type Typer::Visitor::NumberLessThanTyper(Type lhs, Type rhs, Typer* t) {
  return FalsifyUndefined(
      NumberCompareTyper(ToNumber(lhs, t), ToNumber(rhs, t), t), t);
}

// static
Type Typer::Visitor::NumberLessThanOrEqualTyper(Type lhs, Type rhs, Typer* t) {
  return FalsifyUndefined(
      Invert(JSCompareTyper(ToNumber(rhs, t), ToNumber(lhs, t), t), t), t);
}

Type Typer::Visitor::TypeNumberEqual(Node* node) {
  return TypeBinaryOp(node, NumberEqualTyper);
}

Type Typer::Visitor::TypeNumberLessThan(Node* node) {
  return TypeBinaryOp(node, NumberLessThanTyper);
}

Type Typer::Visitor::TypeNumberLessThanOrEqual(Node* node) {
  return TypeBinaryOp(node, NumberLessThanOrEqualTyper);
}

Type Typer::Visitor::TypeSpeculativeNumberEqual(Node* node) {
  return TypeBinaryOp(node, NumberEqualTyper);
}

Type Typer::Visitor::TypeSpeculativeNumberLessThan(Node* node) {
  return TypeBinaryOp(node, NumberLessThanTyper);
}

Type Typer::Visitor::TypeSpeculativeNumberLessThanOrEqual(Node* node) {
  return TypeBinaryOp(node, NumberLessThanOrEqualTyper);
}

Type Typer::Visitor::TypeStringConcat(Node* node) { return Type::String(); }

Type Typer::Visitor::TypeStringToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}

Type Typer::Visitor::TypePlainPrimitiveToNumber(Node* node) {
  return TypeUnaryOp(node, ToNumber);
}

Type Typer::Visitor::TypePlainPrimitiveToWord32(Node* node) {
  return Type::Integral32();
}

Type Typer::Visitor::TypePlainPrimitiveToFloat64(Node* node) {
  return Type::Number();
}

// static
Type Typer::Visitor::ReferenceEqualTyper(Type lhs, Type rhs, Typer* t) {
  if (lhs.IsHeapConstant() && rhs.Is(lhs)) {
    return t->singleton_true_;
  }
  return Type::Boolean();
}

Type Typer::Visitor::TypeReferenceEqual(Node* node) {
  return TypeBinaryOp(node, ReferenceEqualTyper);
}

// static
Type Typer::Visitor::SameValueTyper(Type lhs, Type rhs, Typer* t) {
  return t->operation_typer()->SameValue(lhs, rhs);
}

Type Typer::Visitor::TypeSameValue(Node* node) {
  return TypeBinaryOp(node, SameValueTyper);
}

Type Typer::Visitor::TypeStringEqual(Node* node) { return Type::Boolean(); }

Type Typer::Visitor::TypeStringLessThan(Node* node) { return Type::Boolean(); }

Type Typer::Visitor::TypeStringLessThanOrEqual(Node* node) {
  return Type::Boolean();
}

Type Typer::Visitor::StringFromSingleCharCodeTyper(Type type, Typer* t) {
  return Type::String();
}

Type Typer::Visitor::StringFromSingleCodePointTyper(Type type, Typer* t) {
  return Type::String();
}

Type Typer::Visitor::TypeStringToLowerCaseIntl(Node* node) {
  return Type::String();
}

Type Typer::Visitor::TypeStringToUpperCaseIntl(Node* node) {
  return Type::String();
}

Type Typer::Visitor::TypeStringCharCodeAt(Node* node) {
  return typer_->cache_->kUint16;
}

Type Typer::Visitor::TypeStringCodePointAt(Node* node) {
  return Type::Range(0.0, String::kMaxCodePoint, zone());
}

Type Typer::Visitor::TypeStringFromSingleCharCode(Node* node) {
  return TypeUnaryOp(node, StringFromSingleCharCodeTyper);
}

Type Typer::Visitor::TypeStringFromSingleCodePoint(Node* node) {
  return TypeUnaryOp(node, StringFromSingleCodePointTyper);
}

Type Typer::Visitor::TypeStringIndexOf(Node* node) {
  return Type::Range(-1.0, String::kMaxLength, zone());
}

Type Typer::Visitor::TypeStringLength(Node* node) {
  return typer_->cache_->kStringLengthType;
}

Type Typer::Visitor::TypeStringSubstring(Node* node) { return Type::String(); }

Type Typer::Visitor::TypePoisonIndex(Node* node) {
  return Type::Union(Operand(node, 0), typer_->cache_->kSingletonZero, zone());
}

Type Typer::Visitor::TypeCheckBounds(Node* node) {
  return typer_->operation_typer_.CheckBounds(Operand(node, 0),
                                              Operand(node, 1));
}

Type Typer::Visitor::TypeCheckHeapObject(Node* node) {
  Type type = Operand(node, 0);
  return type;
}

Type Typer::Visitor::TypeCheckIf(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeCheckInternalizedString(Node* node) {
  Type arg = Operand(node, 0);
  return Type::Intersect(arg, Type::InternalizedString(), zone());
}

Type Typer::Visitor::TypeCheckMaps(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeCompareMaps(Node* node) { return Type::Boolean(); }

Type Typer::Visitor::TypeCheckNumber(Node* node) {
  return typer_->operation_typer_.CheckNumber(Operand(node, 0));
}

Type Typer::Visitor::TypeCheckReceiver(Node* node) {
  Type arg = Operand(node, 0);
  return Type::Intersect(arg, Type::Receiver(), zone());
}

Type Typer::Visitor::TypeCheckReceiverOrNullOrUndefined(Node* node) {
  Type arg = Operand(node, 0);
  return Type::Intersect(arg, Type::ReceiverOrNullOrUndefined(), zone());
}

Type Typer::Visitor::TypeCheckSmi(Node* node) {
  Type arg = Operand(node, 0);
  return Type::Intersect(arg, Type::SignedSmall(), zone());
}

Type Typer::Visitor::TypeCheckString(Node* node) {
  Type arg = Operand(node, 0);
  return Type::Intersect(arg, Type::String(), zone());
}

Type Typer::Visitor::TypeCheckSymbol(Node* node) {
  Type arg = Operand(node, 0);
  return Type::Intersect(arg, Type::Symbol(), zone());
}

Type Typer::Visitor::TypeCheckFloat64Hole(Node* node) {
  return typer_->operation_typer_.CheckFloat64Hole(Operand(node, 0));
}

Type Typer::Visitor::TypeCheckNotTaggedHole(Node* node) {
  Type type = Operand(node, 0);
  type = Type::Intersect(type, Type::NonInternal(), zone());
  return type;
}

Type Typer::Visitor::TypeConvertReceiver(Node* node) {
  Type arg = Operand(node, 0);
  return typer_->operation_typer_.ConvertReceiver(arg);
}

Type Typer::Visitor::TypeConvertTaggedHoleToUndefined(Node* node) {
  Type type = Operand(node, 0);
  return typer_->operation_typer()->ConvertTaggedHoleToUndefined(type);
}

Type Typer::Visitor::TypeCheckEqualsInternalizedString(Node* node) {
  UNREACHABLE();
}

Type Typer::Visitor::TypeCheckEqualsSymbol(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeAllocate(Node* node) {
  return AllocateTypeOf(node->op());
}

Type Typer::Visitor::TypeAllocateRaw(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeLoadFieldByIndex(Node* node) {
  return Type::NonInternal();
}

Type Typer::Visitor::TypeLoadField(Node* node) {
  return FieldAccessOf(node->op()).type;
}

Type Typer::Visitor::TypeLoadElement(Node* node) {
  return ElementAccessOf(node->op()).type;
}

Type Typer::Visitor::TypeLoadTypedElement(Node* node) {
  switch (ExternalArrayTypeOf(node->op())) {
#define TYPED_ARRAY_CASE(ElemType, type, TYPE, ctype) \
  case kExternal##ElemType##Array:                    \
    return typer_->cache_->k##ElemType;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
}

Type Typer::Visitor::TypeLoadDataViewElement(Node* node) {
  switch (ExternalArrayTypeOf(node->op())) {
#define TYPED_ARRAY_CASE(ElemType, type, TYPE, ctype) \
  case kExternal##ElemType##Array:                    \
    return typer_->cache_->k##ElemType;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
}

Type Typer::Visitor::TypeStoreField(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeStoreElement(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeTransitionAndStoreElement(Node* node) {
  UNREACHABLE();
}

Type Typer::Visitor::TypeTransitionAndStoreNumberElement(Node* node) {
  UNREACHABLE();
}

Type Typer::Visitor::TypeTransitionAndStoreNonNumberElement(Node* node) {
  UNREACHABLE();
}

Type Typer::Visitor::TypeStoreSignedSmallElement(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeStoreTypedElement(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeStoreDataViewElement(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeObjectIsArrayBufferView(Node* node) {
  return TypeUnaryOp(node, ObjectIsArrayBufferView);
}

Type Typer::Visitor::TypeObjectIsBigInt(Node* node) {
  return TypeUnaryOp(node, ObjectIsBigInt);
}

Type Typer::Visitor::TypeObjectIsCallable(Node* node) {
  return TypeUnaryOp(node, ObjectIsCallable);
}

Type Typer::Visitor::TypeObjectIsConstructor(Node* node) {
  return TypeUnaryOp(node, ObjectIsConstructor);
}

Type Typer::Visitor::TypeObjectIsDetectableCallable(Node* node) {
  return TypeUnaryOp(node, ObjectIsDetectableCallable);
}

Type Typer::Visitor::TypeObjectIsMinusZero(Node* node) {
  return TypeUnaryOp(node, ObjectIsMinusZero);
}

Type Typer::Visitor::TypeNumberIsMinusZero(Node* node) {
  return TypeUnaryOp(node, NumberIsMinusZero);
}

Type Typer::Visitor::TypeNumberIsFloat64Hole(Node* node) {
  return Type::Boolean();
}

Type Typer::Visitor::TypeNumberIsFinite(Node* node) { return Type::Boolean(); }

Type Typer::Visitor::TypeObjectIsFiniteNumber(Node* node) {
  return Type::Boolean();
}

Type Typer::Visitor::TypeNumberIsInteger(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeObjectIsSafeInteger(Node* node) {
  return Type::Boolean();
}

Type Typer::Visitor::TypeNumberIsSafeInteger(Node* node) { UNREACHABLE(); }

Type Typer::Visitor::TypeObjectIsInteger(Node* node) { return Type::Boolean(); }

Type Typer::Visitor::TypeObjectIsNaN(Node* node) {
  return TypeUnaryOp(node, ObjectIsNaN);
}

Type Typer::Visitor::TypeNumberIsNaN(Node* node) {
  return TypeUnaryOp(node, NumberIsNaN);
}

Type Typer::Visitor::TypeObjectIsNonCallable(Node* node) {
  return TypeUnaryOp(node, ObjectIsNonCallable);
}

Type Typer::Visitor::TypeObjectIsNumber(Node* node) {
  return TypeUnaryOp(node, ObjectIsNumber);
}

Type Typer::Visitor::TypeObjectIsReceiver(Node* node) {
  return TypeUnaryOp(node, ObjectIsReceiver);
}

Type Typer::Visitor::TypeObjectIsSmi(Node* node) {
  return TypeUnaryOp(node, ObjectIsSmi);
}

Type Typer::Visitor::TypeObjectIsString(Node* node) {
  return TypeUnaryOp(node, ObjectIsString);
}

Type Typer::Visitor::TypeObjectIsSymbol(Node* node) {
  return TypeUnaryOp(node, ObjectIsSymbol);
}

Type Typer::Visitor::TypeObjectIsUndetectable(Node* node) {
  return TypeUnaryOp(node, ObjectIsUndetectable);
}

Type Typer::Visitor::TypeArgumentsLength(Node* node) {
  return TypeCache::Get()->kArgumentsLengthType;
}

Type Typer::Visitor::TypeArgumentsFrame(Node* node) {
  return Type::ExternalPointer();
}

Type Typer::Visitor::TypeNewDoubleElements(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeNewSmiOrObjectElements(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeNewArgumentsElements(Node* node) {
  return Type::OtherInternal();
}

Type Typer::Visitor::TypeNewConsString(Node* node) { return Type::String(); }

Type Typer::Visitor::TypeDelayedStringConstant(Node* node) {
  return Type::String();
}

Type Typer::Visitor::TypeFindOrderedHashMapEntry(Node* node) {
  return Type::Range(-1.0, FixedArray::kMaxLength, zone());
}

Type Typer::Visitor::TypeFindOrderedHashMapEntryForInt32Key(Node* node) {
  return Type::Range(-1.0, FixedArray::kMaxLength, zone());
}

Type Typer::Visitor::TypeRuntimeAbort(Node* node) { UNREACHABLE(); }

// Heap constants.

Type Typer::Visitor::TypeConstant(Handle<Object> value) {
  return Type::NewConstant(typer_->broker(), value, zone());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
