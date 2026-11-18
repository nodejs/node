// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_BYTECODE_VERIFIER_H_
#define V8_SANDBOX_BYTECODE_VERIFIER_H_

#include "src/objects/bytecode-array.h"
#include "src/sandbox/isolate.h"
#include "src/zone/zone.h"

namespace v8::internal {

// Bytecode verifier to ensure bytecode is safe to execute.
//
// The verifier's scope is sandbox safety only: it guarantees that executing
// the bytecode cannot escape the sandbox. Light verification covers
// control-flow integrity, full verification additionally covers that
// registers and parameters don't refer to memory outside of the sandbox.
// Bytecode that is semantically incorrect but still cannot escape the
// sandbox (for example a register that the bytecode generator would never
// emit at that place) is intentionally accepted if it can never be generated
// in production, i.e. if it can only be constructed by installing arbitrary
// bytecode. Semantically incorrect bytecode that our own code can generate
// is a bug and should be fixed, even if it is sandbox-safe.
//
// See the design document for more details:
// https://docs.google.com/document/d/1UUooVKUvf1zDobG34VDVuLsjoKZd-CeSuhvBcLysc7U/edit?usp=sharing
class V8_EXPORT_PRIVATE BytecodeVerifier {
 public:
  // Verifies the given bytecode to guarantee certain security properties with
  // regards to the sandbox. If verification fails, this will crash the process.
  static void Verify(IsolateForSandbox isolate, Handle<BytecodeArray> bytecode,
                     Zone* zone);

 private:
  friend class BytecodeVerifierTest;

  static void VerifyLight(IsolateForSandbox isolate,
                          Handle<BytecodeArray> bytecode, Zone* zone);
  static void VerifyFull(IsolateForSandbox isolate,
                         Handle<BytecodeArray> bytecode, Zone* zone);

  static bool IsAllowedRuntimeFunction(Runtime::FunctionId id);

  [[noreturn]] static void FailVerification(const char* reason) {
    // If desired, we could log more information here, such as the current
    // bytecode and its offset. This might require making the BytecodeVerifier
    // a non-static class with the BytecodeArrayIterator as a member, so that
    // we can access it here.
    FATAL("Bytecode verification failed: %s", reason);
  }

  static inline void Check(bool condition, const char* error_msg) {
    if (!condition) [[unlikely]] {
      FailVerification(error_msg);
    }
  }
};

}  // namespace v8::internal

#endif  // V8_SANDBOX_BYTECODE_VERIFIER_H_
