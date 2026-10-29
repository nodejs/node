// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const dv = new DataView(new ArrayBuffer(10));
const getUint32Bound = Function.prototype.call.bind(DataView.prototype.getUint32);

// Test case 1: Simple ReturnCall to WKI inside try-catch.
(function testSimple() {
  let builder = new WasmModuleBuilder();
  let sig_i_eii = makeSig([kWasmExternRef, kWasmI32, kWasmI32], [kWasmI32]);
  let kGetUint32 = builder.addImport('env', 'getUint32', sig_i_eii);
  let f_b = builder.addFunction("b", sig_i_eii)
    .exportFunc()
    .addBody([
      kExprTry, kWasmI32,
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprReturnCall, kGetUint32,
      kExprEnd
    ]);
  let instance = builder.instantiate({ env: { getUint32: getUint32Bound } });

  instance.exports.b(dv, 0, 1);
  %WasmTierUpFunction(instance.exports.b);
  instance.exports.b(dv, 0, 1);
})();

// Test case 2: Nested inlining.
(function testNestedInlining() {
  let builder = new WasmModuleBuilder();
  let sig_i_eii = makeSig([kWasmExternRef, kWasmI32, kWasmI32], [kWasmI32]);
  let kGetUint32 = builder.addImport('env', 'getUint32', sig_i_eii);

  // f2: tail-calls WKI inside try. Should NOT catch exception.
  let f2 = builder.addFunction("f2", sig_i_eii)
    .addBody([
      kExprTry, kWasmI32,
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprReturnCall, kGetUint32,
      kExprCatchAll,
        kExprI32Const, 42, // Return 42 if caught (incorrect)
      kExprEnd
    ]);

  // f1: calls f2 inside try. SHOULD catch exception.
  let f1 = builder.addFunction("f1", sig_i_eii)
    .exportFunc()
    .addBody([
      kExprTry, kWasmI32,
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprCallFunction, f2.index,
      kExprCatchAll,
        kExprI32Const, 43, // Return 43 if caught (correct)
      kExprEnd
    ]);

  let instance = builder.instantiate({ env: { getUint32: getUint32Bound } });

  // 1. Path without exceptions
  assertEquals(0, instance.exports.f1(dv, 0, 1));

  // 2. Path with exception (should return 43)
  assertEquals(43, instance.exports.f1(null, 0, 1));

  // Tier up f1
  %WasmTierUpFunction(instance.exports.f1);

  // 3. Verify behavior after Turboshaft inlining
  assertEquals(0, instance.exports.f1(dv, 0, 1));
  assertEquals(43, instance.exports.f1(null, 0, 1));
})();
