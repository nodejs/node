// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-deopt --allow-natives-syntax --wasm-inlining --liftoff
// Flags: --trace-wasm-inlining

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestDeoptInputcount() {
  var builder = new WasmModuleBuilder();
  let funcRefT = builder.addType(kSig_i_ii);

  builder.addFunction("add", funcRefT)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
    .exportFunc();
  builder.addFunction("mul", funcRefT)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Mul])
    .exportFunc();

  let mainSig = makeSig(
    [kWasmI32, kWasmI32, wasmRefType(funcRefT)], [kWasmI32]);

  builder.addFunction("main", mainSig)
    .addLocals(kWasmI32, 500)
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprCallRef, funcRefT,
  ]).exportFunc();

  let wasm = builder.instantiate().exports;
  assertEquals(2 + 3, wasm.main(2, 3, wasm.add));
  %WasmTierUpFunction(wasm.main);
  assertEquals(2 + 3, wasm.main(2, 3, wasm.add));
  assertTrue(%IsTurboFanFunction(wasm.main));
  // New target on 2nd call_ref.
  assertEquals(2 * 3, wasm.main(2, 3, wasm.mul));
  // A deopt point was never emitted, so the function is still optimized.
  assertTrue(%IsTurboFanFunction(wasm.main));
})();
