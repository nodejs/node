// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_OPERATION_H_
#define V8_COMMON_OPERATION_H_

#include <ostream>

#define ARITHMETIC_OPERATION_LIST(V) \
  V(Add)                             \
  V(Subtract)                        \
  V(Multiply)                        \
  V(Divide)                          \
  V(Modulus)                         \
  V(Exponentiate)                    \
  V(BitwiseAnd)                      \
  V(BitwiseOr)                       \
  V(BitwiseXor)                      \
  V(ShiftLeft)                       \
  V(ShiftRight)                      \
  V(ShiftRightLogical)

#define UNARY_OPERATION_LIST(V) \
  V(BitwiseNot)                 \
  V(Negate)                     \
  V(Increment)                  \
  V(Decrement)

#define COMPARISON_OPERATION_LIST(V) \
  V(Equal)                           \
  V(StrictEqual)                     \
  V(LessThan)                        \
  V(LessThanOrEqual)                 \
  V(GreaterThan)                     \
  V(GreaterThanOrEqual)

#define OPERATION_LIST(V)      \
  ARITHMETIC_OPERATION_LIST(V) \
  UNARY_OPERATION_LIST(V)      \
  COMPARISON_OPERATION_LIST(V)

enum class Operation {
#define DEFINE_OP(name) k##name,
  OPERATION_LIST(DEFINE_OP)
#undef DEFINE_OP
};

inline std::ostream& operator<<(std::ostream& os, const Operation& operation) {
  switch (operation) {
#define CASE(name)         \
  case Operation::k##name: \
    return os << #name;
    OPERATION_LIST(CASE)
#undef CASE
  }
}

#endif  // V8_COMMON_OPERATION_H_
