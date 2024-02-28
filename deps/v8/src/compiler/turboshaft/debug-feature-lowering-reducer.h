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
    if (isolate_ != nullptr) {
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
    } else {
#if V8_ENABLE_WEBASSEMBLY
      DCHECK(PipelineData::Get().is_wasm());
      V<WasmInstanceObject> instance_node = __ WasmInstanceParameter();
      V<Tagged> native_context =
          __ Load(instance_node, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::TaggedPointer(),
                  WasmInstanceObject::kNativeContextOffset);
      switch (rep.value()) {
        case RegisterRepresentation::Float64():
          __ CallBuiltin(Builtin::kDebugPrintFloat64, {input, native_context},
                         Operator::kNoProperties);
          break;
        case RegisterRepresentation::PointerSized():
          __ CallBuiltin(Builtin::kDebugPrintWordPtr, {input, native_context},
                         Operator::kNoProperties);
          break;
        default:
          // TODO(mliedtke): Support other representations.
          UNIMPLEMENTED();
      }
#else
      UNREACHABLE();
#endif
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
