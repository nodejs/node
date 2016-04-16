// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_TRAITS_H_
#define V8_INTERPRETER_BYTECODE_TRAITS_H_

#include "src/interpreter/bytecodes.h"

namespace v8 {
namespace internal {
namespace interpreter {

// TODO(rmcilroy): consider simplifying this to avoid the template magic.

// Template helpers to deduce the number of operands each bytecode has.
#define OPERAND_TERM OperandType::kNone, OperandType::kNone, OperandType::kNone

template <OperandType>
struct OperandTraits {};

#define DECLARE_OPERAND_SIZE(Name, Size)             \
  template <>                                        \
  struct OperandTraits<OperandType::k##Name> {       \
    static const OperandSize kSizeType = Size;       \
    static const int kSize = static_cast<int>(Size); \
  };
OPERAND_TYPE_LIST(DECLARE_OPERAND_SIZE)
#undef DECLARE_OPERAND_SIZE

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

template <OperandType... Args>
struct BytecodeTraits {};

template <OperandType operand_0, OperandType operand_1, OperandType operand_2,
          OperandType operand_3>
struct BytecodeTraits<operand_0, operand_1, operand_2, operand_3,
                      OPERAND_TERM> {
  static OperandType GetOperandType(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandType kOperands[] = {operand_0, operand_1, operand_2,
                                     operand_3};
    return kOperands[i];
  }

  static inline OperandSize GetOperandSize(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandSize kOperandSizes[] =
        {OperandTraits<operand_0>::kSizeType,
         OperandTraits<operand_1>::kSizeType,
         OperandTraits<operand_2>::kSizeType,
         OperandTraits<operand_3>::kSizeType};
    return kOperandSizes[i];
  }

  static inline int GetOperandOffset(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const int kOffset0 = 1;
    const int kOffset1 = kOffset0 + OperandTraits<operand_0>::kSize;
    const int kOffset2 = kOffset1 + OperandTraits<operand_1>::kSize;
    const int kOffset3 = kOffset2 + OperandTraits<operand_2>::kSize;
    const int kOperandOffsets[] = {kOffset0, kOffset1, kOffset2, kOffset3};
    return kOperandOffsets[i];
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot || operand_2 == ot ||
           operand_3 == ot;
  }

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
  static const int kSize =
      1 + OperandTraits<operand_0>::kSize + OperandTraits<operand_1>::kSize +
      OperandTraits<operand_2>::kSize + OperandTraits<operand_3>::kSize;
};

template <OperandType operand_0, OperandType operand_1, OperandType operand_2>
struct BytecodeTraits<operand_0, operand_1, operand_2, OPERAND_TERM> {
  static inline OperandType GetOperandType(int i) {
    DCHECK(0 <= i && i <= 2);
    const OperandType kOperands[] = {operand_0, operand_1, operand_2};
    return kOperands[i];
  }

  static inline OperandSize GetOperandSize(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandSize kOperandSizes[] =
        {OperandTraits<operand_0>::kSizeType,
         OperandTraits<operand_1>::kSizeType,
         OperandTraits<operand_2>::kSizeType};
    return kOperandSizes[i];
  }

  static inline int GetOperandOffset(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const int kOffset0 = 1;
    const int kOffset1 = kOffset0 + OperandTraits<operand_0>::kSize;
    const int kOffset2 = kOffset1 + OperandTraits<operand_1>::kSize;
    const int kOperandOffsets[] = {kOffset0, kOffset1, kOffset2};
    return kOperandOffsets[i];
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot || operand_2 == ot;
  }

  static const int kOperandCount = 3;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand +
      RegisterOperandTraits<operand_2>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      (RegisterOperandTraits<operand_1>::kIsRegisterOperand << 1) +
      (RegisterOperandTraits<operand_2>::kIsRegisterOperand << 2);
  static const int kSize =
      1 + OperandTraits<operand_0>::kSize + OperandTraits<operand_1>::kSize +
      OperandTraits<operand_2>::kSize;
};

template <OperandType operand_0, OperandType operand_1>
struct BytecodeTraits<operand_0, operand_1, OPERAND_TERM> {
  static inline OperandType GetOperandType(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandType kOperands[] = {operand_0, operand_1};
    return kOperands[i];
  }

  static inline OperandSize GetOperandSize(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const OperandSize kOperandSizes[] =
        {OperandTraits<operand_0>::kSizeType,
         OperandTraits<operand_1>::kSizeType};
    return kOperandSizes[i];
  }

  static inline int GetOperandOffset(int i) {
    DCHECK(0 <= i && i < kOperandCount);
    const int kOffset0 = 1;
    const int kOffset1 = kOffset0 + OperandTraits<operand_0>::kSize;
    const int kOperandOffsets[] = {kOffset0, kOffset1};
    return kOperandOffsets[i];
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot;
  }

  static const int kOperandCount = 2;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      (RegisterOperandTraits<operand_1>::kIsRegisterOperand << 1);
  static const int kSize =
      1 + OperandTraits<operand_0>::kSize + OperandTraits<operand_1>::kSize;
};

template <OperandType operand_0>
struct BytecodeTraits<operand_0, OPERAND_TERM> {
  static inline OperandType GetOperandType(int i) {
    DCHECK(i == 0);
    return operand_0;
  }

  static inline OperandSize GetOperandSize(int i) {
    DCHECK(i == 0);
    return OperandTraits<operand_0>::kSizeType;
  }

  static inline int GetOperandOffset(int i) {
    DCHECK(i == 0);
    return 1;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot;
  }

  static const int kOperandCount = 1;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand;
  static const int kRegisterOperandBitmap =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand;
  static const int kSize = 1 + OperandTraits<operand_0>::kSize;
};

template <>
struct BytecodeTraits<OperandType::kNone, OPERAND_TERM> {
  static inline OperandType GetOperandType(int i) {
    UNREACHABLE();
    return OperandType::kNone;
  }

  static inline OperandSize GetOperandSize(int i) {
    UNREACHABLE();
    return OperandSize::kNone;
  }

  static inline int GetOperandOffset(int i) {
    UNREACHABLE();
    return 1;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return false;
  }

  static const int kOperandCount = 0;
  static const int kRegisterOperandCount = 0;
  static const int kRegisterOperandBitmap = 0;
  static const int kSize = 1 + OperandTraits<OperandType::kNone>::kSize;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_TRAITS_H_
