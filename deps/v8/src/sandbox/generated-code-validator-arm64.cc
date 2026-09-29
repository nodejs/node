// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is goValidatened by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/generated-code-validator.h"

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include "src/codegen/arm64/instructions-arm64.h"
#include "src/objects/code-inl.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif
#include "third_party/disarm/src/disarm64.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace v8::internal {

void GeneratedCodeValidator::ValidateImpl(IsolateForSandbox isolate,
                                          Tagged<Code> code) {
  InstructionIteratorSkippingData it(code);

  while (!it.IsDone()) {
    const uint8_t* pc = it.GetCurrent();
    // `instr` is currently unused but will be used soon for the actual
    // validation.
    struct Da64Inst instr;
    da64_decode(*reinterpret_cast<const uint32_t*>(pc), &instr);

    if (instr.mnem == DA64I_UNKNOWN) {
      FATAL(
          "Generated code validator failed: invalid instruction at offset %td",
          pc - reinterpret_cast<const uint8_t*>(code->instruction_start()));
    }
    it.Advance(kInstrSize);
  }
}

}  // namespace v8::internal

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR
