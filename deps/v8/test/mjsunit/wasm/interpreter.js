// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-interpret-all --allow-natives-syntax --expose-gc

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

// The stack trace contains file path, only keep "interpreter.js".
let stripPath = s => s.replace(/[^ (]*interpreter\.js/g, 'interpreter.js');

function checkStack(stack, expected_lines) {
  print('stack: ' + stack);
  var lines = stack.split('\n');
  assertEquals(expected_lines.length, lines.length);
  for (var i = 0; i < lines.length; ++i) {
    let test =
        typeof expected_lines[i] == 'string' ? assertEquals : assertMatches;
    test(expected_lines[i], lines[i], 'line ' + i);
  }
}

(function testCallImported() {
  var stack;
  let func = () => stack = new Error('test imported stack').stack;

  var builder = new WasmModuleBuilder();
  builder.addImport('mod', 'func', kSig_v_v);
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprCallFunction, 0])
      .exportFunc();
  var instance = builder.instantiate({mod: {func: func}});
  // Test that this does not mess up internal state by executing it three times.
  for (var i = 0; i < 3; ++i) {
    var interpreted_before = % WasmNumInterpretedCalls(instance);
    instance.exports.main();
    assertEquals(interpreted_before + 1, % WasmNumInterpretedCalls(instance));
    checkStack(stripPath(stack), [
      'Error: test imported stack',                           // -
      /^    at func \(interpreter.js:\d+:28\)$/,              // -
      '    at main (<WASM>[1]+1)',                            // -
      /^    at testCallImported \(interpreter.js:\d+:22\)$/,  // -
      /^    at interpreter.js:\d+:3$/
    ]);
  }
})();

(function testCallImportedWithParameters() {
  var stack;
  var passed_args = [];
  let func1 = (i, j) => (passed_args.push(i, j), 2 * i + j);
  let func2 = (f) => (passed_args.push(f), 8 * f);

  var builder = new WasmModuleBuilder();
  builder.addImport('mod', 'func1', makeSig([kWasmI32, kWasmI32], [kWasmF32]));
  builder.addImport('mod', 'func2', makeSig([kWasmF64], [kWasmI32]));
  builder.addFunction('main', makeSig([kWasmI32, kWasmF64], [kWasmF32]))
      .addBody([
        // call #0 with arg 0 and arg 0 + 1
        kExprGetLocal, 0, kExprGetLocal, 0, kExprI32Const, 1, kExprI32Add,
        kExprCallFunction, 0,
        // call #1 with arg 1
        kExprGetLocal, 1, kExprCallFunction, 1,
        // convert returned value to f32
        kExprF32UConvertI32,
        // add the two values
        kExprF32Add
      ])
      .exportFunc();
  var instance = builder.instantiate({mod: {func1: func1, func2: func2}});
  var interpreted_before = % WasmNumInterpretedCalls(instance);
  var args = [11, 0.3];
  var ret = instance.exports.main(...args);
  assertEquals(interpreted_before + 1, % WasmNumInterpretedCalls(instance));
  var passed_test_args = [...passed_args];
  var expected = func1(args[0], args[0] + 1) + func2(args[1]) | 0;
  assertEquals(expected, ret);
  assertArrayEquals([args[0], args[0] + 1, args[1]], passed_test_args);
})();

(function testTrap() {
  var builder = new WasmModuleBuilder();
  var foo_idx = builder.addFunction('foo', kSig_v_v)
                    .addBody([kExprNop, kExprNop, kExprUnreachable])
                    .index;
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprNop, kExprCallFunction, foo_idx])
      .exportFunc();
  var instance = builder.instantiate();
  // Test that this does not mess up internal state by executing it three times.
  for (var i = 0; i < 3; ++i) {
    var interpreted_before = % WasmNumInterpretedCalls(instance);
    var stack;
    try {
      instance.exports.main();
      assertUnreachable();
    } catch (e) {
      stack = e.stack;
    }
    assertEquals(interpreted_before + 2, % WasmNumInterpretedCalls(instance));
    checkStack(stripPath(stack), [
      'RuntimeError: unreachable',                    // -
      '    at foo (<WASM>[0]+3)',                     // -
      '    at main (<WASM>[1]+2)',                    // -
      /^    at testTrap \(interpreter.js:\d+:24\)$/,  // -
      /^    at interpreter.js:\d+:3$/
    ]);
  }
})();

