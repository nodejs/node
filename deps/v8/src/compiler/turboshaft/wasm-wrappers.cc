// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-wrappers.h"

#include "src/codegen/interface-descriptors-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/turboshaft/dataview-lowering-reducer.h"
#include "src/compiler/turboshaft/fast-api-call-lowering-reducer.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/compiler/turboshaft/wasm-wrappers-inl.h"
#include "src/wasm/turboshaft-graph-interface-inl.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/zone.h"

namespace v8::internal::compiler::turboshaft {

const TSCallDescriptor* GetBuiltinCallDescriptor(Builtin name, Zone* zone) {
  CallInterfaceDescriptor interface_descriptor =
      Builtins::CallInterfaceDescriptorFor(name);
  compiler::CallDescriptor* call_desc =
      compiler::Linkage::GetStubCallDescriptor(
          zone,                  // zone
          interface_descriptor,  // descriptor
          interface_descriptor
              .GetStackParameterCount(),       // stack parameter count
          compiler::CallDescriptor::kNoFlags,  // flags
          compiler::Operator::kNoProperties,   // properties
          StubCallMode::kCallBuiltinPointer);  // stub call mode
  return TSCallDescriptor::Create(call_desc, compiler::CanThrow::kNo,
                                  compiler::LazyDeoptOnThrow::kNo, zone);
}

void BuildWasmWrapper(PipelineData* data, Graph& graph,
                      const wasm::CanonicalSig* sig,
                      wasm::WrapperCompilationInfo wrapper_info,
                      DirectHandle<JSReceiver> callable) {
  Zone zone(data->allocator(), ZONE_NAME);
  using WrapperAssembler =
      Assembler<SelectLoweringReducer, DataViewLoweringReducer,
                FastApiCallLoweringReducer, VariableReducer>;
  WrapperAssembler assembler(data, graph, graph, &zone);
  WasmWrapperTSGraphBuilder<WrapperAssembler> builder(
      &zone, assembler, sig, /*is_inlining_into_js*/ false);
  switch (wrapper_info.code_kind) {
    case CodeKind::JS_TO_WASM_FUNCTION:
      return builder.BuildJSToWasmWrapper();
    case CodeKind::WASM_TO_JS_FUNCTION:
      if (wrapper_info.import_kind == wasm::ImportCallKind::kWasmToJSFastApi) {
        return builder.BuildJSFastApiCallWrapper(callable);
      }
      return builder.BuildWasmToJSWrapper(wrapper_info.import_kind,
                                          wrapper_info.expected_arity,
                                          wrapper_info.suspend);
    case CodeKind::WASM_TO_CAPI_FUNCTION:
      return builder.BuildCapiCallWrapper();
    case CodeKind::C_WASM_ENTRY:
      return builder.BuildCWasmEntryWrapper();
    case CodeKind::WASM_STACK_ENTRY:
      return builder.BuildWasmStackEntryWrapper();
    default:
      UNREACHABLE();
  }
}

}  // namespace v8::internal::compiler::turboshaft
