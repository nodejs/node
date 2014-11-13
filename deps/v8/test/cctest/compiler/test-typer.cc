// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "src/codegen.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/typer.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/types-fuzz.h"

using namespace v8::internal;
using namespace v8::internal::compiler;


// TODO(titzer): generate a large set of deterministic inputs for these tests.
class TyperTester : public HandleAndZoneScope, public GraphAndBuilders {
 public:
  TyperTester()
      : GraphAndBuilders(main_zone()),
        types_(main_zone(), isolate()),
        typer_(graph(), MaybeHandle<Context>()),
        javascript_(main_zone()) {
    Node* s = graph()->NewNode(common()->Start(3));
    graph()->SetStart(s);
    context_node_ = graph()->NewNode(common()->Parameter(2), graph()->start());
    rng_ = isolate()->random_number_generator();

    integers.push_back(0);
    integers.push_back(0);
    integers.push_back(-1);
    integers.push_back(+1);
    integers.push_back(-V8_INFINITY);
    integers.push_back(+V8_INFINITY);
    for (int i = 0; i < 5; ++i) {
      double x = rng_->NextInt();
      integers.push_back(x);
      x *= rng_->NextInt();
      if (!IsMinusZero(x)) integers.push_back(x);
    }

    int32s.push_back(0);
    int32s.push_back(0);
    int32s.push_back(-1);
    int32s.push_back(+1);
    int32s.push_back(kMinInt);
    int32s.push_back(kMaxInt);
    for (int i = 0; i < 10; ++i) {
      int32s.push_back(rng_->NextInt());
    }
  }

  Types<Type, Type*, Zone> types_;
  Typer typer_;
  JSOperatorBuilder javascript_;
  Node* context_node_;
  v8::base::RandomNumberGenerator* rng_;
  std::vector<double> integers;
  std::vector<double> int32s;

  Isolate* isolate() { return main_isolate(); }
  Graph* graph() { return main_graph_; }
  CommonOperatorBuilder* common() { return &main_common_; }

  Node* Parameter(int index = 0) {
    return graph()->NewNode(common()->Parameter(index), graph()->start());
  }

  Type* TypeBinaryOp(const Operator* op, Type* lhs, Type* rhs) {
    Node* p0 = Parameter(0);
    Node* p1 = Parameter(1);
    NodeProperties::SetBounds(p0, Bounds(lhs));
    NodeProperties::SetBounds(p1, Bounds(rhs));
    Node* n = graph()->NewNode(
        op, p0, p1, context_node_, graph()->start(), graph()->start());
    return NodeProperties::GetBounds(n).upper;
  }

  Type* RandomRange(bool int32 = false) {
    std::vector<double>& numbers = int32 ? int32s : integers;
    double i = numbers[rng_->NextInt(static_cast<int>(numbers.size()))];
    double j = numbers[rng_->NextInt(static_cast<int>(numbers.size()))];
    return NewRange(i, j);
  }

  Type* NewRange(double i, double j) {
    Factory* f = isolate()->factory();
    i::Handle<i::Object> min = f->NewNumber(i);
    i::Handle<i::Object> max = f->NewNumber(j);
    if (min->Number() > max->Number()) std::swap(min, max);
    return Type::Range(min, max, main_zone());
  }

  double RandomInt(double min, double max) {
    switch (rng_->NextInt(4)) {
      case 0: return min;
      case 1: return max;
      default: break;
    }
    if (min == +V8_INFINITY) return +V8_INFINITY;
    if (max == -V8_INFINITY) return -V8_INFINITY;
    if (min == -V8_INFINITY && max == +V8_INFINITY) {
      return rng_->NextInt() * static_cast<double>(rng_->NextInt());
    }
    double result = nearbyint(min + (max - min) * rng_->NextDouble());
    if (IsMinusZero(result)) return 0;
    if (std::isnan(result)) return rng_->NextInt(2) ? min : max;
    DCHECK(min <= result && result <= max);
    return result;
  }

  double RandomInt(Type::RangeType* range) {
    return RandomInt(range->Min()->Number(), range->Max()->Number());
  }

