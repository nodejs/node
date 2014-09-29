// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_UNITTESTS_NODE_MATCHERS_H_
#define V8_COMPILER_UNITTESTS_NODE_MATCHERS_H_

#include "src/compiler/machine-operator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HeapObject;
template <class T>
class PrintableUnique;

namespace compiler {

// Forward declarations.
class Node;

using testing::Matcher;

Matcher<Node*> IsBranch(const Matcher<Node*>& value_matcher,
                        const Matcher<Node*>& control_matcher);
Matcher<Node*> IsHeapConstant(
    const Matcher<PrintableUnique<HeapObject> >& value_matcher);
Matcher<Node*> IsInt32Constant(const Matcher<int32_t>& value_matcher);
Matcher<Node*> IsPhi(const Matcher<Node*>& value0_matcher,
                     const Matcher<Node*>& value1_matcher,
                     const Matcher<Node*>& merge_matcher);
Matcher<Node*> IsProjection(const Matcher<int32_t>& index_matcher,
                            const Matcher<Node*>& base_matcher);

Matcher<Node*> IsLoad(const Matcher<MachineType>& type_matcher,
                      const Matcher<Node*>& base_matcher,
                      const Matcher<Node*>& index_matcher,
                      const Matcher<Node*>& effect_matcher);
Matcher<Node*> IsStore(const Matcher<MachineType>& type_matcher,
                       const Matcher<WriteBarrierKind>& write_barrier_matcher,
                       const Matcher<Node*>& base_matcher,
                       const Matcher<Node*>& index_matcher,
                       const Matcher<Node*>& value_matcher,
                       const Matcher<Node*>& effect_matcher,
                       const Matcher<Node*>& control_matcher);
Matcher<Node*> IsWord32And(const Matcher<Node*>& lhs_matcher,
                           const Matcher<Node*>& rhs_matcher);
Matcher<Node*> IsWord32Sar(const Matcher<Node*>& lhs_matcher,
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
Matcher<Node*> IsConvertInt64ToInt32(const Matcher<Node*>& input_matcher);
Matcher<Node*> IsChangeInt32ToFloat64(const Matcher<Node*>& input_matcher);

}  //  namespace compiler
}  //  namespace internal
}  //  namespace v8

#endif  // V8_COMPILER_UNITTESTS_NODE_MATCHERS_H_
