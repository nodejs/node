// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function Issue490183533() {
  print(arguments.callee.name);

  const builder = new WasmModuleBuilder();
  let main = builder.addFunction("main", kSig_f_lff)
    .exportFunc()
    .addBody([
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 0,
      ...wasmI64Const(BigInt(1) << BigInt(32)),
      kExprI64GeU,
      kExprSelect,
  ]);
  const instance = builder.instantiate({});

  %WasmTierUpFunction(instance.exports.main);

  const expected = 13.0;
  const param = (BigInt(1) << BigInt(48)) + BigInt(666);
  const result = instance.exports.main(param, expected, 42.0);

  assertTrue(result === expected);
})();
