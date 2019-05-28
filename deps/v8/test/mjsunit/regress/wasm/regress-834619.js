// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-lazy-compilation

load("test/mjsunit/wasm/wasm-module-builder.js");

(function ExportedFunctionsImportedOrder() {
  print(arguments.callee.name);

  let i1 = (() => {
    let builder = new WasmModuleBuilder();
    builder.addFunction("f1", kSig_i_v)
      .addBody(
        [kExprI32Const, 1])
      .exportFunc();
    builder.addFunction("f2", kSig_i_v)
      .addBody(
        [kExprI32Const, 2])
      .exportFunc();
    return builder.instantiate();
  })();

  let i2 = (() => {
    let builder = new WasmModuleBuilder();
    builder.addImport("q", "f2", kSig_i_v);
    builder.addImport("q", "f1", kSig_i_v);
    builder.addFunction("main", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprCallIndirect, 0, kTableZero
      ])
      .exportFunc();
    builder.addElementSegment(0, 0, false, [0, 1, 1, 0]);

    return builder.instantiate({q: {f2: i1.exports.f2, f1: i1.exports.f1}});
  })();

  print("--->calling 0");
  assertEquals(2, i2.exports.main(0));
  print("--->calling 1");
  assertEquals(1, i2.exports.main(1));
  print("--->calling 2");
  assertEquals(1, i2.exports.main(2));
  print("--->calling 3");
  assertEquals(2, i2.exports.main(3));
})();
