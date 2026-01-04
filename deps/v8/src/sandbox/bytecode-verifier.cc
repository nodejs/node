// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/bytecode-verifier.h"

#include "src/flags/flags.h"
#include "src/objects/bytecode-array-inl.h"

namespace v8::internal {

// static
void BytecodeVerifier::Verify(IsolateForSandbox isolate,
                              Handle<BytecodeArray> bytecode) {
  if (v8_flags.verify_bytecode_full) {
    VerifyFull(isolate, bytecode);
  } else if (v8_flags.verify_bytecode_light) {
    VerifyLight(isolate, bytecode);
  }

  bytecode->MarkVerified(isolate);
}

// static
void BytecodeVerifier::VerifyLight(IsolateForSandbox isolate,
                                   Handle<BytecodeArray> bytecode) {
  // VerifyLight is meant to catch the most important issues (in particular,
  // ones that we've seen in the past) and should be lightweight enough to be
  // enabled by default.

  // TODO(461681036): Implement this.
}

// static
void BytecodeVerifier::VerifyFull(IsolateForSandbox isolate,
                                  Handle<BytecodeArray> bytecode) {
  // VerifyFull does full verification and is for now just used during fuzzing
  // (to test the verification). However, in the future it may also (sometimes)
  // be enabled in production as well.

  // TODO(461681036): Implement this.
}

}  // namespace v8::internal
