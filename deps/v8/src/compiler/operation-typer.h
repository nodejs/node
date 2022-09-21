// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATION_TYPER_H_
#define V8_COMPILER_OPERATION_TYPER_H_

#include "src/base/flags.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/types.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;
class RangeType;
class Zone;

namespace compiler {

// Forward declarations.
class Operator;
class Type;
class TypeCache;

class V8_EXPORT_PRIVATE OperationTyper {
 public:
  OperationTyper(JSHeapBroker* broker, Zone* zone);

  // Typing Phi.
  Type Merge(Type left, Type right);

  Type ToPrimitive(Type type);
  Type ToNumber(Type type);
  Type ToNumberConvertBigInt(Type type);
  Type ToNumeric(Type type);
  Type ToBoolean(Type type);

  Type WeakenRange(Type current_range, Type previous_range);

// Unary operators.
#define DECLARE_METHOD(Name) Type Name(Type type);
  SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_BIGINT_UNOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_NUMBER_UNOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_BIGINT_UNOP_LIST(DECLARE_METHOD)
  DECLARE_METHOD(ConvertReceiver)
#undef DECLARE_METHOD

// Numeric binary operators.
#define DECLARE_METHOD(Name) Type Name(Type lhs, Type rhs);
  SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_BIGINT_BINOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_BIGINT_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  // Comparison operators.
  Type SameValue(Type lhs, Type rhs);
  Type SameValueNumbersOnly(Type lhs, Type rhs);
  Type StrictEqual(Type lhs, Type rhs);

  // Check operators.
  Type CheckBounds(Type index, Type length);
  Type CheckFloat64Hole(Type type);
  Type CheckNumber(Type type);
  Type ConvertTaggedHoleToUndefined(Type type);

  Type TypeTypeGuard(const Operator* sigma_op, Type input);

  enum ComparisonOutcomeFlags {
    kComparisonTrue = 1,
    kComparisonFalse = 2,
    kComparisonUndefined = 4
  };

  Type singleton_false() const { return singleton_false_; }
  Type singleton_true() const { return singleton_true_; }
  Type singleton_the_hole() const { return singleton_the_hole_; }

 private:
  using ComparisonOutcome = base::Flags<ComparisonOutcomeFlags>;

  ComparisonOutcome Invert(ComparisonOutcome);
  Type Invert(Type);
  Type FalsifyUndefined(ComparisonOutcome);

  Type Rangify(Type);
  Type AddRanger(double lhs_min, double lhs_max, double rhs_min,
                 double rhs_max);
  Type SubtractRanger(double lhs_min, double lhs_max, double rhs_min,
                      double rhs_max);
  Type MultiplyRanger(double lhs_min, double lhs_max, double rhs_min,
                      double rhs_max);

  Zone* zone() const { return zone_; }

  Zone* const zone_;
  TypeCache const* cache_;

  Type infinity_;
  Type minus_infinity_;
  Type singleton_NaN_string_;
  Type singleton_zero_string_;
  Type singleton_false_;
  Type singleton_true_;
  Type singleton_the_hole_;
  Type signed32ish_;
  Type unsigned32ish_;
  Type singleton_empty_string_;
  Type truish_;
  Type falsish_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPERATION_TYPER_H_
