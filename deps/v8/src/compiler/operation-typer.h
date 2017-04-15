// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATION_TYPER_H_
#define V8_COMPILER_OPERATION_TYPER_H_

#include "src/base/flags.h"
#include "src/compiler/opcodes.h"

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

class OperationTyper {
 public:
  OperationTyper(Isolate* isolate, Zone* zone);

  // Typing Phi.
  Type* Merge(Type* left, Type* right);

  Type* ToPrimitive(Type* type);

  // Helpers for number operation typing.
  Type* ToNumber(Type* type);
  Type* WeakenRange(Type* current_range, Type* previous_range);

// Number unary operators.
#define DECLARE_METHOD(Name) Type* Name(Type* type);
  SIMPLIFIED_NUMBER_UNOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

// Number binary operators.
#define DECLARE_METHOD(Name) Type* Name(Type* lhs, Type* rhs);
  SIMPLIFIED_NUMBER_BINOP_LIST(DECLARE_METHOD)
  SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(DECLARE_METHOD)
#undef DECLARE_METHOD

  Type* TypeTypeGuard(const Operator* sigma_op, Type* input);

  enum ComparisonOutcomeFlags {
    kComparisonTrue = 1,
    kComparisonFalse = 2,
    kComparisonUndefined = 4
  };

  Type* singleton_false() const { return singleton_false_; }
  Type* singleton_true() const { return singleton_true_; }
  Type* singleton_the_hole() const { return singleton_the_hole_; }

 private:
  typedef base::Flags<ComparisonOutcomeFlags> ComparisonOutcome;

  ComparisonOutcome Invert(ComparisonOutcome);
  Type* Invert(Type*);
  Type* FalsifyUndefined(ComparisonOutcome);

  Type* Rangify(Type*);
  Type* AddRanger(double lhs_min, double lhs_max, double rhs_min,
                  double rhs_max);
  Type* SubtractRanger(double lhs_min, double lhs_max, double rhs_min,
                       double rhs_max);
  Type* MultiplyRanger(Type* lhs, Type* rhs);

  Zone* zone() const { return zone_; }

  Zone* const zone_;
  TypeCache const& cache_;

  Type* infinity_;
  Type* minus_infinity_;
  Type* singleton_false_;
  Type* singleton_true_;
  Type* singleton_the_hole_;
  Type* signed32ish_;
  Type* unsigned32ish_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPERATION_TYPER_H_
