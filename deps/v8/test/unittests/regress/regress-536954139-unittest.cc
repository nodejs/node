// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/vector.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

using DisasmX64RegressTest = TestWithContext;

TEST_F(DisasmX64RegressTest, Regress536954139) {
  // `62 F2 75 08 39 C2` should disassemble as `vpminsd`, not `vpminsq`.
  uint8_t code[] = {0x62, 0xF2, 0x75, 0x08, 0x39, 0xC2};

  disasm::NameConverter converter;
  disasm::Disassembler disassembler(converter);

  v8::base::EmbeddedVector<char, 128> buffer;
  buffer[0] = '\0';
  disassembler.InstructionDecode(buffer, code);

  // The disassembled string should contain "vpminsd".
  EXPECT_NE(nullptr, strstr(buffer.begin(), "vpminsd")) << buffer.begin();
}

}  // namespace internal
}  // namespace v8
