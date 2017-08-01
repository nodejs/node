// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <memory>

#include "src/assembler-inl.h"
#include "src/wasm/wasm-interpreter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

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
  BUILD(r, WASM_I32V_2(kExpectedValue));
  CHECK_EQ(kExpectedValue, r.Call());
}

TEST(Run_WasmIfElse) {
  WasmRunner<int32_t, int32_t> r(kExecuteInterpreted);
  BUILD(r, WASM_IF_ELSE_I(WASM_GET_LOCAL(0), WASM_I32V_1(9), WASM_I32V_1(10)));
  CHECK_EQ(10, r.Call(0));
  CHECK_EQ(9, r.Call(1));
}

TEST(Run_WasmIfReturn) {
  WasmRunner<int32_t, int32_t> r(kExecuteInterpreted);
  BUILD(r, WASM_IF(WASM_GET_LOCAL(0), WASM_RETURN1(WASM_I32V_2(77))),
        WASM_I32V_2(65));
  CHECK_EQ(65, r.Call(0));
  CHECK_EQ(77, r.Call(1));
}

TEST(Run_WasmNopsN) {
  const int kMaxNops = 10;
  byte code[kMaxNops + 2];
  for (int nops = 0; nops < kMaxNops; nops++) {
    byte expected = static_cast<byte>(20 + nops);
    memset(code, kExprNop, sizeof(code));
    code[nops] = kExprI32Const;
    code[nops + 1] = expected;

    WasmRunner<int32_t> r(kExecuteInterpreted);
    r.Build(code, code + nops + 2);
    CHECK_EQ(expected, r.Call());
  }
}

