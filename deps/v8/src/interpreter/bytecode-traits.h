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
  typedef OperandTypeInfoTraits<OperandTypeInfo::kNone> TypeInfoTraits;
  static const OperandTypeInfo kOperandTypeInfo = OperandTypeInfo::kNone;
};

#define DECLARE_OPERAND_TYPE_TRAITS(Name, InfoType)           \
  template <>                                                 \
  struct OperandTraits<OperandType::k##Name> {                \
    typedef OperandTypeInfoTraits<InfoType> TypeInfoTraits;   \
    static const OperandTypeInfo kOperandTypeInfo = InfoType; \
  };
OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE_TRAITS)
#undef DECLARE_OPERAND_TYPE_TRAITS

template <OperandType operand_type, OperandScale operand_scale>
struct OperandScaler {
  template <bool, OperandSize, OperandScale>
  struct Helper {
    static const int kSize = 0;
  };
  template <OperandSize size, OperandScale scale>
  struct Helper<false, size, scale> {
    static const int kSize = static_cast<int>(size);
  };
  template <OperandSize size, OperandScale scale>
  struct Helper<true, size, scale> {
    static const int kSize = static_cast<int>(size) * static_cast<int>(scale);
  };

  static const int kSize =
      Helper<OperandTraits<operand_type>::TypeInfoTraits::kIsScalable,
             OperandTraits<operand_type>::TypeInfoTraits::kUnscaledSize,
             operand_scale>::kSize;
  static const OperandSize kOperandSize = static_cast<OperandSize>(kSize);
};

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
  static const OperandType* GetOperandTypes() {
    static const OperandType operand_types[] = {operand_0, operand_1, operand_2,
                                                operand_3, OperandType::kNone};
    return operand_types;
  }

  static const OperandTypeInfo* GetOperandTypeInfos() {
    static const OperandTypeInfo operand_type_infos[] = {
        OperandTraits<operand_0>::kOperandTypeInfo,
        OperandTraits<operand_1>::kOperandTypeInfo,
        OperandTraits<operand_2>::kOperandTypeInfo,
        OperandTraits<operand_3>::kOperandTypeInfo, OperandTypeInfo::kNone};
    return operand_type_infos;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot || operand_2 == ot ||
           operand_3 == ot;
  }

  static inline bool IsScalable() {
    return (OperandTraits<operand_0>::TypeInfoTraits::kIsScalable |
            OperandTraits<operand_1>::TypeInfoTraits::kIsScalable |
            OperandTraits<operand_2>::TypeInfoTraits::kIsScalable |
            OperandTraits<operand_3>::TypeInfoTraits::kIsScalable);
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 4;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand +
      RegisterOperandTraits<operand_2>::kIsRegisterOperand +
      RegisterOperandTraits<operand_3>::kIsRegisterOperand;
};

template <AccumulatorUse accumulator_use, OperandType operand_0,
          OperandType operand_1, OperandType operand_2>
struct BytecodeTraits<accumulator_use, operand_0, operand_1, operand_2> {
  static const OperandType* GetOperandTypes() {
    static const OperandType operand_types[] = {operand_0, operand_1, operand_2,
                                                OperandType::kNone};
    return operand_types;
  }

  static const OperandTypeInfo* GetOperandTypeInfos() {
    static const OperandTypeInfo operand_type_infos[] = {
        OperandTraits<operand_0>::kOperandTypeInfo,
        OperandTraits<operand_1>::kOperandTypeInfo,
        OperandTraits<operand_2>::kOperandTypeInfo, OperandTypeInfo::kNone};
    return operand_type_infos;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot || operand_2 == ot;
  }

  static inline bool IsScalable() {
    return (OperandTraits<operand_0>::TypeInfoTraits::kIsScalable |
            OperandTraits<operand_1>::TypeInfoTraits::kIsScalable |
            OperandTraits<operand_2>::TypeInfoTraits::kIsScalable);
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 3;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand +
      RegisterOperandTraits<operand_2>::kIsRegisterOperand;
};

template <AccumulatorUse accumulator_use, OperandType operand_0,
          OperandType operand_1>
struct BytecodeTraits<accumulator_use, operand_0, operand_1> {
  static const OperandType* GetOperandTypes() {
    static const OperandType operand_types[] = {operand_0, operand_1,
                                                OperandType::kNone};
    return operand_types;
  }

  static const OperandTypeInfo* GetOperandTypeInfos() {
    static const OperandTypeInfo operand_type_infos[] = {
        OperandTraits<operand_0>::kOperandTypeInfo,
        OperandTraits<operand_1>::kOperandTypeInfo, OperandTypeInfo::kNone};
    return operand_type_infos;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot || operand_1 == ot;
  }

  static inline bool IsScalable() {
    return (OperandTraits<operand_0>::TypeInfoTraits::kIsScalable |
            OperandTraits<operand_1>::TypeInfoTraits::kIsScalable);
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 2;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand +
      RegisterOperandTraits<operand_1>::kIsRegisterOperand;
};

template <AccumulatorUse accumulator_use, OperandType operand_0>
struct BytecodeTraits<accumulator_use, operand_0> {
  static const OperandType* GetOperandTypes() {
    static const OperandType operand_types[] = {operand_0, OperandType::kNone};
    return operand_types;
  }

  static const OperandTypeInfo* GetOperandTypeInfos() {
    static const OperandTypeInfo operand_type_infos[] = {
        OperandTraits<operand_0>::kOperandTypeInfo, OperandTypeInfo::kNone};
    return operand_type_infos;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return operand_0 == ot;
  }

  static inline bool IsScalable() {
    return OperandTraits<operand_0>::TypeInfoTraits::kIsScalable;
  }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 1;
  static const int kRegisterOperandCount =
      RegisterOperandTraits<operand_0>::kIsRegisterOperand;
};

template <AccumulatorUse accumulator_use>
struct BytecodeTraits<accumulator_use> {
  static const OperandType* GetOperandTypes() {
    static const OperandType operand_types[] = {OperandType::kNone};
    return operand_types;
  }

  static const OperandTypeInfo* GetOperandTypeInfos() {
    static const OperandTypeInfo operand_type_infos[] = {
        OperandTypeInfo::kNone};
    return operand_type_infos;
  }

  template <OperandType ot>
  static inline bool HasAnyOperandsOfType() {
    return false;
  }

  static inline bool IsScalable() { return false; }

  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 0;
  static const int kRegisterOperandCount = 0;
};

static OperandSize ScaledOperandSize(OperandType operand_type,
                                     OperandScale operand_scale) {
  STATIC_ASSERT(static_cast<int>(OperandScale::kQuadruple) == 4 &&
                OperandScale::kLast == OperandScale::kQuadruple);
  int index = static_cast<int>(operand_scale) >> 1;
  switch (operand_type) {
#define CASE(Name, TypeInfo)                                    \
  case OperandType::k##Name: {                                  \
    static const OperandSize kOperandSizes[] = {                \
        OperandScaler<OperandType::k##Name,                     \
                      OperandScale::kSingle>::kOperandSize,     \
        OperandScaler<OperandType::k##Name,                     \
                      OperandScale::kDouble>::kOperandSize,     \
        OperandScaler<OperandType::k##Name,                     \
                      OperandScale::kQuadruple>::kOperandSize}; \
    return kOperandSizes[index];                                \
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
