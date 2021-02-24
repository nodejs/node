// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-module-builder.js");

(function TestRegress9832() {
  let builder = new WasmModuleBuilder();
  let f = builder.addFunction("f", kSig_i_i)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprI32Add,
      ]).exportFunc();
  builder.addFunction("main", kSig_i_i)
      .addLocals(kWasmExnRef, 1)
      .addBody([
        kExprTry, kWasmStmt,
          kExprLocalGet, 0,
          kExprCallFunction, f.index,
          kExprCallFunction, f.index,
          kExprLocalSet, 0,
        kExprCatchAll,
          kExprLocalGet, 0,
          kExprCallFunction, f.index,
          kExprLocalSet, 0,
          kExprEnd,
        kExprLocalGet, 0,
      ]).exportFunc();
  let instance = builder.instantiate();
  assertEquals(92, instance.exports.main(23));
})();
