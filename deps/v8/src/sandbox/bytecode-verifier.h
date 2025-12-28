// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_BYTECODE_VERIFIER_H_
#define V8_SANDBOX_BYTECODE_VERIFIER_H_

#include "src/objects/bytecode-array.h"
#include "src/sandbox/isolate.h"

namespace v8::internal {

class V8_EXPORT_PRIVATE BytecodeVerifier {
 public:
  // Verifies the given bytecode to guarantee certain security properties with
  // regards to the sandbox. If verification fails, this will crash the process.
  static void Verify(IsolateForSandbox isolate, Handle<BytecodeArray> bytecode);

 private:
  static void VerifyLight(IsolateForSandbox isolate,
                          Handle<BytecodeArray> bytecode);
  static void VerifyFull(IsolateForSandbox isolate,
                         Handle<BytecodeArray> bytecode);
};

}  // namespace v8::internal

#endif  // V8_SANDBOX_BYTECODE_VERIFIER_H_
