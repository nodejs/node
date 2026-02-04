// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbolev-inline-js-wasm-wrappers --turboshaft-wasm-in-js-inlining
// Flags: --turbolev --no-maglev
// Flags: --trace-turbo-inlining --trace-deopt
// Flags: --allow-natives-syntax --no-stress-incremental-marking

// Test case for JS-to-Wasm wrapper inlining in Turbolev with tracing output.
// Note that there is a variant for 32 bit and 64 bit, since the Wasm-into-JS
// body inlining is not available on 32 bit platforms and thus there are
// differences in the tracing output.
// Please keep the flags above in sync and keep the code in the 64 bit variant.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

// Wrapper inlining with i64 arguments is not supported on 32-bit architectures.
(function testJsWasmWrapperInliningWithI64Args() {
  print("testJsWasmWrapperInliningWithI64Args");
  var builder = new WasmModuleBuilder();

  builder.addFunction("squareI64", kSig_l_l)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI64Mul])
    .exportAs("squareI64");

  let instance = builder.instantiate({});

  function callSquare(n) {
    return instance.exports.squareI64(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  callSquare(1n);
  print("Test BigInt can be converted");
  %OptimizeFunctionOnNextCall(callSquare);
  callSquare(3n);

  // Invalid arguments throw an exception.
  print("Test input of unconvertible type");
  assertThrows(
    () => callSquare({ x: 4 }), SyntaxError);
  assertThrows(
    () => callSquare("test"), SyntaxError);
})();

// Wrapper inlining with i32 arguments is supported everywhere.
(function testJsWasmWrapperInliningWithI32Args() {
  print("testJsWasmWrapperInliningWithI32Args");
  var builder = new WasmModuleBuilder();

  builder.addFunction("squareI32", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Mul])
    .exportAs("squareI32");

  let instance = builder.instantiate({});

  let square = instance.exports.squareI32;
  function callSquare(n) {
    return square(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  assertEquals(1, callSquare(1));
  %OptimizeFunctionOnNextCall(callSquare);
  assertEquals(9, callSquare(3));

  // Convertible arguments do not trigger a deopt, but are converted to 0.
  print("Test inputs of different convertible type");
  assertEquals(0, callSquare({ x: 4 }));
  assertEquals(0, callSquare("test"));
  assertEquals(0, callSquare(undefined));

  // Non convertible arguments throws an exception.
  print("Test input of unconvertible type");
  assertThrows(
    () => callSquare(2n), TypeError);

  // Eager-deopt expected if we change the callee.
  print("Test different callee");
  square = (a) => a + a;
  assertEquals(6, callSquare(3));

  // Optimizing again does not trigger another deoptimization.
  print("Test optimizing again does not trigger another deoptimization");
  %OptimizeFunctionOnNextCall(callSquare);
  assertEquals(6, callSquare(3));
  assertTrue(%ActiveTierIsTurbofan(callSquare));
})();

(function testJsWasmWrapperInliningWithDifferentModules() {
  print("testJsWasmWrapperInliningWithDifferentModules");
  var builder1 = new WasmModuleBuilder();
  builder1.addFunction("squareI32", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Mul])
    .exportAs("squareI32");
  let instance1 = builder1.instantiate({});

  let square = instance1.exports.squareI32;
  function callSquare(n) {
    return square(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  assertEquals(25, callSquare(5));
  %OptimizeFunctionOnNextCall(callSquare);
  assertEquals(9, callSquare(3));

  var builder2 = new WasmModuleBuilder();
  builder2.addFunction("doubleI32", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Add])
    .exportAs("squareI32");  // Export with same function name.
  let instance2 = builder2.instantiate({});

  // Eager-deopt expected if we change the callee.
  print("Test callee from different instance");
  square = instance2.exports.squareI32;
  assertEquals(6, callSquare(3));

  // Optimizing again does not trigger another deoptimization.
  print("Test optimizing again does not trigger another deoptimization");
  %OptimizeFunctionOnNextCall(callSquare);
  assertEquals(6, callSquare(3));
  assertTrue(%ActiveTierIsTurbofan(callSquare));
})();

(function testJsWasmWrapperInliningOfSameExportedImportedFunction() {
  print("testJsWasmWrapperInliningOfSameExportedImportedFunction");
  var builder1 = new WasmModuleBuilder();
  builder1.addFunction("squareI32", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Mul])
    .exportAs("squareI32");
  builder1.addFunction("doubleI32", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprI32Add])
    .exportAs("doubleI32");
  let instance1 = builder1.instantiate({});

  let square = instance1.exports.squareI32;
  function callSquare(n) {
    return square(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  assertEquals(25, callSquare(5));
  %OptimizeFunctionOnNextCall(callSquare);
  assertEquals(9, callSquare(3));

  var builder2 = new WasmModuleBuilder();
  builder2.addImport("o", "fn", kSig_i_i);
  builder2.addExport("fn", 0);
  let instance2 = builder2.instantiate({
    o: { fn: instance1.exports.squareI32 }
  });
  // No deopt expected because it is the same function.
  print("Test replacing callee with same function imported and exported by " +
    "different modules");
  square = instance2.exports.fn;
  assertEquals(9, callSquare(3));
  assertTrue(%ActiveTierIsTurbofan(callSquare));

  var builder3 = new WasmModuleBuilder();
  builder3.addImport("o", "fn", kSig_i_i);
  builder3.addExport("fn", 0);
  let instance3 = builder2.instantiate({
    o: { fn: instance1.exports.doubleI32 }
  });
  // Deopt expected because it is a different function.
  print("Test replacing callee with different function from same module");
  square = instance3.exports.fn;
  assertEquals(6, callSquare(3));
  assertFalse(%ActiveTierIsTurbofan(callSquare));
  // Optimizing again does not trigger another deoptimization.
  print("Test optimizing again does not trigger another deoptimization");
  %OptimizeFunctionOnNextCall(callSquare);
  assertEquals(6, callSquare(3));
  assertTrue(%ActiveTierIsTurbofan(callSquare));
})();

(function testJsWasmWrapperInliningWithF32Args() {
  print("testJsWasmWrapperInliningWithF32Args");
  var builder = new WasmModuleBuilder();

  builder.addFunction("squareF32", kSig_f_f)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprF32Mul])
    .exportAs("squareF32");

  let instance = builder.instantiate({});

  function callSquare(n) {
    return instance.exports.squareF32(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  callSquare(1.1);
  %OptimizeFunctionOnNextCall(callSquare);
  callSquare(3.3);

  // Convertible arguments do not trigger a deopt, but are converted to NaN.
  print("Test inputs of different convertible type");
  assertEquals(NaN, callSquare({ x: 4 }));
  assertEquals(NaN, callSquare("test"));
  assertEquals(NaN, callSquare(undefined));

  // Non convertible arguments throw an exception.
  print("Test input of unconvertible type");
  assertThrows(
    () => callSquare(2n), TypeError);
})();

(function testJsWasmWrapperInliningWithF64Args() {
  print("testJsWasmWrapperInliningWithF64Args");
  var builder = new WasmModuleBuilder();

  builder.addFunction("squareF64", kSig_d_d)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 0,
      kExprF64Mul])
    .exportAs("squareF64");

  let instance = builder.instantiate({});

  function callSquare(n) {
    return instance.exports.squareF64(n);
  }

  %PrepareFunctionForOptimization(callSquare);
  callSquare(1.1);
  %OptimizeFunctionOnNextCall(callSquare);
  callSquare(3.3);

  // Convertible arguments do not trigger a deopt, but are converted to NaN.
  print("Test inputs of different convertible type");
  assertEquals(NaN, callSquare({ x: 4 }));
  assertEquals(NaN, callSquare("test"));
  assertEquals(NaN, callSquare(undefined));

  // Non convertible arguments throw an exception.
  print("Test input of unconvertible type");
  assertThrows(
    () => callSquare(2n), TypeError);
})();

