// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_NODE_TEST_UTILS_H_
#define V8_UNITTESTS_COMPILER_NODE_TEST_UTILS_H_

#include "src/compiler/machine-operator.h"
#include "src/compiler/simplified-operator.h"
#include "src/machine-type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ExternalReference;
template <typename T>
class Handle;
class HeapObject;
class Type;
enum TypeofMode : int;

namespace compiler {

// Forward declarations.
class BufferAccess;
class CallDescriptor;
class ContextAccess;
struct ElementAccess;
struct FieldAccess;
class Node;


using ::testing::Matcher;

#define SPECULATIVE_BINOPS(V)           \
  V(SpeculativeNumberAdd)               \
  V(SpeculativeNumberSubtract)          \
  V(SpeculativeNumberShiftLeft)         \
  V(SpeculativeNumberShiftRight)        \
  V(SpeculativeNumberShiftRightLogical) \
  V(SpeculativeNumberBitwiseAnd)        \
  V(SpeculativeNumberBitwiseOr)         \
  V(SpeculativeNumberBitwiseXor)

Matcher<Node*> IsDead();
Matcher<Node*> IsEnd(const Matcher<Node*>& control0_matcher);
Matcher<Node*> IsEnd(const Matcher<Node*>& control0_matcher,
                     const Matcher<Node*>& control1_matcher);
Matcher<Node*> IsEnd(const Matcher<Node*>& control0_matcher,
                     const Matcher<Node*>& control1_matcher,
                     const Matcher<Node*>& control2_matcher);
Matcher<Node*> IsBranch(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& control_matcher);
Matcher<Node*> IsMerge(const Matcher<Node*>& control0_matcher,
                       const Matcher<Node*>& control1_matcher);
Matcher<Node*> IsMerge(const Matcher<Node*>& control0_matcher,
                       const Matcher<Node*>& control1_matcher,
                       const Matcher<Node*>& control2_matcher);
Matcher<Node*> IsLoop(const Matcher<Node*>& control0_matcher,
                      const Matcher<Node*>& control1_matcher);
Matcher<Node*> IsLoop(const Matcher<Node*>& control0_matcher,
                      const Matcher<Node*>& control1_matcher,
                      const Matcher<Node*>& control2_matcher);
Matcher<Node*> IsIfTrue(const Matcher<Node*>& control_matcher);
Matcher<Node*> IsIfFalse(const Matcher<Node*>& control_matcher);
Matcher<Node*> IsIfSuccess(const Matcher<Node*>& control_matcher);
Matcher<Node*> IsSwitch(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& control_matcher);
Matcher<Node*> IsIfValue(const Matcher<int32_t>& value_matcher,
                         const Matcher<Node*>& control_matcher);
Matcher<Node*> IsIfDefault(const Matcher<Node*>& control_matcher);
Matcher<Node*> IsBeginRegion(const Matcher<Node*>& effect_matcher);
Matcher<Node*> IsFinishRegion(const Matcher<Node*>& value_matcher,
                              const Matcher<Node*>& effect_matcher);
Matcher<Node*> IsReturn(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& effect_matcher,
                        const Matcher<Node*>& control_matcher);
Matcher<Node*> IsReturn2(const Matcher<Node*>& value_matcher,
                         const Matcher<Node*>& value2_matcher,
                         const Matcher<Node*>& effect_matcher,
                         const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTerminate(const Matcher<Node*>& effect_matcher,
                           const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTypeGuard(const Matcher<Node*>& value_matcher,
                           const Matcher<Node*>& control_matcher);
Matcher<Node*> IsExternalConstant(
    const Matcher<ExternalReference>& value_matcher);
Matcher<Node*> IsHeapConstant(Handle<HeapObject> value);
Matcher<Node*> IsFloat32Constant(const Matcher<float>& value_matcher);
Matcher<Node*> IsFloat64Constant(const Matcher<double>& value_matcher);
Matcher<Node*> IsInt32Constant(const Matcher<int32_t>& value_matcher);
Matcher<Node*> IsInt64Constant(const Matcher<int64_t>& value_matcher);
Matcher<Node*> IsNumberConstant(const Matcher<double>& value_matcher);
Matcher<Node*> IsPointerConstant(const Matcher<intptr_t>& value_matcher);
Matcher<Node*> IsSelect(const Matcher<MachineRepresentation>& type_matcher,
                        const Matcher<Node*>& value0_matcher,
                        const Matcher<Node*>& value1_matcher,
                        const Matcher<Node*>& value2_matcher);
Matcher<Node*> IsPhi(const Matcher<MachineRepresentation>& type_matcher,
                     const Matcher<Node*>& value0_matcher,
                     const Matcher<Node*>& value1_matcher,
                     const Matcher<Node*>& merge_matcher);
Matcher<Node*> IsPhi(const Matcher<MachineRepresentation>& type_matcher,
                     const Matcher<Node*>& value0_matcher,
                     const Matcher<Node*>& value1_matcher,
                     const Matcher<Node*>& value2_matcher,
                     const Matcher<Node*>& merge_matcher);
Matcher<Node*> IsEffectPhi(const Matcher<Node*>& effect0_matcher,
                           const Matcher<Node*>& effect1_matcher,
                           const Matcher<Node*>& merge_matcher);
Matcher<Node*> IsProjection(const Matcher<size_t>& index_matcher,
                            const Matcher<Node*>& base_matcher);
Matcher<Node*> IsCall(const Matcher<const CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(const Matcher<const CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(const Matcher<const CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& value2_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(const Matcher<const CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& value2_matcher,
                      const Matcher<Node*>& value3_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(const Matcher<const CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& value2_matcher,
                      const Matcher<Node*>& value3_matcher,
                      const Matcher<Node*>& value4_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(const Matcher<const CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& value2_matcher,
                      const Matcher<Node*>& value3_matcher,
                      const Matcher<Node*>& value4_matcher,
                      const Matcher<Node*>& value5_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(
    const Matcher<const CallDescriptor*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& value3_matcher,
    const Matcher<Node*>& value4_matcher, const Matcher<Node*>& value5_matcher,
    const Matcher<Node*>& value6_matcher, const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& value3_matcher,
    const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& value3_matcher,
    const Matcher<Node*>& value4_matcher, const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& value3_matcher,
    const Matcher<Node*>& value4_matcher, const Matcher<Node*>& value5_matcher,
    const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& value3_matcher,
    const Matcher<Node*>& value4_matcher, const Matcher<Node*>& value5_matcher,
    const Matcher<Node*>& value6_matcher, const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsTailCall(
    const Matcher<CallDescriptor const*>& descriptor_matcher,
    const Matcher<Node*>& value0_matcher, const Matcher<Node*>& value1_matcher,
    const Matcher<Node*>& value2_matcher, const Matcher<Node*>& value3_matcher,
    const Matcher<Node*>& value4_matcher, const Matcher<Node*>& value5_matcher,
    const Matcher<Node*>& value6_matcher, const Matcher<Node*>& value7_matcher,
    const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);


Matcher<Node*> IsBooleanNot(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsReferenceEqual(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberEqual(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberLessThan(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberAdd(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);

#define DECLARE_SPECULATIVE_BINOP_MATCHER(opcode)                             \
  Matcher<Node*> Is##opcode(const Matcher<NumberOperationHint>& hint_matcher, \
                            const Matcher<Node*>& lhs_matcher,                \
                            const Matcher<Node*>& rhs_matcher,                \
                            const Matcher<Node*>& effect_matcher,             \
                            const Matcher<Node*>& control_matcher);
SPECULATIVE_BINOPS(DECLARE_SPECULATIVE_BINOP_MATCHER);
#undef DECLARE_SPECULATIVE_BINOP_MATCHER

Matcher<Node*> IsNumberSubtract(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberMultiply(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberShiftLeft(const Matcher<Node*>& lhs_matcher,
                                 const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberShiftRight(const Matcher<Node*>& lhs_matcher,
                                  const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberShiftRightLogical(const Matcher<Node*>& lhs_matcher,
                                         const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberImul(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberAbs(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAcos(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAcosh(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAsin(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAsinh(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAtan(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAtanh(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberAtan2(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberCbrt(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberCeil(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberClz32(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberCos(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberCosh(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberExp(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberExpm1(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberFloor(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberFround(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberLog(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberLog1p(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberLog10(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberLog2(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberMax(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberMin(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberRound(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberPow(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberSign(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberSin(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberSinh(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberSqrt(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberTan(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberTanh(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberTrunc(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsStringFromCharCode(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsAllocate(const Matcher<Node*>& size_matcher,
                          const Matcher<Node*>& effect_matcher,
                          const Matcher<Node*>& control_matcher);
Matcher<Node*> IsLoadField(const Matcher<FieldAccess>& access_matcher,
                           const Matcher<Node*>& base_matcher,
                           const Matcher<Node*>& effect_matcher,
                           const Matcher<Node*>& control_matcher);
Matcher<Node*> IsStoreField(const Matcher<FieldAccess>& access_matcher,
                            const Matcher<Node*>& base_matcher,
                            const Matcher<Node*>& value_matcher,
                            const Matcher<Node*>& effect_matcher,
                            const Matcher<Node*>& control_matcher);
Matcher<Node*> IsLoadBuffer(const Matcher<BufferAccess>& access_matcher,
                            const Matcher<Node*>& buffer_matcher,
                            const Matcher<Node*>& offset_matcher,
                            const Matcher<Node*>& length_matcher,
                            const Matcher<Node*>& effect_matcher,
                            const Matcher<Node*>& control_matcher);
Matcher<Node*> IsStoreBuffer(const Matcher<BufferAccess>& access_matcher,
                             const Matcher<Node*>& buffer_matcher,
                             const Matcher<Node*>& offset_matcher,
                             const Matcher<Node*>& length_matcher,
                             const Matcher<Node*>& value_matcher,
                             const Matcher<Node*>& effect_matcher,
                             const Matcher<Node*>& control_matcher);
Matcher<Node*> IsLoadElement(const Matcher<ElementAccess>& access_matcher,
                             const Matcher<Node*>& base_matcher,
                             const Matcher<Node*>& index_matcher,
                             const Matcher<Node*>& control_matcher,
                             const Matcher<Node*>& effect_matcher);
Matcher<Node*> IsStoreElement(const Matcher<ElementAccess>& access_matcher,
                              const Matcher<Node*>& base_matcher,
                              const Matcher<Node*>& index_matcher,
                              const Matcher<Node*>& value_matcher,
                              const Matcher<Node*>& effect_matcher,
                              const Matcher<Node*>& control_matcher);
Matcher<Node*> IsObjectIsNaN(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsObjectIsReceiver(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsObjectIsSmi(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsObjectIsUndetectable(const Matcher<Node*>& value_matcher);

Matcher<Node*> IsLoad(const Matcher<LoadRepresentation>& rep_matcher,
                      const Matcher<Node*>& base_matcher,
                      const Matcher<Node*>& index_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsUnalignedLoad(
    const Matcher<UnalignedLoadRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher,
    const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsStore(const Matcher<StoreRepresentation>& rep_matcher,
                       const Matcher<Node*>& base_matcher,
                       const Matcher<Node*>& index_matcher,
                       const Matcher<Node*>& value_matcher,
                       const Matcher<Node*>& effect_matcher,
                       const Matcher<Node*>& control_matcher);
Matcher<Node*> IsUnalignedStore(
    const Matcher<UnalignedStoreRepresentation>& rep_matcher,
    const Matcher<Node*>& base_matcher, const Matcher<Node*>& index_matcher,
    const Matcher<Node*>& value_matcher, const Matcher<Node*>& effect_matcher,
    const Matcher<Node*>& control_matcher);
Matcher<Node*> IsStackSlot(const Matcher<StackSlotRepresentation>& rep_matcher);
Matcher<Node*> IsWord32And(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Or(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Xor(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Sar(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Shl(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Shr(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Ror(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Equal(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Clz(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsWord32Ctz(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsWord32Popcnt(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsWord64And(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Or(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Shl(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Sar(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Equal(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32AddWithOverflow(const Matcher<Node*>& lhs_matcher,
                                      const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32SubWithOverflow(const Matcher<Node*>& lhs_matcher,
                                      const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32Add(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32Sub(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32Mul(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32MulHigh(const Matcher<Node*>& lhs_matcher,
                              const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32LessThan(const Matcher<Node*>& lhs_matcher,
                               const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsUint32LessThan(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsUint32LessThanOrEqual(const Matcher<Node*>& lhs_matcher,
                                       const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt64Add(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt64Sub(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt64Mul(const Matcher<Node*>& lhs_matcher,
                          const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsJSAdd(const Matcher<Node*>& lhs_matcher,
                       const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsBitcastTaggedToWord(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsBitcastWordToTagged(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsBitcastWordToTaggedSigned(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsTruncateFloat64ToWord32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeFloat64ToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeFloat64ToUint32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeInt32ToFloat64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeInt32ToInt64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeUint32ToFloat64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeUint32ToUint64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsTruncateFloat64ToFloat32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsTruncateInt64ToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat32Abs(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat32Neg(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat32Equal(const Matcher<Node*>& lhs_matcher,
                              const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat32LessThan(const Matcher<Node*>& lhs_matcher,
                                 const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat32LessThanOrEqual(const Matcher<Node*>& lhs_matcher,
                                        const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Max(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Min(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Add(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Sub(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Mul(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Abs(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64Neg(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64Sqrt(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64RoundDown(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64RoundTruncate(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64RoundTiesAway(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64ExtractLowWord32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64ExtractHighWord32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64InsertLowWord32(const Matcher<Node*>& lhs_matcher,
                                        const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64InsertHighWord32(const Matcher<Node*>& lhs_matcher,
                                         const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsToNumber(const Matcher<Node*>& base_matcher,
                          const Matcher<Node*>& context_matcher,
                          const Matcher<Node*>& effect_matcher,
                          const Matcher<Node*>& control_matcher);
Matcher<Node*> IsLoadContext(const Matcher<ContextAccess>& access_matcher,
                             const Matcher<Node*>& context_matcher);
Matcher<Node*> IsNumberToBoolean(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsNumberToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsNumberToUint32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsParameter(const Matcher<int> index_matcher);
Matcher<Node*> IsLoadFramePointer();
Matcher<Node*> IsLoadParentFramePointer();
Matcher<Node*> IsPlainPrimitiveToNumber(const Matcher<Node*>& input_matcher);

Matcher<Node*> IsInt32PairAdd(const Matcher<Node*>& a_matcher,
                              const Matcher<Node*>& b_matcher,
                              const Matcher<Node*>& c_matcher,
                              const Matcher<Node*>& d_matcher);
Matcher<Node*> IsInt32PairSub(const Matcher<Node*>& a_matcher,
                              const Matcher<Node*>& b_matcher,
                              const Matcher<Node*>& c_matcher,
                              const Matcher<Node*>& d_matcher);
Matcher<Node*> IsInt32PairMul(const Matcher<Node*>& a_matcher,
                              const Matcher<Node*>& b_matcher,
                              const Matcher<Node*>& c_matcher,
                              const Matcher<Node*>& d_matcher);

Matcher<Node*> IsWord32PairShl(const Matcher<Node*>& lhs_matcher,
                               const Matcher<Node*>& mid_matcher,
                               const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32PairShr(const Matcher<Node*>& lhs_matcher,
                               const Matcher<Node*>& mid_matcher,
                               const Matcher<Node*>& rhs_matcher);

Matcher<Node*> IsWord32PairSar(const Matcher<Node*>& lhs_matcher,
                               const Matcher<Node*>& mid_matcher,
                               const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32ReverseBytes(const Matcher<Node*>& value_matcher);

Matcher<Node*> IsStackSlot();

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_NODE_TEST_UTILS_H_
