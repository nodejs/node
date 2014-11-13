// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_NODE_TEST_UTILS_H_
#define V8_UNITTESTS_COMPILER_NODE_TEST_UTILS_H_

#include "src/compiler/machine-operator.h"
#include "src/compiler/machine-type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ExternalReference;
class HeapObject;
template <class T>
class Unique;

namespace compiler {

// Forward declarations.
class CallDescriptor;
struct ElementAccess;
struct FieldAccess;
class Node;


using ::testing::Matcher;


Matcher<Node*> IsBranch(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& control_matcher);
Matcher<Node*> IsMerge(const Matcher<Node*>& control0_matcher,
                       const Matcher<Node*>& control1_matcher);
Matcher<Node*> IsIfTrue(const Matcher<Node*>& control_matcher);
Matcher<Node*> IsIfFalse(const Matcher<Node*>& control_matcher);
Matcher<Node*> IsValueEffect(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsFinish(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& effect_matcher);
Matcher<Node*> IsExternalConstant(
    const Matcher<ExternalReference>& value_matcher);
Matcher<Node*> IsHeapConstant(
    const Matcher<Unique<HeapObject> >& value_matcher);
Matcher<Node*> IsFloat32Constant(const Matcher<float>& value_matcher);
Matcher<Node*> IsFloat64Constant(const Matcher<double>& value_matcher);
Matcher<Node*> IsInt32Constant(const Matcher<int32_t>& value_matcher);
Matcher<Node*> IsInt64Constant(const Matcher<int64_t>& value_matcher);
Matcher<Node*> IsNumberConstant(const Matcher<double>& value_matcher);
Matcher<Node*> IsSelect(const Matcher<MachineType>& type_matcher,
                        const Matcher<Node*>& value0_matcher,
                        const Matcher<Node*>& value1_matcher,
                        const Matcher<Node*>& value2_matcher);
Matcher<Node*> IsPhi(const Matcher<MachineType>& type_matcher,
                     const Matcher<Node*>& value0_matcher,
                     const Matcher<Node*>& value1_matcher,
                     const Matcher<Node*>& merge_matcher);
Matcher<Node*> IsEffectPhi(const Matcher<Node*>& effect0_matcher,
                           const Matcher<Node*>& effect1_matcher,
                           const Matcher<Node*>& merge_matcher);
Matcher<Node*> IsProjection(const Matcher<size_t>& index_matcher,
                            const Matcher<Node*>& base_matcher);
Matcher<Node*> IsCall(const Matcher<CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsCall(const Matcher<CallDescriptor*>& descriptor_matcher,
                      const Matcher<Node*>& value0_matcher,
                      const Matcher<Node*>& value1_matcher,
                      const Matcher<Node*>& value2_matcher,
                      const Matcher<Node*>& value3_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);

Matcher<Node*> IsBooleanNot(const Matcher<Node*>& value_matcher);
Matcher<Node*> IsNumberEqual(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberLessThan(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberSubtract(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsNumberMultiply(const Matcher<Node*>& lhs_matcher,
                                const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsLoadField(const Matcher<FieldAccess>& access_matcher,
                           const Matcher<Node*>& base_matcher,
                           const Matcher<Node*>& effect_matcher,
                           const Matcher<Node*>& control_matcher);
Matcher<Node*> IsLoadElement(const Matcher<ElementAccess>& access_matcher,
                             const Matcher<Node*>& base_matcher,
                             const Matcher<Node*>& index_matcher,
                             const Matcher<Node*>& length_matcher,
                             const Matcher<Node*>& effect_matcher);
Matcher<Node*> IsStoreElement(const Matcher<ElementAccess>& access_matcher,
                              const Matcher<Node*>& base_matcher,
                              const Matcher<Node*>& index_matcher,
                              const Matcher<Node*>& length_matcher,
                              const Matcher<Node*>& value_matcher,
                              const Matcher<Node*>& effect_matcher,
                              const Matcher<Node*>& control_matcher);

Matcher<Node*> IsLoad(const Matcher<LoadRepresentation>& rep_matcher,
                      const Matcher<Node*>& base_matcher,
                      const Matcher<Node*>& index_matcher,
                      const Matcher<Node*>& effect_matcher,
                      const Matcher<Node*>& control_matcher);
Matcher<Node*> IsStore(const Matcher<StoreRepresentation>& rep_matcher,
                       const Matcher<Node*>& base_matcher,
                       const Matcher<Node*>& index_matcher,
                       const Matcher<Node*>& value_matcher,
                       const Matcher<Node*>& effect_matcher,
                       const Matcher<Node*>& control_matcher);
Matcher<Node*> IsWord32And(const Matcher<Node*>& lhs_matcher,
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
Matcher<Node*> IsWord64And(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Shl(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Sar(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord64Equal(const Matcher<Node*>& lhs_matcher,
                             const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsInt32AddWithOverflow(const Matcher<Node*>& lhs_matcher,
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
Matcher<Node*> IsChangeFloat64ToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeFloat64ToUint32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeInt32ToFloat64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeInt32ToInt64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeUint32ToFloat64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeUint32ToUint64(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsTruncateFloat64ToFloat32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsTruncateFloat64ToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsTruncateInt64ToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64Sub(const Matcher<Node*>& lhs_matcher,
                            const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsFloat64Sqrt(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64Floor(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64Ceil(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64RoundTruncate(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsFloat64RoundTiesAway(const Matcher<Node*>& input_matcher);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_NODE_TEST_UTILS_H_
