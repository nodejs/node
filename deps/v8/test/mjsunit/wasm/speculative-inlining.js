// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-speculative-inlining --experimental-wasm-return-call
// Flags: --experimental-wasm-typed-funcref

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function CallRefSpecSucceededTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = x - 1
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub]);

  let global = builder.addGlobal(wasmRefType(0), false,
                                 WasmInitExpr.RefFunc(callee.index));

  // g(x) = f(5) + x
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprGlobalGet, global.index, kExprCallRef,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  let instance = builder.instantiate();
  // Run it 10 times to trigger tier-up.
  for (var i = 0; i < 10; i++) assertEquals(14, instance.exports.main(10));
})();

(function CallRefSpecFailedTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // h(x) = x - 1
  let callee0 = builder.addFunction("callee0", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub]);

  // f(x) = x - 2
  let callee1 = builder.addFunction("callee1", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Sub]);

  let global0 = builder.addGlobal(wasmRefType(1), false,
                                 WasmInitExpr.RefFunc(callee0.index));
  let global1 = builder.addGlobal(wasmRefType(1), false,
                                 WasmInitExpr.RefFunc(callee1.index));

  // g(x, y) = if (y) { h(5) + x } else { f(7) + x }
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprLocalGet, 1,
      kExprIf, kWasmI32,
        kExprI32Const, 5, kExprGlobalGet, global0.index, kExprCallRef,
        kExprLocalGet, 0, kExprI32Add,
      kExprElse,
        kExprI32Const, 7, kExprGlobalGet, global1.index, kExprCallRef,
        kExprLocalGet, 0, kExprI32Add,
      kExprEnd])
    .exportAs("main");

  let instance = builder.instantiate();
  // Run main 10 times with the same function reference to trigger tier-up.
  // This will speculatively inline a call to function {h}.
  for (var i = 0; i < 10; i++) assertEquals(14, instance.exports.main(10, 1));
  // If tier-up is done, "callee0" should be inlined in the trace.
  assertEquals(14, instance.exports.main(10, 1))

  // Now, run main with {f} instead. The correct reference should still be
  // called, i.e., "callee1".
  assertEquals(15, instance.exports.main(10, 0));
})();

// TODO(manoskouk): Fix the following tests.
(function CallReturnRefSpecSucceededTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = x - 1
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub]);

  let global = builder.addGlobal(wasmRefType(0), false,
                                 WasmInitExpr.RefFunc(callee.index));

  // g(x) = f(5 + x)
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprLocalGet, 0, kExprI32Add,
              kExprGlobalGet, global.index, kExprReturnCallRef])
    .exportAs("main");

  let instance = builder.instantiate();
  // Run it 10 times to trigger tier-up.
  for (var i = 0; i < 10; i++) assertEquals(14, instance.exports.main(10));
})();

(function CallReturnRefSpecFailedTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // h(x) = x - 1
  let callee0 = builder.addFunction("callee0", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub]);

  // f(x) = x - 2
  let callee1 = builder.addFunction("callee1", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Sub]);

  let global0 = builder.addGlobal(wasmRefType(1), false,
                                 WasmInitExpr.RefFunc(callee0.index));
  let global1 = builder.addGlobal(wasmRefType(1), false,
                                 WasmInitExpr.RefFunc(callee1.index));

  // g(x, y) = if (y) { h(x) } else { f(x) }
  builder.addFunction("main", kSig_i_ii)
    .addBody([
      kExprLocalGet, 1,
      kExprIf, kWasmI32,
        kExprLocalGet, 0, kExprGlobalGet, global0.index, kExprReturnCallRef,
      kExprElse,
        kExprLocalGet, 0, kExprGlobalGet, global1.index, kExprReturnCallRef,
      kExprEnd])
    .exportAs("main");

  let instance = builder.instantiate();
  // Run main 10 times with the same function reference to trigger tier-up.
  // This will speculatively inline a call to function {h}.
  for (var i = 0; i < 10; i++) assertEquals(9, instance.exports.main(10, 1));
  // If tier-up is done, "callee0" should be inlined in the trace.
  assertEquals(9, instance.exports.main(10, 1))

  // Now, run main with {f} instead. The correct reference should still be
  // called, i.e., "callee1".
  assertEquals(8, instance.exports.main(10, 0));
})();
