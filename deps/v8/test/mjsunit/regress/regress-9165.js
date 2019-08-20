// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-anyref

load("test/mjsunit/wasm/wasm-module-builder.js");

let kSig_r_i = makeSig([kWasmI32], [kWasmAnyRef]);

(function TestMergeOfAnyFuncIntoAnyRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("merge", kSig_r_i)
      .addLocals({anyref_count: 1, anyfunc_count: 1})
      .addBody([
        kExprGetLocal, 0,
        kExprI32Eqz,
        kExprIf, kWasmAnyRef,
          kExprGetLocal, 1,
        kExprElse,
          kExprGetLocal, 2,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();
  assertEquals(null, instance.exports.merge(0));
  assertEquals(null, instance.exports.merge(1));
})();

(function TestMergeOfAnyFuncIntoNullRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("merge", kSig_r_i)
      .addLocals({anyfunc_count: 1})
      .addBody([
        kExprGetLocal, 0,
        kExprI32Eqz,
        kExprIf, kWasmAnyRef,
          kExprRefNull,
        kExprElse,
          kExprGetLocal, 1,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();
  assertEquals(null, instance.exports.merge(0));
  assertEquals(null, instance.exports.merge(1));
})();
