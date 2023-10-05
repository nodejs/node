// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_DEBUG_FEATURE_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_DEBUG_FEATURE_LOWERING_REDUCER_H_

#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/representations.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <typename Next>
class DebugFeatureLoweringReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  OpIndex REDUCE(DebugPrint)(OpIndex input, RegisterRepresentation rep) {
    switch (rep.value()) {
      case RegisterRepresentation::PointerSized():
        __ CallBuiltin_DebugPrintWordPtr(isolate_, __ NoContextConstant(),
                                         input);
        break;
      case RegisterRepresentation::Float64():
        __ CallBuiltin_DebugPrintFloat64(isolate_, __ NoContextConstant(),
                                         input);
        break;
      default:
        // TODO(nicohartmann@): Support other representations.
        UNIMPLEMENTED();
    }
    return {};
  }

  OpIndex REDUCE(StaticAssert)(OpIndex condition, const char* source) {
    // Static asserts should be (statically asserted and) removed by turboshaft.
    UnparkedScopeIfNeeded scope(broker_);
    AllowHandleDereference allow_handle_dereference;
    std::cout << __ output_graph().Get(condition);
    FATAL(
        "Expected Turbofan static assert to hold, but got non-true input:\n  "
        "%s",
        source);
  }

  OpIndex REDUCE(CheckTurboshaftTypeOf)(OpIndex input,
                                        RegisterRepresentation rep, Type type,
                                        bool successful) {
    if (successful) return input;

    UnparkedScopeIfNeeded scope(broker_);
    AllowHandleDereference allow_handle_dereference;
    FATAL("Checking type %s of operation %d:%s failed!",
          type.ToString().c_str(), input.id(),
          __ output_graph().Get(input).ToString().c_str());
  }

 private:
  Isolate* isolate_ = PipelineData::Get().isolate();
  JSHeapBroker* broker_ = PipelineData::Get().broker();
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_DEBUG_FEATURE_LOWERING_REDUCER_H_
