// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_MAGLEV_ASSERT_TYPES_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_MAGLEV_ASSERT_TYPES_REDUCER_H_

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/builtin-call-descriptors.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/representations.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <typename Next>
class MaglevAssertTypesReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(MaglevAssertTypes)

  V<None> REDUCE(CheckMaglevType)(V<Object> input,
                                  maglev::NodeType expected_type,
                                  bool allow_widening_smi_to_int32) {
    __ template CallBuiltin<builtin::CheckMaglevType>(
        {.object = input,
         .type = __ SmiConstant(Smi::FromEnum(expected_type)),
         .allow_widening_smi_to_int32 =
             __ SmiConstant(Smi::FromInt(allow_widening_smi_to_int32))});
    return V<None>::Invalid();
  }
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_MAGLEV_ASSERT_TYPES_REDUCER_H_