TEST(Run_WasmConstsN) {
  const int kMaxConsts = 5;
  byte code[kMaxConsts * 3];
  int32_t expected = 0;
  for (int count = 1; count < kMaxConsts; count++) {
    for (int i = 0; i < count; i++) {
      byte val = static_cast<byte>(count * 10 + i);
      code[i * 3] = kExprI32Const;
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
    code[2 + nops] = kExprI32Const;
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
  int run = 0;
  byte code[kMaxNops + kExtra];
  for (int nops = 0; nops < kMaxNops; nops++) {
    // Place the break anywhere within the block.
    for (int index = 0; index < nops; index++) {
      memset(code, kExprNop, sizeof(code));
      code[0] = kExprBlock;
      code[1] = kLocalI32;
      code[sizeof(code) - 1] = kExprEnd;

      int expected = run++;
      code[2 + index + 0] = kExprI32Const;
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
  WasmRunner<int32_t, int32_t, int32_t> r(kExecuteInterpreted);

  BUILD(
      r,
      WASM_IF_ELSE_I(
          WASM_GET_LOCAL(0),
          WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_I32V_1(11), WASM_I32V_1(12)),
          WASM_IF_ELSE_I(WASM_GET_LOCAL(1), WASM_I32V_1(13), WASM_I32V_1(14))));

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

  WasmRunner<int32_t, uint32_t, uint32_t> r(kExecuteInterpreted);

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
      thread->InitFrame(r.function(), args);

      for (int i = 0; i < kNumBreakpoints; i++) {
        thread->Run();  // run to next breakpoint
        // Check the thread stopped at the right pc.
        CHECK_EQ(WasmInterpreter::PAUSED, thread->state());
        CHECK_EQ(static_cast<size_t>(kLocalsDeclSize + offsets[i]),
                 thread->GetBreakpointPc());
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

  WasmRunner<int32_t, uint32_t, uint32_t> r(kExecuteInterpreted);

  r.Build(code, code + arraysize(code));

  WasmInterpreter* interpreter = r.interpreter();
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);

  FOR_UINT32_INPUTS(a) {
    for (uint32_t b = 33; b < 3000000000u; b += 1000000000u) {
      thread->Reset();
      WasmVal args[] = {WasmVal(*a), WasmVal(b)};
      thread->InitFrame(r.function(), args);

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

  WasmRunner<int32_t, uint32_t, uint32_t> r(kExecuteInterpreted);

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
        thread->InitFrame(r.function(), args);

        if (do_break) {
          thread->Run();  // run to next breakpoint
          // Check the thread stopped at the right pc.
          CHECK_EQ(WasmInterpreter::PAUSED, thread->state());
          CHECK_EQ(static_cast<size_t>(kLocalsDeclSize + offsets[0]),
                   thread->GetBreakpointPc());
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
  {
    WasmRunner<int32_t, uint32_t> r(kExecuteInterpreted);
    r.module().AddMemory(WasmModule::kPageSize);
    r.module().SetMaxMemPages(10);
    BUILD(r, WASM_GROW_MEMORY(WASM_GET_LOCAL(0)));
    CHECK_EQ(1, r.Call(1));
  }
  {
    WasmRunner<int32_t, uint32_t> r(kExecuteInterpreted);
    r.module().AddMemory(WasmModule::kPageSize);
    r.module().SetMaxMemPages(10);
    BUILD(r, WASM_GROW_MEMORY(WASM_GET_LOCAL(0)));
    CHECK_EQ(-1, r.Call(11));
  }
}

TEST(GrowMemoryPreservesData) {
  int32_t index = 16;
  int32_t value = 2335;
  WasmRunner<int32_t, uint32_t> r(kExecuteInterpreted);
  r.module().AddMemory(WasmModule::kPageSize);
  BUILD(r, WASM_STORE_MEM(MachineType::Int32(), WASM_I32V(index),
                          WASM_I32V(value)),
        WASM_GROW_MEMORY(WASM_GET_LOCAL(0)), WASM_DROP,
        WASM_LOAD_MEM(MachineType::Int32(), WASM_I32V(index)));
  CHECK_EQ(value, r.Call(1));
}

TEST(GrowMemoryInvalidSize) {
  // Grow memory by an invalid amount without initial memory.
  WasmRunner<int32_t, uint32_t> r(kExecuteInterpreted);
  r.module().AddMemory(WasmModule::kPageSize);
  BUILD(r, WASM_GROW_MEMORY(WASM_GET_LOCAL(0)));
  CHECK_EQ(-1, r.Call(1048575));
}

TEST(TestPossibleNondeterminism) {
  {
    WasmRunner<int32_t, float> r(kExecuteInterpreted);
    BUILD(r, WASM_I32_REINTERPRET_F32(WASM_GET_LOCAL(0)));
    r.Call(1048575.5f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    WasmRunner<int64_t, double> r(kExecuteInterpreted);
    BUILD(r, WASM_I64_REINTERPRET_F64(WASM_GET_LOCAL(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    WasmRunner<float, float> r(kExecuteInterpreted);
    BUILD(r, WASM_F32_COPYSIGN(WASM_F32(42.0f), WASM_GET_LOCAL(0)));
    r.Call(16.0f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    WasmRunner<double, double> r(kExecuteInterpreted);
    BUILD(r, WASM_F64_COPYSIGN(WASM_F64(42.0), WASM_GET_LOCAL(0)));
    r.Call(16.0);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    int32_t index = 16;
    WasmRunner<int32_t, float> r(kExecuteInterpreted);
    r.module().AddMemory(WasmModule::kPageSize);
    BUILD(r, WASM_STORE_MEM(MachineType::Float32(), WASM_I32V(index),
                            WASM_GET_LOCAL(0)),
          WASM_I32V(index));
    r.Call(1345.3456f);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<float>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
  {
    int32_t index = 16;
    WasmRunner<int32_t, double> r(kExecuteInterpreted);
    r.module().AddMemory(WasmModule::kPageSize);
    BUILD(r, WASM_STORE_MEM(MachineType::Float64(), WASM_I32V(index),
                            WASM_GET_LOCAL(0)),
          WASM_I32V(index));
    r.Call(1345.3456);
    CHECK(!r.possible_nondeterminism());
    r.Call(std::numeric_limits<double>::quiet_NaN());
    CHECK(r.possible_nondeterminism());
  }
}

TEST(WasmInterpreterActivations) {
  WasmRunner<void> r(kExecuteInterpreted);
  Isolate* isolate = r.main_isolate();
  BUILD(r, WASM_NOP);

  WasmInterpreter* interpreter = r.interpreter();
  WasmInterpreter::Thread* thread = interpreter->GetThread(0);
  CHECK_EQ(0, thread->NumActivations());
  uint32_t act0 = thread->StartActivation();
  CHECK_EQ(0, act0);
  thread->InitFrame(r.function(), nullptr);
  uint32_t act1 = thread->StartActivation();
  CHECK_EQ(1, act1);
  thread->InitFrame(r.function(), nullptr);
  CHECK_EQ(2, thread->NumActivations());
  CHECK_EQ(2, thread->GetFrameCount());
  isolate->set_pending_exception(Smi::kZero);
  thread->HandleException(isolate);
  CHECK_EQ(1, thread->GetFrameCount());
  CHECK_EQ(2, thread->NumActivations());
  thread->FinishActivation(act1);
  CHECK_EQ(1, thread->GetFrameCount());
  CHECK_EQ(1, thread->NumActivations());
  thread->HandleException(isolate);
  CHECK_EQ(0, thread->GetFrameCount());
  CHECK_EQ(1, thread->NumActivations());
  thread->FinishActivation(act0);
  CHECK_EQ(0, thread->NumActivations());
}

TEST(InterpreterLoadWithoutMemory) {
  WasmRunner<int32_t, int32_t> r(kExecuteInterpreted);
  r.module().AddMemory(0);
  BUILD(r, WASM_LOAD_MEM(MachineType::Int32(), WASM_GET_LOCAL(0)));
  CHECK_TRAP32(r.Call(0));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
