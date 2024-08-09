// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/codegen/turboshaft-builtins-assembler-inl.h"

namespace v8::internal {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using namespace compiler::turboshaft;  // NOLINT(build/namespaces)

template <typename Next>
class StringBuiltinsReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(StringBuiltins)
};

class StringBuiltinsAssemblerTS
    : public TurboshaftBuiltinsAssembler<StringBuiltinsReducer> {
 public:
  StringBuiltinsAssemblerTS(PipelineData* data,
                            compiler::turboshaft::Graph& graph,
                            Zone* phase_zone)
      : TurboshaftBuiltinsAssembler(data, graph, phase_zone) {}
};

#ifdef V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS

TS_BUILTIN(StringFromCodePointAt, StringBuiltinsAssemblerTS) {
  auto receiver = __ Parameter<String>(Descriptor::kReceiver);
  auto position = __ Parameter<WordPtr>(Descriptor::kPosition);

  // Load the character code at the {position} from the {receiver}.
  V<Word32> codepoint =
      LoadSurrogatePairAt(receiver, {}, position, UnicodeEncoding::UTF16);
  // Create a String from the UTF16 encoded code point
  V<String> result =
      StringFromSingleCodePoint(codepoint, UnicodeEncoding::UTF16);
  Return(result);
}

#endif  // V8_ENABLE_EXPERIMENTAL_TSA_BUILTINS

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal
