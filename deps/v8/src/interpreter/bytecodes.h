// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODES_H_
#define V8_INTERPRETER_BYTECODES_H_

#include <iosfwd>

// Clients of this interface shouldn't depend on lots of interpreter internals.
// Do not include anything from src/interpreter here!
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

// The list of operand types used by bytecodes.
#define OPERAND_TYPE_LIST(V) \
  V(None)                    \
  V(Imm8)                    \
  V(Reg)

// The list of bytecodes which are interpreted by the interpreter.
#define BYTECODE_LIST(V)               \
                                       \
  /* Loading the accumulator */        \
  V(LdaZero, OperandType::kNone)       \
  V(LdaSmi8, OperandType::kImm8)       \
  V(LdaUndefined, OperandType::kNone)  \
  V(LdaNull, OperandType::kNone)       \
  V(LdaTheHole, OperandType::kNone)    \
  V(LdaTrue, OperandType::kNone)       \
  V(LdaFalse, OperandType::kNone)      \
                                       \
  /* Register-accumulator transfers */ \
  V(Ldar, OperandType::kReg)           \
  V(Star, OperandType::kReg)           \
                                       \
  /* Binary Operators */               \
  V(Add, OperandType::kReg)            \
  V(Sub, OperandType::kReg)            \
  V(Mul, OperandType::kReg)            \
  V(Div, OperandType::kReg)            \
                                       \
  /* Control Flow */                   \
  V(Return, OperandType::kNone)


// Enumeration of operand types used by bytecodes.
enum class OperandType : uint8_t {
#define DECLARE_OPERAND_TYPE(Name) k##Name,
  OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE)
#undef DECLARE_OPERAND_TYPE
#define COUNT_OPERAND_TYPES(x) +1
  // The COUNT_OPERAND macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last operand.
  kLast = -1 OPERAND_TYPE_LIST(COUNT_OPERAND_TYPES)
#undef COUNT_OPERAND_TYPES
};


// Enumeration of interpreter bytecodes.
enum class Bytecode : uint8_t {
#define DECLARE_BYTECODE(Name, ...) k##Name,
  BYTECODE_LIST(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
#define COUNT_BYTECODE(x, ...) +1
  // The COUNT_BYTECODE macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last real bytecode.
  kLast = -1 BYTECODE_LIST(COUNT_BYTECODE)
#undef COUNT_BYTECODE
};


class Bytecodes {
 public:
  // Returns string representation of |bytecode|.
  static const char* ToString(Bytecode bytecode);

  // Returns string representation of |operand_type|.
  static const char* OperandTypeToString(OperandType operand_type);

  // Returns byte value of bytecode.
  static uint8_t ToByte(Bytecode bytecode);

  // Returns bytecode for |value|.
  static Bytecode FromByte(uint8_t value);

  // Returns the number of operands expected by |bytecode|.
  static int NumberOfOperands(Bytecode bytecode);

  // Return the i-th operand of |bytecode|.
  static OperandType GetOperandType(Bytecode bytecode, int i);

  // Returns the size of the bytecode including its operands.
  static int Size(Bytecode bytecode);

  // The maximum number of operands across all bytecodes.
  static int MaximumNumberOfOperands();

  // Maximum size of a bytecode and its operands.
  static int MaximumSize();

  // Decode a single bytecode and operands to |os|.
  static std::ostream& Decode(std::ostream& os, const uint8_t* bytecode_start);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Bytecodes);
};

std::ostream& operator<<(std::ostream& os, const Bytecode& bytecode);
std::ostream& operator<<(std::ostream& os, const OperandType& operand_type);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODES_H_
