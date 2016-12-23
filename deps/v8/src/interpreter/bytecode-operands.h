// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_OPERANDS_H_
#define V8_INTERPRETER_BYTECODE_OPERANDS_H_

#include "src/globals.h"

namespace v8 {
namespace internal {
namespace interpreter {

#define INVALID_OPERAND_TYPE_LIST(V) V(None, OperandTypeInfo::kNone)

#define REGISTER_INPUT_OPERAND_TYPE_LIST(V)        \
  V(RegList, OperandTypeInfo::kScalableSignedByte) \
  V(Reg, OperandTypeInfo::kScalableSignedByte)     \
  V(RegPair, OperandTypeInfo::kScalableSignedByte)

#define REGISTER_OUTPUT_OPERAND_TYPE_LIST(V)          \
  V(RegOut, OperandTypeInfo::kScalableSignedByte)     \
  V(RegOutPair, OperandTypeInfo::kScalableSignedByte) \
  V(RegOutTriple, OperandTypeInfo::kScalableSignedByte)

#define SCALAR_OPERAND_TYPE_LIST(V)                   \
  V(Flag8, OperandTypeInfo::kFixedUnsignedByte)       \
  V(IntrinsicId, OperandTypeInfo::kFixedUnsignedByte) \
  V(Idx, OperandTypeInfo::kScalableUnsignedByte)      \
  V(UImm, OperandTypeInfo::kScalableUnsignedByte)     \
  V(Imm, OperandTypeInfo::kScalableSignedByte)        \
  V(RegCount, OperandTypeInfo::kScalableUnsignedByte) \
  V(RuntimeId, OperandTypeInfo::kFixedUnsignedShort)

#define REGISTER_OPERAND_TYPE_LIST(V) \
  REGISTER_INPUT_OPERAND_TYPE_LIST(V) \
  REGISTER_OUTPUT_OPERAND_TYPE_LIST(V)

#define NON_REGISTER_OPERAND_TYPE_LIST(V) \
  INVALID_OPERAND_TYPE_LIST(V)            \
  SCALAR_OPERAND_TYPE_LIST(V)

// The list of operand types used by bytecodes.
#define OPERAND_TYPE_LIST(V)        \
  NON_REGISTER_OPERAND_TYPE_LIST(V) \
  REGISTER_OPERAND_TYPE_LIST(V)

// Enumeration of scaling factors applicable to scalable operands. Code
// relies on being able to cast values to integer scaling values.
#define OPERAND_SCALE_LIST(V) \
  V(Single, 1)                \
  V(Double, 2)                \
  V(Quadruple, 4)

enum class OperandScale : uint8_t {
#define DECLARE_OPERAND_SCALE(Name, Scale) k##Name = Scale,
  OPERAND_SCALE_LIST(DECLARE_OPERAND_SCALE)
#undef DECLARE_OPERAND_SCALE
      kLast = kQuadruple
};

// Enumeration of the size classes of operand types used by
// bytecodes. Code relies on being able to cast values to integer
// types to get the size in bytes.
enum class OperandSize : uint8_t {
  kNone = 0,
  kByte = 1,
  kShort = 2,
  kQuad = 4,
  kLast = kQuad
};

// Primitive operand info used that summarize properties of operands.
// Columns are Name, IsScalable, IsUnsigned, UnscaledSize.
#define OPERAND_TYPE_INFO_LIST(V)                         \
  V(None, false, false, OperandSize::kNone)               \
  V(ScalableSignedByte, true, false, OperandSize::kByte)  \
  V(ScalableUnsignedByte, true, true, OperandSize::kByte) \
  V(FixedUnsignedByte, false, true, OperandSize::kByte)   \
  V(FixedUnsignedShort, false, true, OperandSize::kShort)

enum class OperandTypeInfo : uint8_t {
#define DECLARE_OPERAND_TYPE_INFO(Name, ...) k##Name,
  OPERAND_TYPE_INFO_LIST(DECLARE_OPERAND_TYPE_INFO)
#undef DECLARE_OPERAND_TYPE_INFO
};

// Enumeration of operand types used by bytecodes.
enum class OperandType : uint8_t {
#define DECLARE_OPERAND_TYPE(Name, _) k##Name,
  OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE)
#undef DECLARE_OPERAND_TYPE
#define COUNT_OPERAND_TYPES(x, _) +1
  // The COUNT_OPERAND macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last operand.
  kLast = -1 OPERAND_TYPE_LIST(COUNT_OPERAND_TYPES)
#undef COUNT_OPERAND_TYPES
};

enum class AccumulatorUse : uint8_t {
  kNone = 0,
  kRead = 1 << 0,
  kWrite = 1 << 1,
  kReadWrite = kRead | kWrite
};

inline AccumulatorUse operator&(AccumulatorUse lhs, AccumulatorUse rhs) {
  int result = static_cast<int>(lhs) & static_cast<int>(rhs);
  return static_cast<AccumulatorUse>(result);
}

inline AccumulatorUse operator|(AccumulatorUse lhs, AccumulatorUse rhs) {
  int result = static_cast<int>(lhs) | static_cast<int>(rhs);
  return static_cast<AccumulatorUse>(result);
}

std::ostream& operator<<(std::ostream& os, const AccumulatorUse& use);
std::ostream& operator<<(std::ostream& os, const OperandScale& operand_scale);
std::ostream& operator<<(std::ostream& os, const OperandSize& operand_size);
std::ostream& operator<<(std::ostream& os, const OperandType& operand_type);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_OPERANDS_H_
