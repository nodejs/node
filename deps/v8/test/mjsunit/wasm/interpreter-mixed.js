// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

load('test/mjsunit/wasm/wasm-module-builder.js');

// =============================================================================
// Tests in this file test the interaction between the wasm interpreter and
// compiled code.
// =============================================================================

// The stack trace contains file path, replace it by "file".
let stripPath = s => s.replace(/[^ (]*interpreter-mixed\.js/g, 'file');

function checkStack(stack, expected_lines) {
  print('stack: ' + stack);
  let lines = stack.split('\n');
  assertEquals(expected_lines.length, lines.length);
  for (let i = 0; i < lines.length; ++i) {
    let test =
        typeof expected_lines[i] == 'string' ? assertEquals : assertMatches;
    test(expected_lines[i], lines[i], 'line ' + i);
  }
}

(function testMemoryGrowBetweenInterpretedAndCompiled() {
  // grow_memory can be called from interpreted or compiled code, and changes
  // should be reflected in either execution.
  var builder = new WasmModuleBuilder();
  var grow_body = [kExprGetLocal, 0, kExprMemoryGrow, kMemoryZero];
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
  var initial_interpreted = %WasmNumInterpretedCalls(instance);
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
  assertEquals(initial_interpreted + 9, %WasmNumInterpretedCalls(instance));
})();

function createTwoInstancesCallingEachOther(inner_throws = false) {
  let builder1 = new WasmModuleBuilder();

  let id_imp = builder1.addImport('q', 'id', kSig_i_i);
  let plus_one = builder1.addFunction('plus_one', kSig_i_i)
                     .addBody([
                       kExprGetLocal, 0,  // -
                       kExprI32Const, 1,  // -
                       kExprI32Add,       // -
                       kExprCallFunction, id_imp
                     ])
                     .exportFunc();
  function imp(i) {
    if (inner_throws) throw new Error('i=' + i);
    return i;
  }
  let instance1 = builder1.instantiate({q: {id: imp}});

  let builder2 = new WasmModuleBuilder();

  let plus_one_imp = builder2.addImport('q', 'plus_one', kSig_i_i);
  let plus_two = builder2.addFunction('plus_two', kSig_i_i)
                     .addBody([
                       // Call import, add one more.
                       kExprGetLocal, 0,                 // -
                       kExprCallFunction, plus_one_imp,  // -
                       kExprI32Const, 1,                 // -
                       kExprI32Add
                     ])
                     .exportFunc();

  let instance2 =
      builder2.instantiate({q: {plus_one: instance1.exports.plus_one}});
  return [instance1, instance2];
}

function redirectToInterpreter(
    instance1, instance2, redirect_plus_one, redirect_plus_two) {
  // Redirect functions to the interpreter.
  if (redirect_plus_one) {
    %RedirectToWasmInterpreter(instance1,
                               parseInt(instance1.exports.plus_one.name));
  }
  if (redirect_plus_two) {
    %RedirectToWasmInterpreter(instance2,
                               parseInt(instance2.exports.plus_two.name));
  }
}

(function testImportFromOtherInstance() {
  print("testImportFromOtherInstance");
  // Three runs: Break in instance 1, break in instance 2, or both.
  for (let run = 0; run < 3; ++run) {
    print(" - run " + run);
    let [instance1, instance2] = createTwoInstancesCallingEachOther();

    let interpreted_before_1 = %WasmNumInterpretedCalls(instance1);
    let interpreted_before_2 = %WasmNumInterpretedCalls(instance2);
    // Call plus_two, which calls plus_one.
    assertEquals(9, instance2.exports.plus_two(7));

    // Nothing interpreted:
    assertEquals(interpreted_before_1, %WasmNumInterpretedCalls(instance1));
    assertEquals(interpreted_before_2, %WasmNumInterpretedCalls(instance2));

    // Now redirect functions to the interpreter.
    redirectToInterpreter(instance1, instance2, run != 1, run != 0);

    // Call plus_two, which calls plus_one.
    assertEquals(9, instance2.exports.plus_two(7));

    // TODO(6668): Fix patching of instances which imported others' code.
    //assertEquals(interpreted_before_1 + (run == 1 ? 0 : 1),
    //             %WasmNumInterpretedCalls(instance1));
    assertEquals(interpreted_before_2 + (run == 0 ? 0 : 1),
                 %WasmNumInterpretedCalls(instance2));
  }
})();

(function testStackTraceThroughCWasmEntry() {
  print("testStackTraceThroughCWasmEntry");
  for (let run = 0; run < 3; ++run) {
    print(" - run " + run);
    let [instance1, instance2] = createTwoInstancesCallingEachOther(true);
    redirectToInterpreter(instance1, instance2, run != 1, run != 0);

    try {
      // Call plus_two, which calls plus_one.
      instance2.exports.plus_two(7);
      assertUnreachable('should trap because of unreachable instruction');
    } catch (e) {
      checkStack(stripPath(e.stack), [
        'Error: i=8',                                                // -
        /^    at imp \(file:\d+:29\)$/,                              // -
        '    at plus_one (wasm-function[1]:6)',                      // -
        '    at plus_two (wasm-function[1]:3)',                      // -
        /^    at testStackTraceThroughCWasmEntry \(file:\d+:25\)$/,  // -
        /^    at file:\d+:3$/
      ]);
    }
  }
})();

(function testInterpreterPreservedOnTierUp() {
  print(arguments.callee.name);
  var builder = new WasmModuleBuilder();
  var fun_body = [kExprI32Const, 23];
  var fun = builder.addFunction('fun', kSig_i_v).addBody(fun_body).exportFunc();
  var instance = builder.instantiate();
  var exp = instance.exports;

  // Initially the interpreter is not being called.
  var initial_interpreted = %WasmNumInterpretedCalls(instance);
  assertEquals(23, exp.fun());
  assertEquals(initial_interpreted + 0, %WasmNumInterpretedCalls(instance));

  // Redirection will cause the interpreter to be called.
  %RedirectToWasmInterpreter(instance, fun.index);
  assertEquals(23, exp.fun());
  assertEquals(initial_interpreted + 1, %WasmNumInterpretedCalls(instance));

  // Requesting a tier-up still ensure the interpreter is being called.
  %WasmTierUpFunction(instance, fun.index);
  assertEquals(23, exp.fun());
  assertEquals(initial_interpreted + 2, %WasmNumInterpretedCalls(instance));
})();