  // Careful, this function runs O(max_width^5) trials.
  template <class BinaryFunction>
  void TestBinaryArithOpCloseToZero(const Operator* op, BinaryFunction opfun,
                                    int max_width) {
    const int min_min = -2 - max_width / 2;
    const int max_min = 2 + max_width / 2;
    for (int width = 0; width < max_width; width++) {
      for (int lmin = min_min; lmin <= max_min; lmin++) {
        for (int rmin = min_min; rmin <= max_min; rmin++) {
          Type* r1 = NewRange(lmin, lmin + width);
          Type* r2 = NewRange(rmin, rmin + width);
          Type* expected_type = TypeBinaryOp(op, r1, r2);

          for (int x1 = lmin; x1 < lmin + width; x1++) {
            for (int x2 = rmin; x2 < rmin + width; x2++) {
              double result_value = opfun(x1, x2);
              Type* result_type = Type::Constant(
                  isolate()->factory()->NewNumber(result_value), main_zone());
              CHECK(result_type->Is(expected_type));
            }
          }
        }
      }
    }
  }

  template <class BinaryFunction>
  void TestBinaryArithOp(const Operator* op, BinaryFunction opfun) {
    TestBinaryArithOpCloseToZero(op, opfun, 8);
    for (int i = 0; i < 100; ++i) {
      Type::RangeType* r1 = RandomRange()->AsRange();
      Type::RangeType* r2 = RandomRange()->AsRange();
      Type* expected_type = TypeBinaryOp(op, r1, r2);
      for (int i = 0; i < 10; i++) {
        double x1 = RandomInt(r1);
        double x2 = RandomInt(r2);
        double result_value = opfun(x1, x2);
        Type* result_type = Type::Constant(
            isolate()->factory()->NewNumber(result_value), main_zone());
        CHECK(result_type->Is(expected_type));
      }
    }
  }

  template <class BinaryFunction>
  void TestBinaryCompareOp(const Operator* op, BinaryFunction opfun) {
    for (int i = 0; i < 100; ++i) {
      Type::RangeType* r1 = RandomRange()->AsRange();
      Type::RangeType* r2 = RandomRange()->AsRange();
      Type* expected_type = TypeBinaryOp(op, r1, r2);
      for (int i = 0; i < 10; i++) {
        double x1 = RandomInt(r1);
        double x2 = RandomInt(r2);
        bool result_value = opfun(x1, x2);
        Type* result_type =
            Type::Constant(result_value ? isolate()->factory()->true_value()
                                        : isolate()->factory()->false_value(),
                           main_zone());
        CHECK(result_type->Is(expected_type));
      }
    }
  }

  template <class BinaryFunction>
  void TestBinaryBitOp(const Operator* op, BinaryFunction opfun) {
    for (int i = 0; i < 100; ++i) {
      Type::RangeType* r1 = RandomRange(true)->AsRange();
      Type::RangeType* r2 = RandomRange(true)->AsRange();
      Type* expected_type = TypeBinaryOp(op, r1, r2);
      for (int i = 0; i < 10; i++) {
        int32_t x1 = static_cast<int32_t>(RandomInt(r1));
        int32_t x2 = static_cast<int32_t>(RandomInt(r2));
        double result_value = opfun(x1, x2);
        Type* result_type = Type::Constant(
            isolate()->factory()->NewNumber(result_value), main_zone());
        CHECK(result_type->Is(expected_type));
      }
    }
  }

  Type* RandomSubtype(Type* type) {
    Type* subtype;
    do {
      subtype = types_.Fuzz();
    } while (!subtype->Is(type));
    return subtype;
  }

  void TestBinaryMonotonicity(const Operator* op) {
    for (int i = 0; i < 50; ++i) {
      Type* type1 = types_.Fuzz();
      Type* type2 = types_.Fuzz();
      Type* type = TypeBinaryOp(op, type1, type2);
      Type* subtype1 = RandomSubtype(type1);;
      Type* subtype2 = RandomSubtype(type2);;
      Type* subtype = TypeBinaryOp(op, subtype1, subtype2);
      CHECK(subtype->Is(type));
    }
  }
};


static int32_t shift_left(int32_t x, int32_t y) { return x << y; }
static int32_t shift_right(int32_t x, int32_t y) { return x >> y; }
static int32_t bit_or(int32_t x, int32_t y) { return x | y; }
static int32_t bit_and(int32_t x, int32_t y) { return x & y; }
static int32_t bit_xor(int32_t x, int32_t y) { return x ^ y; }


