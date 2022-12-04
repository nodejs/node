// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/assembler-helper-arm.h"

#include "src/codegen/macro-assembler.h"
#include "src/execution/isolate-inl.h"

namespace v8 {
namespace internal {

Handle<Code> AssembleCodeImpl(Isolate* isolate,
                              std::function<void(MacroAssembler&)> assemble) {
  MacroAssembler assm(isolate, CodeObjectRequired::kYes);

  assemble(assm);
  assm.bx(lr);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  if (v8_flags.print_code) {
    code->Print();
  }
  return code;
}

}  // namespace internal
}  // namespace v8
