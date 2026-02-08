// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wrappers.h"

#include <bit>
#include <optional>

#include "src/base/small-vector.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/turboshaft/dataview-lowering-reducer.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/select-lowering-reducer.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/execution/isolate-data.h"
#include "src/objects/object-list-macros.h"
#include "src/wasm/turboshaft-graph-interface-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wrappers-inl.h"
#include "src/zone/zone.h"

namespace v8::internal::wasm {

const compiler::turboshaft::TSCallDescriptor* GetBuiltinCallDescriptor(
    Builtin name, Zone* zone) {
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
  return compiler::turboshaft::TSCallDescriptor::Create(
      call_desc, compiler::CanThrow::kNo, compiler::LazyDeoptOnThrow::kNo,
      zone);
}

void BuildWasmWrapper(compiler::turboshaft::PipelineData* data,
                      AccountingAllocator* allocator,
                      compiler::turboshaft::Graph& graph,
                      const CanonicalSig* sig,
                      WrapperCompilationInfo wrapper_info) {
  Zone zone(allocator, ZONE_NAME);
  using Assembler = compiler::turboshaft::Assembler<
      compiler::turboshaft::SelectLoweringReducer,
      compiler::turboshaft::DataViewLoweringReducer,
      compiler::turboshaft::VariableReducer>;
  Assembler assembler(data, graph, graph, &zone);
  WasmWrapperTSGraphBuilder<Assembler> builder(&zone, assembler, sig,
                                               /*is_inlining_into_js*/ false);
  if (wrapper_info.code_kind == CodeKind::JS_TO_WASM_FUNCTION) {
    builder.BuildJSToWasmWrapper(wrapper_info.receiver_is_first_param);
  } else if (wrapper_info.code_kind == CodeKind::WASM_TO_JS_FUNCTION) {
    builder.BuildWasmToJSWrapper(wrapper_info.import_kind,
                                 wrapper_info.expected_arity,
                                 wrapper_info.suspend);
  } else if (wrapper_info.code_kind == CodeKind::WASM_TO_CAPI_FUNCTION) {
    builder.BuildCapiCallWrapper();
  } else if (wrapper_info.code_kind == CodeKind::WASM_STACK_ENTRY) {
    builder.BuildWasmStackEntryWrapper();
  } else {
    // TODO(thibaudm): Port remaining wrappers.
    UNREACHABLE();
  }
}

}  // namespace v8::internal::wasm
