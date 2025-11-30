// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-compilation-hints --allow-natives-syntax
// Flags: --liftoff --wasm-dynamic-tiering --wasm-tiering-budget=10
// Flags: --no-predictable

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestCompilationPriority() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let add = builder.addFunction("add", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
    .exportFunc();

  let sub = builder.addFunction("sub", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub])
    .exportFunc();

  let mul = builder.addFunction("mul", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Mul])
    .exportFunc();

  let div = builder.addFunction("div", kSig_i_ii)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32DivS])
    .exportFunc();

  builder.setCompilationPriority(add.index, 2, undefined);
  builder.setCompilationPriority(sub.index, 1, undefined);
  builder.setCompilationPriority(div.index, 0, 0)

  // Should compile both 'add' and 'sub' with Liftoff, but not 'mul'.
  let wasm = builder.instantiate().exports;
  if (%IsWasmTieringPredictable()) {
    assertTrue(%IsLiftoffFunction(wasm.add));
  }
  assertTrue(%IsLiftoffFunction(wasm.sub));
  assertTrue(%IsUncompiledWasmFunction(wasm.mul));
  // 'div' gets compiled with Liftoff at instantiation time, but gets tiered-up
  // to TurboFan in the background.
  while (!%IsTurboFanFunction(wasm.div)) {
    assertTrue(%IsLiftoffFunction(wasm.div) || %IsTurboFanFunction(wasm.div));
  }

  while (!%IsTurboFanFunction(wasm.add)) {
    assertEquals(30, wasm.add(10, 20));  // Should tier-up 'add' eventually.
  }

  assertTrue(%IsTurboFanFunction(wasm.add));
})();

(function TestCompilationPriorityWithImportedFunctions() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  builder.addImport("m", "i", kSig_v_v);

  let dummy = builder.addFunction("dummy", kSig_v_v).addBody([]).exportFunc();

  builder.setCompilationPriority(dummy.index, 0, undefined);

  function imp() { print(0); }

  let wasm = builder.instantiate({m: {i: imp}}).exports;
  assertTrue(%IsLiftoffFunction(wasm.dummy));
})();
