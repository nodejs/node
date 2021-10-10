// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-inlining --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// TODO(12166): Consider running tests with --trace-wasm and inspecting their
// output.

(function SimpleInliningTest() {
  let builder = new WasmModuleBuilder();

  // f(x) = x - 1
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub]);
  // g(x) = f(5) + x
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprCallFunction, callee.index,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(instance.exports.main(10), 14);
})();

(function MultiReturnTest() {
  let builder = new WasmModuleBuilder();

  // f(x) = (x - 1, x + 1)
  let callee = builder.addFunction("callee", kSig_ii_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
              kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add]);
  // g(x) = { let (a, b) = f(x); a * b}
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index, kExprI32Mul])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(instance.exports.main(10), 9 * 11);
})();

(function NoReturnTest() {
  let builder = new WasmModuleBuilder();

  let global = builder.addGlobal(kWasmI32, true);

  let callee = builder.addFunction("callee", kSig_v_i)
    .addBody([kExprLocalGet, 0, kExprGlobalSet, global.index]);

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index,
              kExprGlobalGet, global.index])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(instance.exports.main(10), 10);
})();

(function InfiniteLoopTest() {
  let builder = new WasmModuleBuilder();

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLoop, kWasmVoid,
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
                kExprLocalSet, 0, kExprBr, 0,
              kExprEnd,
              kExprLocalGet, 0]);

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprCallFunction, callee.index,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  builder.instantiate();
})();
