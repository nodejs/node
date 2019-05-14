// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/v8.h"

#include "src/objects-inl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module-builder.h"

#include "test/common/wasm/test-signatures.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmModuleBuilderTest : public TestWithZone {
 protected:
  void AddLocal(WasmFunctionBuilder* f, ValueType type) {
    uint16_t index = f->AddLocal(type);
    f->EmitGetLocal(index);
  }
};

TEST_F(WasmModuleBuilderTest, Regression_647329) {
  // Test crashed with asan.
  ZoneBuffer buffer(zone());
  const size_t kSize = ZoneBuffer::kInitialSize * 3 + 4096 + 100;
  byte data[kSize] = {0};
  buffer.write(data, kSize);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