(function testJsWasmWrapperInliningWithExternRefArgs() {
  print("testJsWasmWrapperInliningWithExternRefArgs");
  function fn() {
    return 42;
  }

  let builder = new WasmModuleBuilder();
  var kSig_r_r = makeSig([kWasmExternRef], [kWasmExternRef]);

  builder.addFunction("refTest", kSig_r_r)
    .addBody([
      kExprLocalGet, 0,
    ])
    .exportAs("refTest");

  let instance = builder.instantiate({});

  function callTest(f) {
    return instance.exports.refTest(f);
  }

  %PrepareFunctionForOptimization(callTest);
  assertEquals(fn, callTest(fn));
  print("Test input of externref type");
  %OptimizeFunctionOnNextCall(callTest);
  assertEquals(fn, callTest(fn));
})();

(function testNoJsWasmWrapperInliningWithRefArgs() {
  print("testNoJsWasmWrapperInliningWithRefArgs");
  function fn() {
    return 42;
  }

  let builder = new WasmModuleBuilder();
  var kSig_a_a = makeSig([kWasmAnyFunc], [kWasmAnyFunc]);

  builder.addFunction("refTest", kSig_a_a)
    .addBody([
      kExprLocalGet, 0,
    ])
    .exportAs("refTest");

  let instance = builder.instantiate({});
  const refTest = instance.exports.refTest;

  function callTest(f) {
    // Funcref arguments do not get a JS-to-Wasm wrapper, so no inlining.
    return refTest(f);
  }

  %PrepareFunctionForOptimization(callTest);
  assertEquals(refTest, callTest(refTest));
  %OptimizeFunctionOnNextCall(callTest);
  assertEquals(refTest, callTest(refTest));
})();

(function testJsWasmWrapperInliningRetrievesWasmInstance() {
  print("testJsWasmWrapperInliningRetrievesWasmInstance");

  let builder = new WasmModuleBuilder();
  builder.addGlobal(kWasmI32, true, false);

  builder.addFunction("globalTest", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprGlobalSet, 0,
      kExprGlobalGet, 0,
    ])
    .exportAs("globalTest");

  let instance = builder.instantiate({});
  const globalTest = instance.exports.globalTest;

  function callTest(i32) {
    // Verify that the Wasm instance is retrieved correctly.
    return globalTest(i32);
  }

  %PrepareFunctionForOptimization(callTest);
  assertEquals(321, callTest(321));
  %OptimizeFunctionOnNextCall(callTest);
  assertEquals(123, callTest(123));
})();
