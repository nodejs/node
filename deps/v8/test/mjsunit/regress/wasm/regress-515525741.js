// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-wide-arithmetic --no-liftoff

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testRegress() {
  const builder = new WasmModuleBuilder();

  // Signature:
  // - Parameter 0: i32 condition (controls whether the branch is taken)
  // - Parameters 1 & 2: 128-bit integer A (low, high halves)
  // - Parameters 3 & 4: 128-bit integer B (low, high halves)
  // - Returns: 128-bit integer (low, high halves) result of:
  //     - (A + B) + (A + B) [double of A + B] if condition is true/taken.
  //     - 0 + (A + B)       [regular A + B]    if condition is false/not taken.
  builder.addFunction("add128_dominance", makeSig(
    [kWasmI32, kWasmI64, kWasmI64, kWasmI64, kWasmI64],
    [kWasmI64, kWasmI64]
  ))
    .exportFunc()
    .addLocals(kWasmI64, 2) // Used to pop results from stack inside IF block
    .addBody([
      kExprLocalGet, 0, // Load the condition parameter
      kExprIf, kWasmVoid,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kNumericPrefix, kExprI64Add128, // 1st addition: X = A + B
      kExprLocalSet, 6, // Store high half to local 6 (X_hi)
      kExprLocalSet, 5, // Store low half to local 5 (X_lo)
      kExprEnd,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprLocalGet, 3,
      kExprLocalGet, 4,
      kNumericPrefix, kExprI64Add128, // 2nd addition: Y = A + B
      kExprLocalGet, 5,
      kExprLocalGet, 6,
      kNumericPrefix, kExprI64Add128, // 3rd addition: Y + X
    ]);

  let instance = builder.instantiate();

  let add128_dominance = instance.exports.add128_dominance;
  // Verify execution when the branch is not taken.
  assertEquals([5n, 0n], add128_dominance(0, 2n, 0n, 3n, 0n));
  // Verify execution when the branch is taken (result is doubled).
  assertEquals([10n, 0n], add128_dominance(1, 2n, 0n, 3n, 0n));
})();
