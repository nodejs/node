// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_TRAITS_H_
#define V8_INTERPRETER_BYTECODE_TRAITS_H_

#include "src/interpreter/bytecode-operands.h"

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
  using TypeInfoTraits = OperandTypeInfoTraits<OperandTypeInfo::kNone>;
  static const OperandTypeInfo kOperandTypeInfo = OperandTypeInfo::kNone;
};

#define DECLARE_OPERAND_TYPE_TRAITS(Name, InfoType)           \
  template <>                                                 \
  struct OperandTraits<OperandType::k##Name> {                \
    using TypeInfoTraits = OperandTypeInfoTraits<InfoType>;   \
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

template <int... values>
struct SumHelper;
template <int value>
struct SumHelper<value> {
  static const int kValue = value;
};
template <int value, int... values>
struct SumHelper<value, values...> {
  static const int kValue = value + SumHelper<values...>::kValue;
};

template <AccumulatorUse accumulator_use, OperandType... operands>
struct BytecodeTraits {
  static const OperandType kOperandTypes[];
  static const OperandTypeInfo kOperandTypeInfos[];
  static const OperandSize kSingleScaleOperandSizes[];
  static const OperandSize kDoubleScaleOperandSizes[];
  static const OperandSize kQuadrupleScaleOperandSizes[];
  static const int kSingleScaleSize = SumHelper<
      1, OperandScaler<operands, OperandScale::kSingle>::kSize...>::kValue;
  static const int kDoubleScaleSize = SumHelper<
      1, OperandScaler<operands, OperandScale::kDouble>::kSize...>::kValue;
  static const int kQuadrupleScaleSize = SumHelper<
      1, OperandScaler<operands, OperandScale::kQuadruple>::kSize...>::kValue;
  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = sizeof...(operands);
};

template <AccumulatorUse accumulator_use, OperandType... operands>
STATIC_CONST_MEMBER_DEFINITION const OperandType
    BytecodeTraits<accumulator_use, operands...>::kOperandTypes[] = {
        operands...};
template <AccumulatorUse accumulator_use, OperandType... operands>
STATIC_CONST_MEMBER_DEFINITION const OperandTypeInfo
    BytecodeTraits<accumulator_use, operands...>::kOperandTypeInfos[] = {
        OperandTraits<operands>::kOperandTypeInfo...};
template <AccumulatorUse accumulator_use, OperandType... operands>
STATIC_CONST_MEMBER_DEFINITION const OperandSize
    BytecodeTraits<accumulator_use, operands...>::kSingleScaleOperandSizes[] = {
        OperandScaler<operands, OperandScale::kSingle>::kOperandSize...};
template <AccumulatorUse accumulator_use, OperandType... operands>
STATIC_CONST_MEMBER_DEFINITION const OperandSize
    BytecodeTraits<accumulator_use, operands...>::kDoubleScaleOperandSizes[] = {
        OperandScaler<operands, OperandScale::kDouble>::kOperandSize...};
template <AccumulatorUse accumulator_use, OperandType... operands>
STATIC_CONST_MEMBER_DEFINITION const OperandSize BytecodeTraits<
    accumulator_use, operands...>::kQuadrupleScaleOperandSizes[] = {
    OperandScaler<operands, OperandScale::kQuadruple>::kOperandSize...};

template <AccumulatorUse accumulator_use>
struct BytecodeTraits<accumulator_use> {
  static const OperandType kOperandTypes[];
  static const OperandTypeInfo kOperandTypeInfos[];
  static const OperandSize kSingleScaleOperandSizes[];
  static const OperandSize kDoubleScaleOperandSizes[];
  static const OperandSize kQuadrupleScaleOperandSizes[];
  static const int kSingleScaleSize = 1;
  static const int kDoubleScaleSize = 1;
  static const int kQuadrupleScaleSize = 1;
  static const AccumulatorUse kAccumulatorUse = accumulator_use;
  static const int kOperandCount = 0;
};

template <AccumulatorUse accumulator_use>
STATIC_CONST_MEMBER_DEFINITION const OperandType
    BytecodeTraits<accumulator_use>::kOperandTypes[] = {OperandType::kNone};
template <AccumulatorUse accumulator_use>
STATIC_CONST_MEMBER_DEFINITION const OperandTypeInfo
    BytecodeTraits<accumulator_use>::kOperandTypeInfos[] = {
        OperandTypeInfo::kNone};
template <AccumulatorUse accumulator_use>
STATIC_CONST_MEMBER_DEFINITION const OperandSize
    BytecodeTraits<accumulator_use>::kSingleScaleOperandSizes[] = {
        OperandSize::kNone};
template <AccumulatorUse accumulator_use>
STATIC_CONST_MEMBER_DEFINITION const OperandSize
    BytecodeTraits<accumulator_use>::kDoubleScaleOperandSizes[] = {
        OperandSize::kNone};
template <AccumulatorUse accumulator_use>
STATIC_CONST_MEMBER_DEFINITION const OperandSize
    BytecodeTraits<accumulator_use>::kQuadrupleScaleOperandSizes[] = {
        OperandSize::kNone};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_TRAITS_H_
