// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-fast-api --wasm-fast-api

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// Regression test for chromium:531123009.
// WellKnown_FastApi's value_out_of_range fallback called BuildWasmCall
// without forwarding check_for_exception, defaulting to kCatchInThisFrame.
// Inside a return_call with a try block, this created exception-routing
// blocks to a catch block that the decoder never finalized.

const fast_c_api = new d8.test.FastCAPI();
const sumInt64Bound =
    Function.prototype.call.bind(fast_c_api.sum_int64_as_number);

(function testReturnCallFastApi() {
  let builder = new WasmModuleBuilder();
  let sig = makeSig([kWasmExternRef, kWasmF64, kWasmF64], [kWasmF64]);
  let kSumInt64 = builder.addImport('env', 'sum_int64_as_number', sig);
  let f_b = builder.addFunction("b", sig)
    .exportFunc()
    .addBody([
      kExprTry, kWasmF64,
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprReturnCall, kSumInt64,
      kExprEnd
    ]);
  let instance =
      builder.instantiate({ env: { sum_int64_as_number: sumInt64Bound } });
  %WasmTierUpFunction(instance.exports.b);
})();
