// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function ExtractWordAfterByteAdds() {
  const builder = new WasmModuleBuilder();
  function addShuffled(mask) {
    return [
      kExprLocalGet, 1, kExprLocalGet, 1, kExprLocalGet, 1,
      ...SimdInstr(kExprI8x16Shuffle), ...mask,
      ...SimdInstr(kExprI8x16Add), kExprLocalSet, 1
    ];
  }
  builder.addFunction('sum', makeSig([kWasmI32], [kWasmI32]))
      .addLocals(kWasmS128, 1)
      .addBody([
        kExprLocalGet, 0, ...SimdInstr(kExprI32x4Splat), kExprLocalSet, 1,
        ...addShuffled([8,9,10,11,12,13,14,15,1,1,1,1,1,1,1,1]),
        ...addShuffled([4,5,6,7,1,1,1,1,1,1,1,1,1,1,1,1]),
        kExprLocalGet, 1, ...SimdInstr(kExprI32x4ExtractLane), 0
      ]).exportFunc();
  const {sum} = builder.instantiate().exports;
  assertEquals(0, sum(64));
  %WasmTierUpFunction(sum);
  assertEquals(0, sum(64));
  assertEquals(4, sum(65));
})();