(function testThrowFromImport() {
  function func() {
    throw new Error('thrown from imported function');
  }
  var builder = new WasmModuleBuilder();
  builder.addImport("mod", "func", kSig_v_v);
  builder.addFunction('main', kSig_v_v)
      .addBody([kExprCallFunction, 0])
      .exportFunc();
  var instance = builder.instantiate({mod: {func: func}});
  // Test that this does not mess up internal state by executing it three times.
  for (var i = 0; i < 3; ++i) {
    var interpreted_before = % WasmNumInterpretedCalls(instance);
    var stack;
    try {
      instance.exports.main();
      assertUnreachable();
    } catch (e) {
      stack = e.stack;
    }
    assertEquals(interpreted_before + 1, % WasmNumInterpretedCalls(instance));
    checkStack(stripPath(stack), [
      'Error: thrown from imported function',                    // -
      /^    at func \(interpreter.js:\d+:11\)$/,                 // -
      '    at main (<WASM>[1]+1)',                               // -
      /^    at testThrowFromImport \(interpreter.js:\d+:24\)$/,  // -
      /^    at interpreter.js:\d+:3$/
    ]);
  }
})();

(function testGlobals() {
  var builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true);  // 0
  builder.addGlobal(kWasmI64, true);  // 1
  builder.addGlobal(kWasmF32, true);  // 2
  builder.addGlobal(kWasmF64, true);  // 3
  builder.addFunction('get_i32', kSig_i_v)
      .addBody([kExprGetGlobal, 0])
      .exportFunc();
  builder.addFunction('get_i64', kSig_d_v)
      .addBody([kExprGetGlobal, 1, kExprF64SConvertI64])
      .exportFunc();
  builder.addFunction('get_f32', kSig_d_v)
      .addBody([kExprGetGlobal, 2, kExprF64ConvertF32])
      .exportFunc();
  builder.addFunction('get_f64', kSig_d_v)
      .addBody([kExprGetGlobal, 3])
      .exportFunc();
  builder.addFunction('set_i32', kSig_v_i)
      .addBody([kExprGetLocal, 0, kExprSetGlobal, 0])
      .exportFunc();
  builder.addFunction('set_i64', kSig_v_d)
      .addBody([kExprGetLocal, 0, kExprI64SConvertF64, kExprSetGlobal, 1])
      .exportFunc();
  builder.addFunction('set_f32', kSig_v_d)
      .addBody([kExprGetLocal, 0, kExprF32ConvertF64, kExprSetGlobal, 2])
      .exportFunc();
  builder.addFunction('set_f64', kSig_v_d)
      .addBody([kExprGetLocal, 0, kExprSetGlobal, 3])
      .exportFunc();
  var instance = builder.instantiate();
  // Initially, all should be zero.
  assertEquals(0, instance.exports.get_i32());
  assertEquals(0, instance.exports.get_i64());
  assertEquals(0, instance.exports.get_f32());
  assertEquals(0, instance.exports.get_f64());
  // Assign values to all variables.
  var values = [4711, 1<<40 + 1 << 33, 0.3, 12.34567];
  instance.exports.set_i32(values[0]);
  instance.exports.set_i64(values[1]);
  instance.exports.set_f32(values[2]);
  instance.exports.set_f64(values[3]);
  // Now check the values.
  assertEquals(values[0], instance.exports.get_i32());
  assertEquals(values[1], instance.exports.get_i64());
  assertEqualsDelta(values[2], instance.exports.get_f32(), 2**-23);
  assertEquals(values[3], instance.exports.get_f64());
})();

