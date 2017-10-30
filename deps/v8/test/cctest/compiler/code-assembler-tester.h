// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-assembler.h"
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
        state_(isolate, &zone_, VoidDescriptor(isolate),
               Code::ComputeFlags(Code::STUB), "test") {}

  // Test generating code for a JS function (e.g. builtins).
  CodeAssemblerTester(Isolate* isolate, int parameter_count,
                      Code::Kind kind = Code::BUILTIN)
      : zone_(isolate->allocator(), ZONE_NAME),
        scope_(isolate),
        state_(isolate, &zone_, parameter_count, Code::ComputeFlags(kind),
               "test") {}

  // This constructor is intended to be used for creating code objects with
  // specific flags.
  CodeAssemblerTester(Isolate* isolate, Code::Flags flags)
      : zone_(isolate->allocator(), ZONE_NAME),
        scope_(isolate),
        state_(isolate, &zone_, 0, flags, "test") {}

  CodeAssemblerState* state() { return &state_; }

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
