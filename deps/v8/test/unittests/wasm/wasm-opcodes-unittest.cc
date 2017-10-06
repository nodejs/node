// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmOpcodesTest : public TestWithZone {};

TEST_F(WasmOpcodesTest, EveryOpcodeHasAName) {
  static const struct {
    WasmOpcode opcode;
    const char* debug_name;
  } kValues[] = {
#define DECLARE_ELEMENT(name, opcode, sig) {kExpr##name, "kExpr" #name},
      FOREACH_OPCODE(DECLARE_ELEMENT)};

  for (size_t i = 0; i < arraysize(kValues); i++) {
    const char* result = WasmOpcodes::OpcodeName(kValues[i].opcode);
    if (strcmp("unknown", result) == 0) {
      EXPECT_TRUE(false) << "WasmOpcodes::OpcodeName(" << kValues[i].debug_name
                         << ") == \"unknown\";"
                            " plazz halp in src/wasm/wasm-opcodes.cc";
    }
  }
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
