// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

// =============================================================================
// Tests in this file test the interaction between the wasm interpreter and
// compiled code.
// =============================================================================

(function testGrowMemoryBetweenInterpretedAndCompiled() {
  // grow_memory can be called from interpreted or compiled code, and changes
  // should be reflected in either execution.
  var builder = new WasmModuleBuilder();
  var grow_body = [kExprGetLocal, 0, kExprGrowMemory, kMemoryZero];
  var load_body = [kExprGetLocal, 0, kExprI32LoadMem, 0, 0];
  var store_body = [kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0];
  builder.addFunction('grow_memory', kSig_i_i).addBody(grow_body).exportFunc();
  builder.addFunction('load', kSig_i_i).addBody(load_body).exportFunc();
  builder.addFunction('store', kSig_v_ii).addBody(store_body).exportFunc();
  var grow_interp_function =
      builder.addFunction('grow_memory_interpreted', kSig_i_i)
          .addBody(grow_body)
          .exportFunc();
  var load_interp_function = builder.addFunction('load_interpreted', kSig_i_i)
                                 .addBody(load_body)
                                 .exportFunc();
  var kNumPages = 2;
  var kMaxPages = 10;
  builder.addMemory(kNumPages, kMaxPages, false);
  var instance = builder.instantiate();
  var exp = instance.exports;
  %RedirectToWasmInterpreter(instance, grow_interp_function.index);
  %RedirectToWasmInterpreter(instance, load_interp_function.index);

  // Initially, we can load from offset 12, but not OOB.
  var oob_index = kNumPages * kPageSize;
  var initial_interpreted = % WasmNumInterpretedCalls(instance);
  assertEquals(0, exp.load(12));
  assertEquals(0, exp.load_interpreted(12));
  assertTraps(kTrapMemOutOfBounds, () => exp.load(oob_index));
  assertTraps(
      kTrapMemOutOfBounds, () => exp.load_interpreted(oob_index));
  // Grow by 2 pages from compiled code, and ensure that this is reflected in
  // the interpreter.
  assertEquals(kNumPages, exp.grow_memory(2));
  kNumPages += 2;
  assertEquals(kNumPages, exp.grow_memory_interpreted(0));
  assertEquals(kNumPages, exp.grow_memory(0));
  // Now we can load from the previous OOB index.
  assertEquals(0, exp.load(oob_index));
  assertEquals(0, exp.load_interpreted(oob_index));
  // Set new OOB index and ensure that it traps.
  oob_index = kNumPages * kPageSize;
  assertTraps(kTrapMemOutOfBounds, () => exp.load(oob_index));
  assertTraps(
      kTrapMemOutOfBounds, () => exp.load_interpreted(oob_index));
  // Grow by another page in the interpreter, and ensure that this is reflected
  // in compiled code.
  assertEquals(kNumPages, exp.grow_memory_interpreted(1));
  kNumPages += 1;
  assertEquals(kNumPages, exp.grow_memory_interpreted(0));
  assertEquals(kNumPages, exp.grow_memory(0));
  // Now we can store to the previous OOB index and read it back in both
  // environments.
  exp.store(oob_index, 47);
  assertEquals(47, exp.load(oob_index));
  assertEquals(47, exp.load_interpreted(oob_index));
  // We cannot grow beyong kMaxPages.
  assertEquals(-1, exp.grow_memory(kMaxPages - kNumPages + 1));
  assertEquals(-1, exp.grow_memory_interpreted(kMaxPages - kNumPages + 1));
  // Overall, we executed 9 functions in the interpreter.
  assertEquals(initial_interpreted + 9, % WasmNumInterpretedCalls(instance));
})();
