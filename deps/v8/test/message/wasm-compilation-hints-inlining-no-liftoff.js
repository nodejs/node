// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --trace-wasm-inlining
// Flags: --allow-natives-syntax --no-liftoff --wasm-lazy-compilation

// We need --wasm-lazy-compilation because otherwise we compile all functions
// (even those without hints) with TurboFan and the test output changes.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');
d8.file.execute('test/mjsunit/mjsunit.js');

(function TestInliningDirectCall() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 1, kExprCallFunction, add.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(caller.index, [{offset: 8, frequency: 64}]);

  let wasm = builder.instantiate({}).exports;

  // Since --no-liftoff, the function gets immediately compiled with TurboFan.
  assertTrue(%IsTurboFanFunction(wasm.caller));

  assertEquals(2 * 20 + 10, wasm.caller(20, 10));
})();

// Tests that we inline `intermediate` based on --no-liftoff budget.
(function TestNoHintForIntermediate() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add]);

  let intermediate = builder.addFunction("intermediate", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
              kExprLocalGet, 1, kExprI32Const, 1, kExprI32Add,
              kExprCallFunction, add.index])

  let caller = builder.addFunction("caller", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul,
              kExprLocalGet, 1, kExprCallFunction, intermediate.index])
    .exportFunc();

  builder.setCompilationPriority(caller.index, 0, 0);

  builder.setInstructionFrequencies(caller.index, [{offset: 8, frequency: 64}]);

  let wasm = builder.instantiate({}).exports;

  // Since --no-liftoff, the function gets immediately compiled with TurboFan.
  assertTrue(%IsTurboFanFunction(wasm.caller));

  assertEquals((2 * 20 + 1) + (10 + 1), wasm.caller(20, 10));
})();
