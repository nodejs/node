// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/deoptimizer/deoptimizer.h"

extern "C" {
void Builtins_InterpreterEnterAtBytecode();
void Builtins_InterpreterEnterAtNextBytecode();
void Builtins_ContinueToCodeStubBuiltinWithResult();
void Builtins_ContinueToCodeStubBuiltin();
void Builtins_ContinueToJavaScriptBuiltinWithResult();
void Builtins_ContinueToJavaScriptBuiltin();
void construct_stub_create_deopt_addr();
void construct_stub_invoke_deopt_addr();
void Builtins_BaselineOrInterpreterEnterAtBytecode();
void Builtins_BaselineOrInterpreterEnterAtNextBytecode();
typedef void (*function_ptr)();
}

namespace v8 {
namespace internal {

// List of allowed builtin addresses that we can return to in the deoptimizer.
constexpr function_ptr builtins[] = {
    &Builtins_InterpreterEnterAtBytecode,
    &Builtins_InterpreterEnterAtNextBytecode,
    &Builtins_ContinueToCodeStubBuiltinWithResult,
    &Builtins_ContinueToCodeStubBuiltin,
    &Builtins_ContinueToJavaScriptBuiltinWithResult,
    &Builtins_ContinueToJavaScriptBuiltin,
    &construct_stub_create_deopt_addr,
    &construct_stub_invoke_deopt_addr,
    &Builtins_BaselineOrInterpreterEnterAtBytecode,
    &Builtins_BaselineOrInterpreterEnterAtNextBytecode,
};

bool Deoptimizer::IsValidReturnAddress(Address address) {
  for (function_ptr builtin : builtins) {
    if (address == FUNCTION_ADDR(builtin)) {
      return true;
    }
  }
  return false;
}

}  // namespace internal
}  // namespace v8
