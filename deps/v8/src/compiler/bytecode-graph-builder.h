// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
#define V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_

#include "src/compiler/js-operator.h"
#include "src/compiler/js-type-hint-lowering.h"
#include "src/handles/handles.h"
#include "src/objects/code-kind.h"
#include "src/utils/utils.h"

namespace v8 {

class TickCounter;

namespace internal {

class BytecodeArray;
class FeedbackVector;
class SharedFunctionInfo;
class Zone;

namespace compiler {

class JSGraph;
class SourcePositionTable;

enum class BytecodeGraphBuilderFlag : uint8_t {
  kSkipFirstStackCheck = 1 << 0,
  // TODO(neis): Remove liveness flag here when concurrent inlining is always
  // on, because then the serializer will be the only place where we perform
  // bytecode analysis.
  kAnalyzeEnvironmentLiveness = 1 << 1,
  kBailoutOnUninitialized = 1 << 2,
};
using BytecodeGraphBuilderFlags = base::Flags<BytecodeGraphBuilderFlag>;

// Note: {invocation_frequency} is taken by reference to work around a GCC bug
// on AIX (v8:8193).
void BuildGraphFromBytecode(JSHeapBroker* broker, Zone* local_zone,
                            SharedFunctionInfoRef const& shared_info,
                            FeedbackCellRef const& feedback_cell,
                            BailoutId osr_offset, JSGraph* jsgraph,
                            CallFrequency const& invocation_frequency,
                            SourcePositionTable* source_positions,
                            int inlining_id, CodeKind code_kind,
                            BytecodeGraphBuilderFlags flags,
                            TickCounter* tick_counter);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BYTECODE_GRAPH_BUILDER_H_
