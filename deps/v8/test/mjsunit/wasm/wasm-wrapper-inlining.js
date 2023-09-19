// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function testOptimized(fct) {
  %PrepareFunctionForOptimization(fct);
  for (let i = 0; i < 10; ++i) {
    fct();
  }
  %OptimizeFunctionOnNextCall(fct);
  fct();
  assertOptimized(fct);
}

(function TestWrapperInlining() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  builder.addFunction('i32Add', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI32Add,
    ])
    .exportFunc();

  builder.addFunction('i32Mul', makeSig([kWasmI32, kWasmI32], [kWasmI32]))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprI32Mul,
    ])
    .exportFunc();

  let instance = builder.instantiate({});
  let wasmFct = instance.exports.i32Add;
  let fct = () => wasmFct(3, 5);

  testOptimized(fct);
  // Replacing the wasm function will cause a deopt.
  wasmFct = instance.exports.i32Mul;
  assertEquals(15, fct());
  assertUnoptimized(fct);

  // Running it again multiple times will optimize the function again.
  testOptimized(fct);
  // Switching back to the previous wasm function will not cause a deopt.
  wasmFct = instance.exports.i32Add;
  assertEquals(8, fct());
  assertOptimized(fct);
})();
