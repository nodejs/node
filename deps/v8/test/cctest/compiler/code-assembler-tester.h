// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_CCTEST_COMPILER_CODE_ASSEMBLER_TESTER_H_
#define V8_TEST_CCTEST_COMPILER_CODE_ASSEMBLER_TESTER_H_

#include "src/compiler/code-assembler.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/handles.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class CodeAssemblerTester {
 public:
  // Test generating code for a stub. Assumes VoidDescriptor call interface.
  explicit CodeAssemblerTester(Isolate* isolate)
      : zone_(isolate->allocator(), ZONE_NAME),
        scope_(isolate),
        state_(isolate, &zone_, VoidDescriptor(isolate), Code::STUB, "test") {}

  // Test generating code for a JS function (e.g. builtins).
  CodeAssemblerTester(Isolate* isolate, int parameter_count,
                      Code::Kind kind = Code::BUILTIN)
      : zone_(isolate->allocator(), ZONE_NAME),
        scope_(isolate),
        state_(isolate, &zone_, parameter_count, kind, "test") {}

  CodeAssemblerTester(Isolate* isolate, Code::Kind kind)
      : zone_(isolate->allocator(), ZONE_NAME),
        scope_(isolate),
        state_(isolate, &zone_, 0, kind, "test") {}

  CodeAssemblerTester(Isolate* isolate, CallDescriptor* call_descriptor,
                      const char* name = "test")
      : zone_(isolate->allocator(), ZONE_NAME),
        scope_(isolate),
        state_(isolate, &zone_, call_descriptor, Code::STUB, name, 0, -1) {}

  CodeAssemblerState* state() { return &state_; }

  // Direct low-level access to the machine assembler, for testing only.
  RawMachineAssembler* raw_assembler_for_testing() {
    return state_.raw_assembler_.get();
  }

  Handle<Code> GenerateCode() { return CodeAssembler::GenerateCode(&state_); }

  Handle<Code> GenerateCodeCloseAndEscape() {
    return scope_.CloseAndEscape(GenerateCode());
  }

 private:
  Zone zone_;
  HandleScope scope_;
  LocalContext context_;
  CodeAssemblerState state_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_CCTEST_COMPILER_CODE_ASSEMBLER_TESTER_H_
