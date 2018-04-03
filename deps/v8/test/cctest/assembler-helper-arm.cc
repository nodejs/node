// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/assembler-helper-arm.h"

#include "src/assembler-inl.h"
#include "src/isolate-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

Handle<Code> AssembleCodeImpl(std::function<void(Assembler&)> assemble) {
  Isolate* isolate = CcTest::i_isolate();
  Assembler assm(isolate, nullptr, 0);

  assemble(assm);
  assm.bx(lr);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
  if (FLAG_print_code) {
    code->Print();
  }
  return code;
}

}  // namespace internal
}  // namespace v8
