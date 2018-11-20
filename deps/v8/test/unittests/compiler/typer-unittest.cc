// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>

#include "src/codegen.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-properties.h"
#include "test/cctest/types-fuzz.h"
#include "test/unittests/compiler/graph-unittest.h"

using namespace v8::internal;
using namespace v8::internal::compiler;


// TODO(titzer): generate a large set of deterministic inputs for these tests.
class TyperTest : public TypedGraphTest {
 public:
  TyperTest()
      : TypedGraphTest(3),
        types_(zone(), isolate(), random_number_generator()),
        javascript_(zone()) {
    context_node_ = graph()->NewNode(common()->Parameter(2), graph()->start());
    rng_ = random_number_generator();

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
  JSOperatorBuilder javascript_;
  Node* context_node_;
  v8::base::RandomNumberGenerator* rng_;
  std::vector<double> integers;
  std::vector<double> int32s;

  Type* TypeBinaryOp(const Operator* op, Type* lhs, Type* rhs) {
    Node* p0 = Parameter(0);
    Node* p1 = Parameter(1);
    NodeProperties::SetBounds(p0, Bounds(lhs));
    NodeProperties::SetBounds(p1, Bounds(rhs));
    Node* n = graph()->NewNode(op, p0, p1, context_node_, graph()->start(),
                               graph()->start());
    return NodeProperties::GetBounds(n).upper;
  }

  Type* RandomRange(bool int32 = false) {
    std::vector<double>& numbers = int32 ? int32s : integers;
    double i = numbers[rng_->NextInt(static_cast<int>(numbers.size()))];
    double j = numbers[rng_->NextInt(static_cast<int>(numbers.size()))];
    return NewRange(i, j);
  }

  Type* NewRange(double i, double j) {
    if (i > j) std::swap(i, j);
    return Type::Range(i, j, zone());
  }

  double RandomInt(double min, double max) {
    switch (rng_->NextInt(4)) {
      case 0:
        return min;
      case 1:
        return max;
      default:
        break;
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
    return RandomInt(range->Min(), range->Max());
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
                  isolate()->factory()->NewNumber(result_value), zone());
              EXPECT_TRUE(result_type->Is(expected_type));
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
            isolate()->factory()->NewNumber(result_value), zone());
        EXPECT_TRUE(result_type->Is(expected_type));
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
                           zone());
        EXPECT_TRUE(result_type->Is(expected_type));
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
            isolate()->factory()->NewNumber(result_value), zone());
        EXPECT_TRUE(result_type->Is(expected_type));
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
      Type* subtype1 = RandomSubtype(type1);
      ;
      Type* subtype2 = RandomSubtype(type2);
      ;
      Type* subtype = TypeBinaryOp(op, subtype1, subtype2);
      EXPECT_TRUE(subtype->Is(type));
    }
  }
};


namespace {

int32_t shift_left(int32_t x, int32_t y) { return x << y; }
int32_t shift_right(int32_t x, int32_t y) { return x >> y; }
int32_t bit_or(int32_t x, int32_t y) { return x | y; }
int32_t bit_and(int32_t x, int32_t y) { return x & y; }
int32_t bit_xor(int32_t x, int32_t y) { return x ^ y; }

}  // namespace


//------------------------------------------------------------------------------
// Soundness
//   For simplicity, we currently only test soundness on expression operators
//   that have a direct equivalent in C++.  Also, testing is currently limited
//   to ranges as input types.


TEST_F(TyperTest, TypeJSAdd) {
  TestBinaryArithOp(javascript_.Add(), std::plus<double>());
}


TEST_F(TyperTest, TypeJSSubtract) {
  TestBinaryArithOp(javascript_.Subtract(), std::minus<double>());
}


TEST_F(TyperTest, TypeJSMultiply) {
  TestBinaryArithOp(javascript_.Multiply(), std::multiplies<double>());
}


