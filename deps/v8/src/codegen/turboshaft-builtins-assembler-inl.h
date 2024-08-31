// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_TURBOSHAFT_BUILTINS_ASSEMBLER_INL_H_
#define V8_CODEGEN_TURBOSHAFT_BUILTINS_ASSEMBLER_INL_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/machine-lowering-reducer-inl.h"

namespace v8::internal {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

template <template <typename> typename... Reducers>
class TurboshaftBuiltinsAssembler
    : public compiler::turboshaft::TSAssembler<
          Reducers..., compiler::turboshaft::MachineLoweringReducer,
          compiler::turboshaft::VariableReducer> {
  using Base = compiler::turboshaft::TSAssembler<
      Reducers..., compiler::turboshaft::MachineLoweringReducer,
      compiler::turboshaft::VariableReducer>;

 public:
  template <typename T>
  using V = compiler::turboshaft::V<T>;

  TurboshaftBuiltinsAssembler(compiler::turboshaft::PipelineData* data,
                              compiler::turboshaft::Graph& graph,
                              Zone* phase_zone)
      : Base(data, graph, graph, phase_zone) {}
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal

#endif  // V8_CODEGEN_TURBOSHAFT_BUILTINS_ASSEMBLER_INL_H_