(function testReentrantInterpreter() {
  var stacks;
  var instance;
  function func(i) {
    stacks.push(new Error('reentrant interpreter test #' + i).stack);
    if (i < 2) instance.exports.main(i + 1);
  }

  var builder = new WasmModuleBuilder();
  builder.addImport('mod', 'func', kSig_v_i);
  builder.addFunction('main', kSig_v_i)
      .addBody([kExprGetLocal, 0, kExprCallFunction, 0])
      .exportFunc();
  instance = builder.instantiate({mod: {func: func}});
  // Test that this does not mess up internal state by executing it three times.
  for (var i = 0; i < 3; ++i) {
    var interpreted_before = % WasmNumInterpretedCalls(instance);
    stacks = [];
    instance.exports.main(0);
    assertEquals(interpreted_before + 3, % WasmNumInterpretedCalls(instance));
    assertEquals(3, stacks.length);
    for (var e = 0; e < stacks.length; ++e) {
      expected = ['Error: reentrant interpreter test #' + e];
      expected.push(/^    at func \(interpreter.js:\d+:17\)$/);
      expected.push('    at main (<WASM>[1]+3)');
      for (var k = e; k > 0; --k) {
        expected.push(/^    at func \(interpreter.js:\d+:33\)$/);
        expected.push('    at main (<WASM>[1]+3)');
      }
      expected.push(
          /^    at testReentrantInterpreter \(interpreter.js:\d+:22\)$/);
      expected.push(/    at interpreter.js:\d+:3$/);
      checkStack(stripPath(stacks[e]), expected);
    }
  }
})();

(function testIndirectImports() {
  var builder = new WasmModuleBuilder();

  var sig_i_ii = builder.addType(kSig_i_ii);
  var sig_i_i = builder.addType(kSig_i_i);
  var mul = builder.addImport('q', 'mul', sig_i_ii);
  var add = builder.addFunction('add', sig_i_ii).addBody([
    kExprGetLocal, 0, kExprGetLocal, 1, kExprI32Add
  ]);
  var mismatch =
      builder.addFunction('sig_mismatch', sig_i_i).addBody([kExprGetLocal, 0]);
  var main = builder.addFunction('main', kSig_i_iii)
                 .addBody([
                   // Call indirect #0 with args <#1, #2>.
                   kExprGetLocal, 1, kExprGetLocal, 2, kExprGetLocal, 0,
                   kExprCallIndirect, sig_i_ii, kTableZero
                 ])
                 .exportFunc();
  builder.appendToTable([mul, add.index, mismatch.index, main.index]);

  var instance = builder.instantiate({q: {mul: (a, b) => a * b}});

  // Call mul.
  assertEquals(-6, instance.exports.main(0, -2, 3));
  // Call add.
  assertEquals(99, instance.exports.main(1, 22, 77));
  // main and sig_mismatch have another signature.
  assertTraps(kTrapFuncSigMismatch, () => instance.exports.main(2, 12, 33));
  assertTraps(kTrapFuncSigMismatch, () => instance.exports.main(3, 12, 33));
  // Function index 4 does not exist.
  assertTraps(kTrapFuncInvalid, () => instance.exports.main(4, 12, 33));
})();