TEST_F(TyperTest, TypeJSDivide) {
  TestBinaryArithOp(javascript_.Divide(), std::divides<double>());
}


TEST_F(TyperTest, TypeJSModulus) {
  TestBinaryArithOp(javascript_.Modulus(), modulo);
}


TEST_F(TyperTest, TypeJSBitwiseOr) {
  TestBinaryBitOp(javascript_.BitwiseOr(), bit_or);
}


TEST_F(TyperTest, TypeJSBitwiseAnd) {
  TestBinaryBitOp(javascript_.BitwiseAnd(), bit_and);
}


TEST_F(TyperTest, TypeJSBitwiseXor) {
  TestBinaryBitOp(javascript_.BitwiseXor(), bit_xor);
}


TEST_F(TyperTest, TypeJSShiftLeft) {
  TestBinaryBitOp(javascript_.ShiftLeft(), shift_left);
}


TEST_F(TyperTest, TypeJSShiftRight) {
  TestBinaryBitOp(javascript_.ShiftRight(), shift_right);
}


TEST_F(TyperTest, TypeJSLessThan) {
  TestBinaryCompareOp(javascript_.LessThan(), std::less<double>());
}


TEST_F(TyperTest, TypeJSLessThanOrEqual) {
  TestBinaryCompareOp(javascript_.LessThanOrEqual(), std::less_equal<double>());
}


TEST_F(TyperTest, TypeJSGreaterThan) {
  TestBinaryCompareOp(javascript_.GreaterThan(), std::greater<double>());
}


TEST_F(TyperTest, TypeJSGreaterThanOrEqual) {
  TestBinaryCompareOp(javascript_.GreaterThanOrEqual(),
                      std::greater_equal<double>());
}


TEST_F(TyperTest, TypeJSEqual) {
  TestBinaryCompareOp(javascript_.Equal(), std::equal_to<double>());
}


TEST_F(TyperTest, TypeJSNotEqual) {
  TestBinaryCompareOp(javascript_.NotEqual(), std::not_equal_to<double>());
}


// For numbers there's no difference between strict and non-strict equality.
TEST_F(TyperTest, TypeJSStrictEqual) {
  TestBinaryCompareOp(javascript_.StrictEqual(), std::equal_to<double>());
}


TEST_F(TyperTest, TypeJSStrictNotEqual) {
  TestBinaryCompareOp(javascript_.StrictNotEqual(),
                      std::not_equal_to<double>());
}


//------------------------------------------------------------------------------
// Monotonicity


// List should be in sync with JS_SIMPLE_BINOP_LIST.
#define JSBINOP_LIST(V) \
  V(Equal)              \
  V(NotEqual)           \
  V(StrictEqual)        \
  V(StrictNotEqual)     \
  V(LessThan)           \
  V(GreaterThan)        \
  V(LessThanOrEqual)    \
  V(GreaterThanOrEqual) \
  V(BitwiseOr)          \
  V(BitwiseXor)         \
  V(BitwiseAnd)         \
  V(ShiftLeft)          \
  V(ShiftRight)         \
  V(ShiftRightLogical)  \
  V(Add)                \
  V(Subtract)           \
  V(Multiply)           \
  V(Divide)             \
  V(Modulus)


#define TEST_FUNC(name)                         \
  TEST_F(TyperTest, Monotonicity_##name) {      \
    TestBinaryMonotonicity(javascript_.name()); \
  }
JSBINOP_LIST(TEST_FUNC)
#undef TEST_FUNC


//------------------------------------------------------------------------------
// Regression tests


TEST_F(TyperTest, TypeRegressInt32Constant) {
  int values[] = {-5, 10};
  for (auto i : values) {
    Node* c = graph()->NewNode(common()->Int32Constant(i));
    Type* type = NodeProperties::GetBounds(c).upper;
    EXPECT_TRUE(type->Is(NewRange(i, i)));
  }
}
