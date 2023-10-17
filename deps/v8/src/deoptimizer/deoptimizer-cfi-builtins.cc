// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/snapshot/embedded/embedded-data.h"

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
void Builtins_RestartFrameTrampoline();
typedef void (*function_ptr)();
}

namespace v8 {
namespace internal {

extern "C" const uint8_t v8_Default_embedded_blob_code_[];
extern "C" uint32_t v8_Default_embedded_blob_code_size_;

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
    &Builtins_RestartFrameTrampoline,
};

bool Deoptimizer::IsValidReturnAddress(Address address, Isolate* isolate) {
  EmbeddedData d = EmbeddedData::FromBlobForPc(isolate, address);
  Address code_start = reinterpret_cast<Address>(d.code());
  Address offset = address - code_start;
  if (offset >= v8_Default_embedded_blob_code_size_) return false;
  Address blob_start =
      reinterpret_cast<Address>(v8_Default_embedded_blob_code_);
  Address original_address = blob_start + offset;
  for (function_ptr builtin : builtins) {
    if (original_address == FUNCTION_ADDR(builtin)) {
      return true;
    }
  }
  return false;
}

}  // namespace internal
}  // namespace v8