(function testIllegalImports() {
  var builder = new WasmModuleBuilder();

  var sig_l_v = builder.addType(kSig_l_v);
  var imp = builder.addImport('q', 'imp', sig_l_v);
  var direct = builder.addFunction('direct', kSig_l_v)
                   .addBody([kExprCallFunction, imp])
                   .exportFunc();
  var indirect = builder.addFunction('indirect', kSig_l_v).addBody([
    kExprI32Const, 0, kExprCallIndirect, sig_l_v, kTableZero
  ]);
  var main =
      builder.addFunction('main', kSig_v_i)
          .addBody([
            // Call indirect #0 with arg #0, drop result.
            kExprGetLocal, 0, kExprCallIndirect, sig_l_v, kTableZero, kExprDrop
          ])
          .exportFunc();
  builder.appendToTable([imp, direct.index, indirect.index]);

  var instance = builder.instantiate({q: {imp: () => 1}});

  // Calling imported functions with i64 in signature should fail.
  try {
    // Via direct call.
    instance.exports.main(1);
  } catch (e) {
    if (!(e instanceof TypeError)) throw e;
    checkStack(stripPath(e.stack), [
      'TypeError: invalid type',                                // -
      '    at direct (<WASM>[1]+1)',                            // -
      '    at main (<WASM>[3]+3)',                              // -
      /^    at testIllegalImports \(interpreter.js:\d+:22\)$/,  // -
      /^    at interpreter.js:\d+:3$/
    ]);
  }
  try {
    // Via indirect call.
    instance.exports.main(2);
  } catch (e) {
    if (!(e instanceof TypeError)) throw e;
    checkStack(stripPath(e.stack), [
      'TypeError: invalid type',                                // -
      '    at indirect (<WASM>[2]+1)',                          // -
      '    at main (<WASM>[3]+3)',                              // -
      /^    at testIllegalImports \(interpreter.js:\d+:22\)$/,  // -
      /^    at interpreter.js:\d+:3$/
    ]);
  }
})();

(function testInfiniteRecursion() {
  var builder = new WasmModuleBuilder();

  var direct = builder.addFunction('main', kSig_v_v)
                   .addBody([kExprNop, kExprCallFunction, 0])
                   .exportFunc();
  var instance = builder.instantiate();

  try {
    instance.exports.main();
    assertUnreachable("should throw");
  } catch (e) {
    if (!(e instanceof RangeError)) throw e;
    checkStack(stripPath(e.stack), [
      'RangeError: Maximum call stack size exceeded',
      '    at main (<WASM>[0]+0)'
    ].concat(Array(9).fill('    at main (<WASM>[0]+2)')));
  }
})();

(function testUnwindSingleActivation() {
  // Create two activations and unwind just the top one.
  var builder = new WasmModuleBuilder();

  function MyError(i) {
    this.i = i;
  }

  // We call WASM -> func 1 -> WASM -> func2.
  // func2 throws, func 1 catches.
  function func1() {
    try {
      return instance.exports.foo();
    } catch (e) {
      if (!(e instanceof MyError)) throw e;
      return e.i + 2;
    }
  }
  function func2() {
    throw new MyError(11);
  }
  var imp1 = builder.addImport('mod', 'func1', kSig_i_v);
  var imp2 = builder.addImport('mod', 'func2', kSig_v_v);
  builder.addFunction('main', kSig_i_v)
      .addBody([kExprCallFunction, imp1, kExprI32Const, 2, kExprI32Mul])
      .exportFunc();
  builder.addFunction('foo', kSig_v_v)
      .addBody([kExprCallFunction, imp2])
      .exportFunc();
  var instance = builder.instantiate({mod: {func1: func1, func2: func2}});

  var interpreted_before = % WasmNumInterpretedCalls(instance);
  assertEquals(2 * (11 + 2), instance.exports.main());
  assertEquals(interpreted_before + 2, % WasmNumInterpretedCalls(instance));
})();

(function testInterpreterGC() {
  function run(f) {
    // wrap the creation in a closure so that the only thing returned is
    // the module (i.e. the underlying array buffer of WASM wire bytes dies).
    var module = (() => {
      var builder = new WasmModuleBuilder();
      var imp = builder.addImport('mod', 'the_name_of_my_import', kSig_i_i);
      builder.addFunction('main', kSig_i_i)
          .addBody([kExprGetLocal, 0, kExprCallFunction, imp])
          .exportAs('main');
      print('module');
      return new WebAssembly.Module(builder.toBuffer());
    })();

    gc();
    for (var i = 0; i < 10; i++) {
      print('  instance ' + i);
      var instance =
          new WebAssembly.Instance(module, {'mod': {the_name_of_my_import: f}});
      var g = instance.exports.main;
      assertEquals('function', typeof g);
      for (var j = 0; j < 10; j++) {
        assertEquals(f(j), g(j));
      }
    }
  }

  for (var i = 0; i < 3; i++) {
    run(x => (x + 19));
    run(x => (x - 18));
  }
})();
