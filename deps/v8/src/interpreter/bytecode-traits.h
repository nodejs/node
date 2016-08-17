// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_TRAITS_H_
#define V8_INTERPRETER_BYTECODE_TRAITS_H_

#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

template <OperandTypeInfo>
struct OperandTypeInfoTraits {
  static const bool kIsScalable = false;
  static const bool kIsUnsigned = false;
  static const OperandSize kUnscaledSize = OperandSize::kNone;
};

#define DECLARE_OPERAND_TYPE_INFO(Name, Scalable, Unsigned, BaseSize) \
  template <>                                                         \
  struct OperandTypeInfoTraits<OperandTypeInfo::k##Name> {            \
    static const bool kIsScalable = Scalable;                         \
    static const bool kIsUnsigned = Unsigned;                         \
    static const OperandSize kUnscaledSize = BaseSize;                \
  };
OPERAND_TYPE_INFO_LIST(DECLARE_OPERAND_TYPE_INFO)
#undef DECLARE_OPERAND_TYPE_INFO

template <OperandType>
struct OperandTraits {
  typedef OperandTypeInfoTraits<OperandTypeInfo::kNone> TypeInfo;
};

#define DECLARE_OPERAND_TYPE_TRAITS(Name, InfoType)   \
  template <>                                         \
  struct OperandTraits<OperandType::k##Name> {        \
    typedef OperandTypeInfoTraits<InfoType> TypeInfo; \
  };
OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE_TRAITS)
#undef DECLARE_OPERAND_TYPE_TRAITS

template <OperandType>
struct RegisterOperandTraits {
  static const int kIsRegisterOperand = 0;
};

#define DECLARE_REGISTER_OPERAND(Name, _)              \
  template <>                                          \
  struct RegisterOperandTraits<OperandType::k##Name> { \
    static const int kIsRegisterOperand = 1;           \
  };
REGISTER_OPERAND_TYPE_LIST(DECLARE_REGISTER_OPERAND)
#undef DECLARE_REGISTER_OPERAND

template <AccumulatorUse, OperandType...>
struct BytecodeTraits {};

template <AccumulatorUse accumulator_use, OperandType operand_0,
          OperandType operand_1, OperandType operand_2, OperandType operand_3>
struct BytecodeTraits<accumulator_use, operand_0, operand_1, operand_2,
                      operand_3> {
  static OperandType GetOperandType(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandType kOperands[] = {operand_0, operand_1, operand_2,
                                     operand_3};
    return kOperands[i];
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot || operand_2 == ot ||
           operand_3 == ot;
  }

  static inline bool IsScalable() {
    return (OperandTraits<operand_0>::TypeInfo::kIsScalable |
            OperandTraits<operand_1>::TypeInfo::kIsScalable |
            OperandTraits<operand_2>::TypeInfo::kIsScalable |
            OperandTraits<operand_3>::TypeInfo::kIsScalable);
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 4;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand +
      RegisterOperandTraits<operand_2>::kIsRegisterOperand +
      RegisterOperandTraits<operand_3>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      (RegisterOperandTraits<operand_1>::kIsRegisterOperand << 1) +
      (RegisterOperandTraits<operand_2>::kIsRegisterOperand << 2) +
      (RegisterOperandTraits<operand_3>::kIsRegisterOperand << 3);
};

template <AccumulatorUse accumulator_use, OperandType operand_0,
          OperandType operand_1, OperandType operand_2>
struct BytecodeTraits<accumulator_use, operand_0, operand_1, operand_2> {
  static inline OperandType GetOperandType(int i) {
    DCHECK(0 <= i && i <= 2);
    const OperandType kOperands[] = {operand_0, operand_1, operand_2};
    return kOperands[i];
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot || operand_2 == ot;
  }

  static inline bool IsScalable() {
    return (OperandTraits<operand_0>::TypeInfo::kIsScalable |
            OperandTraits<operand_1>::TypeInfo::kIsScalable |
            OperandTraits<operand_2>::TypeInfo::kIsScalable);
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 3;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand +
      RegisterOperandTraits<operand_2>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      (RegisterOperandTraits<operand_1>::kIsRegisterOperand << 1) +
      (RegisterOperandTraits<operand_2>::kIsRegisterOperand << 2);
};

template <AccumulatorUse accumulator_use, OperandType operand_0,
          OperandType operand_1>
struct BytecodeTraits<accumulator_use, operand_0, operand_1> {
  static inline OperandType GetOperandType(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandType kOperands[] = {operand_0, operand_1};
    return kOperands[i];
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot;
  }

  static inline bool IsScalable() {
    return (OperandTraits<operand_0>::TypeInfo::kIsScalable |
            OperandTraits<operand_1>::TypeInfo::kIsScalable);
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 2;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      (RegisterOperandTraits<operand_1>::kIsRegisterOperand << 1);
};

template <AccumulatorUse accumulator_use, OperandType operand_0>
struct BytecodeTraits<accumulator_use, operand_0> {
  static inline OperandType GetOperandType(int i) {
    DCHECK(i == 0);
    return operand_0;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot;
  }

  static inline bool IsScalable() {
    return OperandTraits<operand_0>::TypeInfo::kIsScalable;
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 1;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand;
};

template <AccumulatorUse accumulator_use>
struct BytecodeTraits<accumulator_use> {
  static inline OperandType GetOperandType(int i) {
    UNREACHABLE();
    return OperandType::kNone;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return false;
  }

  static inline bool IsScalable() { return false; }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 0;
  static const int kRegisterOperandCount = 0;
  static const int kRegisterOperandBitmap = 0;
};

template <bool>
struct OperandScaler {
  static int Multiply(int size, int operand_scale) { return 0; }
};

template <>
struct OperandScaler<false> {
  static int Multiply(int size, int operand_scale) { return size; }
};

template <>
struct OperandScaler<true> {
  static int Multiply(int size, int operand_scale) {
    return size * operand_scale;
  }
};

static OperandSize ScaledOperandSize(OperandType operand_type,
                                     OperandScale operand_scale) {
  switch (operand_type) {
#define CASE(Name, TypeInfo)                                                   \
  case OperandType::k##Name: {                                                 \
    OperandSize base_size = OperandTypeInfoTraits<TypeInfo>::kUnscaledSize;    \
    int size =                                                                 \
        OperandScaler<OperandTypeInfoTraits<TypeInfo>::kIsScalable>::Multiply( \
            static_cast<int>(base_size), static_cast<int>(operand_scale));     \
    OperandSize operand_size = static_cast<OperandSize>(size);                 \
    DCHECK(operand_size == OperandSize::kByte ||                               \
           operand_size == OperandSize::kShort ||                              \
           operand_size == OperandSize::kQuad);                                \
    return operand_size;                                                       \
  }
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return OperandSize::kNone;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_TRAITS_H_
