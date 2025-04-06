// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-deopt --allow-natives-syntax --wasm-inlining --liftoff
// Flags: --trace-wasm-inlining --trace-deopt --wasm-deopts-per-function-limit=1

d8.file.execute("test/mjsunit/mjsunit.js");
d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestDeoptTracing() {
  var builder = new WasmModuleBuilder();
  let funcRefT = builder.addType(kSig_i_ii);

  builder.addFunction("add", funcRefT)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Add])
    .exportFunc();
  builder.addFunction("mul", funcRefT)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Mul])
    .exportFunc();
  builder.addFunction("sub", funcRefT)
    .addBody([kExprLocalGet, 0, kExprLocalGet, 1, kExprI32Sub])
    .exportFunc();

  let mainSig = makeSig(
    [kWasmI32, kWasmI32, wasmRefType(funcRefT)], [kWasmI32]);

  builder.addFunction("main", mainSig)
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
  assertFalse(%IsTurboFanFunction(wasm.main));
  // Trigger tier-up again. This disable using deoptimizations as the limit is
  // reached.
  %WasmTierUpFunction(wasm.main);
  assertEquals(2 + 3, wasm.main(2, 3, wasm.add));
  assertEquals(2 * 3, wasm.main(2, 3, wasm.mul));
  assertEquals(-1, wasm.main(2, 3, wasm.sub));
  assertTrue(%IsTurboFanFunction(wasm.main));
})();
