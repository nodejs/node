// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/instruction-selector-tester.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

#if V8_TURBOFAN_TARGET

TEST(InstructionSelectionReturnZero) {
  InstructionSelectorTester m;
  m.Return(m.Int32Constant(0));
  m.SelectInstructions(InstructionSelectorTester::kInternalMode);
  CHECK_EQ(2, static_cast<int>(m.code.size()));
  CHECK_EQ(kArchNop, m.code[0]->opcode());
  CHECK_EQ(kArchRet, m.code[1]->opcode());
  CHECK_EQ(1, static_cast<int>(m.code[1]->InputCount()));
}

#endif  // !V8_TURBOFAN_TARGET
