// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "src/wasm/wasm-macro-gen.h"

#include "src/wasm/wasm-interpreter.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

namespace v8 {
namespace internal {
namespace wasm {

TEST(Run_WasmInt8Const_i) {
  WasmRunner<int32_t> r(kExecuteInterpreted);
  const byte kExpectedValue = 109;
  // return(kExpectedValue)
  BUILD(r, WASM_I8(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}

TEST(Run_WasmIfElse) {
  WasmRunner<int32_t> r(kExecuteInterpreted, MachineType::Int32());
  BUILD(r, WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I8(9), WASM_I8(10)));
  CHECK_EQ(10, r.Call(0));
  CHECK_EQ(9, r.Call(1));
}

TEST(Run_WasmIfReturn) {
  WasmRunner<int32_t> r(kExecuteInterpreted, MachineType::Int32());
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_RETURN1(WASM_I8(77))), WASM_I8(65));
  CHECK_EQ(65, r.Call(0));
  CHECK_EQ(77, r.Call(1));
}

TEST(Run_WasmNopsN) {
  const int kMaxNops = 10;
  byte code[kMaxNops + 2];
  for (int nops = 0; nops < kMaxNops; nops++) {
    byte expected = static_cast<byte>(20 + nops);
    memset(code, kExprNop, sizeof(code));
    code[nops] = kExprI8Const;
    code[nops + 1] = expected;

    WasmRunner<int32_t> r(kExecuteInterpreted);
    r.Build(code, code + nops + 2);
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmConstsN) {
  const int kMaxConsts = 10;
  byte code[kMaxConsts * 3];
  int32_t expected = 0;
  for (int count = 1; count < kMaxConsts; count++) {
    for (int i = 0; i < count; i++) {
      byte val = static_cast<byte>(count * 10 + i);
      code[i * 3] = kExprI8Const;
      code[i * 3 + 1] = val;
      if (i == (count - 1)) {
        code[i * 3 + 2] = kExprNop;
        expected = val;
      } else {
        code[i * 3 + 2] = kExprDrop;
      }
    }

    WasmRunner<int32_t> r(kExecuteInterpreted);
    r.Build(code, code + (count * 3));
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmBlocksN) {
  const int kMaxNops = 10;
  const int kExtra = 5;
  byte code[kMaxNops + kExtra];
  for (int nops = 0; nops < kMaxNops; nops++) {
    byte expected = static_cast<byte>(30 + nops);
    memset(code, kExprNop, sizeof(code));
    code[0] = kExprBlock;
    code[1] = kLocalI32;
    code[2 + nops] = kExprI8Const;
    code[2 + nops + 1] = expected;
    code[2 + nops + 2] = kExprEnd;

    WasmRunner<int32_t> r(kExecuteInterpreted);
    r.Build(code, code + nops + kExtra);
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmBlockBreakN) {
  const int kMaxNops = 10;
  const int kExtra = 6;
  byte code[kMaxNops + kExtra];
  for (int nops = 0; nops < kMaxNops; nops++) {
    // Place the break anywhere within the block.
    for (int index = 0; index < nops; index++) {
      memset(code, kExprNop, sizeof(code));
      code[0] = kExprBlock;
      code[1] = kLocalI32;
      code[sizeof(code) - 1] = kExprEnd;

      int expected = nops * 11 + index;
      code[2 + index + 0] = kExprI8Const;
      code[2 + index + 1] = static_cast<byte>(expected);
      code[2 + index + 2] = kExprBr;
      code[2 + index + 3] = 0;

      WasmRunner<int32_t> r(kExecuteInterpreted);
      r.Build(code, code + kMaxNops + kExtra);
      CHECK_EQ(expected, r.Call());
    }
  }
}

TEST(Run_Wasm_nested_ifs_i) {
  WasmRunner<int32_t> r(kExecuteInterpreted, MachineType::Int32(),
                        MachineType::Int32());

  BUILD(r, WASM_IF_ELSE_I(
               WASM_GET_LOCAL(0),
               WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_I8(11), WASM_I8(12)),
               WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_I8(13), WASM_I8(14))));

  CHECK_EQ(11, r.Call(1, 1));
  CHECK_EQ(12, r.Call(1, 0));
  CHECK_EQ(13, r.Call(0, 1));
  CHECK_EQ(14, r.Call(0, 0));
}

// Make tests more robust by not hard-coding offsets of various operations.
// The {Find} method finds the offsets for the given bytecodes, returning
// the offsets in an array.
std::unique_ptr<int[]> Find(byte* code, size_t code_size, int n, ...) {
  va_list vl;
  va_start(vl, n);

  std::unique_ptr<int[]> offsets(new int[n]);

  for (int i = 0; i < n; i++) {
    offsets[i] = -1;
  }

  int pos = 0;
  WasmOpcode current = static_cast<WasmOpcode>(va_arg(vl, int));
  for (size_t i = 0; i < code_size; i++) {
    if (code[i] == current) {
      offsets[pos++] = static_cast<int>(i);
      if (pos == n) break;
      current = static_cast<WasmOpcode>(va_arg(vl, int));
    }
  }
  va_end(vl);

  return offsets;
}

TEST(Breakpoint_I32Add) {
  static const int kLocalsDeclSize = 1;
  static const int kNumBreakpoints = 3;
  byte code[] = {WASM_I32_ADD(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
  std::unique_ptr<int[]> offsets =
      Find(code, sizeof(code), kNumBreakpoints, kExprGetLocal, kExprGetLocal,
           kExprI32Add);

  WasmRunner<int32_t> r(kExecuteInterpreted, MachineType::Uint32(),
                        MachineType::Uint32());

  r.Build(code, code + arraysize(code));

  WasmInterpreter* interpreter = r.interpreter();
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);
  for (int i = 0; i < kNumBreakpoints; i++) {
    interpreter->SetBreakpoint(r.function(), kLocalsDeclSize + offsets[i],
                               true);
  }

  FOR_UINT32_INPUTS(a) {
    for (uint32_t b = 11; b < 3000000000u; b += 1000000000u) {
      thread->Reset();
      WasmVal args[] = {WasmVal(*a), WasmVal(b)};
      thread->PushFrame(r.function(), args);

      for (int i = 0; i < kNumBreakpoints; i++) {
        thread->Run();  // run to next breakpoint
        // Check the thread stopped at the right pc.
        CHECK_EQ(WasmInterpreter::PAUSED, thread->state());
        CHECK_EQ(kLocalsDeclSize + offsets[i], thread->GetBreakpointPc());
      }

      thread->Run();  // run to completion

      // Check the thread finished with the right value.
      CHECK_EQ(WasmInterpreter::FINISHED, thread->state());
      uint32_t expected = (*a) + (b);
      CHECK_EQ(expected, thread->GetReturnValue().to<uint32_t>());
    }
  }
}

TEST(Step_I32Mul) {
  static const int kTraceLength = 4;
  byte code[] = {WASM_I32_MUL(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};

  WasmRunner<int32_t> r(kExecuteInterpreted, MachineType::Uint32(),
                        MachineType::Uint32());

  r.Build(code, code + arraysize(code));

  WasmInterpreter* interpreter = r.interpreter();
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);

  FOR_UINT32_INPUTS(a) {
    for (uint32_t b = 33; b < 3000000000u; b += 1000000000u) {
      thread->Reset();
      WasmVal args[] = {WasmVal(*a), WasmVal(b)};
      thread->PushFrame(r.function(), args);

      // Run instructions one by one.
      for (int i = 0; i < kTraceLength - 1; i++) {
        thread->Step();
        // Check the thread stopped.
        CHECK_EQ(WasmInterpreter::PAUSED, thread->state());
      }

      // Run last instruction.
      thread->Step();

      // Check the thread finished with the right value.
      CHECK_EQ(WasmInterpreter::FINISHED, thread->state());
      uint32_t expected = (*a) * (b);
      CHECK_EQ(expected, thread->GetReturnValue().to<uint32_t>());
    }
  }
}

TEST(Breakpoint_I32And_disable) {
  static const int kLocalsDeclSize = 1;
  static const int kNumBreakpoints = 1;
  byte code[] = {WASM_I32_AND(WASM_GET_LOCAL(0), WASM_GET_LOCAL(1))};
  std::unique_ptr<int[]> offsets =
      Find(code, sizeof(code), kNumBreakpoints, kExprI32And);

  WasmRunner<int32_t> r(kExecuteInterpreted, MachineType::Uint32(),
                        MachineType::Uint32());

  r.Build(code, code + arraysize(code));

  WasmInterpreter* interpreter = r.interpreter();
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);

  FOR_UINT32_INPUTS(a) {
    for (uint32_t b = 11; b < 3000000000u; b += 1000000000u) {
      // Run with and without breakpoints.
      for (int do_break = 0; do_break < 2; do_break++) {
        interpreter->SetBreakpoint(r.function(), kLocalsDeclSize + offsets[0],
                                   do_break);
        thread->Reset();
        WasmVal args[] = {WasmVal(*a), WasmVal(b)};
        thread->PushFrame(r.function(), args);

        if (do_break) {
          thread->Run();  // run to next breakpoint
          // Check the thread stopped at the right pc.
          CHECK_EQ(WasmInterpreter::PAUSED, thread->state());
          CHECK_EQ(kLocalsDeclSize + offsets[0], thread->GetBreakpointPc());
        }

        thread->Run();  // run to completion

        // Check the thread finished with the right value.
        CHECK_EQ(WasmInterpreter::FINISHED, thread->state());
        uint32_t expected = (*a) & (b);
        CHECK_EQ(expected, thread->GetReturnValue().to<uint32_t>());
      }
    }
  }
}

TEST(GrowMemory) {
  TestingModule module(kExecuteInterpreted);
  WasmRunner<int32_t> r(&module, MachineType::Uint32());
  module.AddMemory(WasmModule::kPageSize);
  BUILD(r, WASM_GROW_MEMORY(WASM_GET_LOCAL(0)));
  CHECK_EQ(1, r.Call(1));
}

TEST(GrowMemoryPreservesData) {
  int32_t index = 16;
  int32_t value = 2335;
  TestingModule module(kExecuteInterpreted);
  WasmRunner<int32_t> r(&module, MachineType::Uint32());
  module.AddMemory(WasmModule::kPageSize);
  BUILD(r, WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                          WASM_I32V(value)),
        WASM_GROW_MEMORY(WASM_GET_LOCAL(0)), WASM_DROP,
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index)));
  CHECK_EQ(value, r.Call(1));
}

TEST(GrowMemoryInvalidSize) {
  {
    // Grow memory by an invalid amount without initial memory.
    TestingModule module(kExecuteInterpreted);
    WasmRunner<int32_t> r(&module, MachineType::Uint32());
    BUILD(r, WASM_GROW_MEMORY(WASM_GET_LOCAL(0)));
    CHECK_EQ(-1, r.Call(1048575));
  }
  {
    // Grow memory by an invalid amount without initial memory.
    TestingModule module(kExecuteInterpreted);
    WasmRunner<int32_t> r(&module, MachineType::Uint32());
    module.AddMemory(WasmModule::kPageSize);
    BUILD(r, WASM_GROW_MEMORY(WASM_GET_LOCAL(0)));
    CHECK_EQ(-1, r.Call(1048575));
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