//------------------------------------------------------------------------------
// Soundness
//   For simplicity, we currently only test soundness on expression operators
//   that have a direct equivalent in C++.  Also, testing is currently limited
//   to ranges as input types.


TEST(TypeJSAdd) {
  TyperTester t;
  t.TestBinaryArithOp(t.javascript_.Add(), std::plus<double>());
}


TEST(TypeJSSubtract) {
  TyperTester t;
  t.TestBinaryArithOp(t.javascript_.Subtract(), std::minus<double>());
}


TEST(TypeJSMultiply) {
  TyperTester t;
  t.TestBinaryArithOp(t.javascript_.Multiply(), std::multiplies<double>());
}


TEST(TypeJSDivide) {
  TyperTester t;
  t.TestBinaryArithOp(t.javascript_.Divide(), std::divides<double>());
}


TEST(TypeJSModulus) {
  TyperTester t;
  t.TestBinaryArithOp(t.javascript_.Modulus(), modulo);
}


TEST(TypeJSBitwiseOr) {
  TyperTester t;
  t.TestBinaryBitOp(t.javascript_.BitwiseOr(), bit_or);
}


TEST(TypeJSBitwiseAnd) {
  TyperTester t;
  t.TestBinaryBitOp(t.javascript_.BitwiseAnd(), bit_and);
}


TEST(TypeJSBitwiseXor) {
  TyperTester t;
  t.TestBinaryBitOp(t.javascript_.BitwiseXor(), bit_xor);
}


TEST(TypeJSShiftLeft) {
  TyperTester t;
  t.TestBinaryBitOp(t.javascript_.ShiftLeft(), shift_left);
}


TEST(TypeJSShiftRight) {
  TyperTester t;
  t.TestBinaryBitOp(t.javascript_.ShiftRight(), shift_right);
}


TEST(TypeJSLessThan) {
  TyperTester t;
  t.TestBinaryCompareOp(t.javascript_.LessThan(), std::less<double>());
}


TEST(TypeJSLessThanOrEqual) {
  TyperTester t;
  t.TestBinaryCompareOp(
      t.javascript_.LessThanOrEqual(), std::less_equal<double>());
}


TEST(TypeJSGreaterThan) {
  TyperTester t;
  t.TestBinaryCompareOp(t.javascript_.GreaterThan(), std::greater<double>());
}


TEST(TypeJSGreaterThanOrEqual) {
  TyperTester t;
  t.TestBinaryCompareOp(
      t.javascript_.GreaterThanOrEqual(), std::greater_equal<double>());
}


TEST(TypeJSEqual) {
  TyperTester t;
  t.TestBinaryCompareOp(t.javascript_.Equal(), std::equal_to<double>());
}


TEST(TypeJSNotEqual) {
  TyperTester t;
  t.TestBinaryCompareOp(t.javascript_.NotEqual(), std::not_equal_to<double>());
}


// For numbers there's no difference between strict and non-strict equality.
TEST(TypeJSStrictEqual) {
  TyperTester t;
  t.TestBinaryCompareOp(t.javascript_.StrictEqual(), std::equal_to<double>());
}


TEST(TypeJSStrictNotEqual) {
  TyperTester t;
  t.TestBinaryCompareOp(
      t.javascript_.StrictNotEqual(), std::not_equal_to<double>());
}


//------------------------------------------------------------------------------
// Monotonicity


// List should be in sync with JS_SIMPLE_BINOP_LIST.
#define JSBINOP_LIST(V) \
  V(Equal) \
  V(NotEqual) \
  V(StrictEqual) \
  V(StrictNotEqual) \
  V(LessThan) \
  V(GreaterThan) \
  V(LessThanOrEqual) \
  V(GreaterThanOrEqual) \
  V(BitwiseOr) \
  V(BitwiseXor) \
  V(BitwiseAnd) \
  V(ShiftLeft) \
  V(ShiftRight) \
  V(ShiftRightLogical) \
  V(Add) \
  V(Subtract) \
  V(Multiply) \
  V(Divide) \
  V(Modulus)


#define TEST_FUNC(name)                             \
  TEST(Monotonicity_##name) {                       \
    TyperTester t;                                  \
    t.TestBinaryMonotonicity(t.javascript_.name()); \
  }
JSBINOP_LIST(TEST_FUNC)
#undef TEST_FUNC
