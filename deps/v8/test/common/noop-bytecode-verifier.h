// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_COMMON_NOOP_BYTECODE_VERIFIER_H_
#define V8_TEST_COMMON_NOOP_BYTECODE_VERIFIER_H_

#include "src/objects/bytecode-array-inl.h"
#include "src/sandbox/isolate.h"

namespace v8 {
namespace internal {

// A no-op bytecode verifier that doesn't perform any validation.
// This must only be used for testing purposes.
class NoOpBytecodeVerifier {
 public:
  static void Verify(IsolateForSandbox isolate,
                     Handle<BytecodeArray> bytecode) {
    bytecode->MarkVerified(isolate);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TEST_COMMON_NOOP_BYTECODE_VERIFIER_H_
