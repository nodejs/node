// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/v8.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/encoder.h"

#include "test/cctest/wasm/test-signatures.h"

namespace v8 {
namespace internal {
namespace wasm {

class EncoderTest : public TestWithZone {
 protected:
  void AddLocal(WasmFunctionBuilder* f, LocalType type) {
    uint16_t index = f->AddLocal(type);
    f->EmitGetLocal(index);
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8
