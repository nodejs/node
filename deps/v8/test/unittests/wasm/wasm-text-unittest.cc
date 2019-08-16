// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "test/unittests/test-utils.h"

#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module-builder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-text.h"
#include "test/common/wasm/test-signatures.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmTextTest : public TestWithIsolateAndZone {
 public:
  TestSignatures sigs;
  WasmFeatures enabled_features_;

  void TestInstruction(const byte* func_start, size_t func_size) {
    WasmModuleBuilder mb(zone());
    auto* fb = mb.AddFunction(sigs.v_v());
    fb->EmitCode(func_start, static_cast<uint32_t>(func_size));
    fb->Emit(kExprEnd);

    ZoneBuffer buffer(zone());
    mb.WriteTo(&buffer);

    ModuleWireBytes wire_bytes(
        Vector<const byte>(buffer.begin(), buffer.size()));

    ModuleResult result = DecodeWasmModule(
        enabled_features_, buffer.begin(), buffer.end(), false, kWasmOrigin,
        isolate()->counters(), isolate()->wasm_engine()->allocator());
    EXPECT_TRUE(result.ok());

    std::stringstream ss;
    PrintWasmText(result.value().get(), wire_bytes, 0, ss, nullptr);
  }
};

TEST_F(WasmTextTest, EveryOpcodeCanBeDecoded) {
  static const struct {
    WasmOpcode opcode;
    const char* debug_name;
  } kValues[] = {
#define DECLARE_ELEMENT(name, opcode, sig) {kExpr##name, "kExpr" #name},
      FOREACH_OPCODE(DECLARE_ELEMENT)};
#undef DECLARE_ELEMENT

  for (const auto& value : kValues) {
    // Pad with 0 for any immediate values. If they're not needed, they'll be
    // interpreted as unreachable.
    byte data[20] = {0};

    printf("%s\n", value.debug_name);
    switch (value.opcode) {
      // Instructions that have a special case because they affect the control
      // depth.
      case kExprBlock:
      case kExprLoop:
      case kExprIf:
      case kExprTry:
        data[0] = value.opcode;
        data[1] = kLocalVoid;
        data[2] = kExprEnd;
        break;
      case kExprElse:
        data[0] = kExprIf;
        data[1] = value.opcode;
        data[2] = kExprEnd;
        break;
      case kExprCatch:
        data[0] = kExprTry;
        data[1] = value.opcode;
        data[2] = kExprEnd;
        break;
      case kExprEnd:
        break;

      // Instructions with special requirements for immediates.
      case kExprSelectWithType:
        data[0] = kExprSelectWithType;
        data[1] = 1;
        data[2] = kLocalI32;
        break;

      default: {
        if (value.opcode >= 0x100) {
          data[0] = value.opcode >> 8;        // Prefix byte.
          byte opcode = value.opcode & 0xff;  // Actual opcode.
          if (opcode >= 0x80) {
            // Opcode with prefix, and needs to be LEB encoded (3 bytes).
            // For now, this can only be in the range [0x80, 0xff], which means
            // that the third byte is always 1.
            data[1] = (opcode & 0x7f) | 0x80;
            data[2] = 1;
          } else {
            // Opcode with prefix (2 bytes).
            data[1] = opcode;
          }
        } else {
          // Single-byte opcode.
          data[0] = value.opcode;
        }
        break;
      }
    }

    TestInstruction(data, arraysize(data));
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
