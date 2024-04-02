// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestWasmI64ToJSBigIntImportedFunc() {
  var builder = new WasmModuleBuilder();

  var a_index = builder
    .addImport("a", "a", kSig_l_l) // i64 -> i64

  builder
    .addFunction("fn", kSig_l_v) // () -> i64
    .addBody([
      kExprI64Const, 0x7,
      kExprCallFunction, a_index
    ])
    .exportFunc();

  a_was_called = false;

  var module = builder.instantiate({
    a: {
      a(param) {
        assertEquals(typeof param, "bigint");
        assertEquals(param, 7n);
        a_was_called = true;
        return 12n;
      },
    }
  });

  assertEquals(module.exports.fn(), 12n);

  assertTrue(a_was_called);
})();
