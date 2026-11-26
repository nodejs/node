// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testArraySegmentSmiBoundaryTrapOrdering() {
  const builder = new WasmModuleBuilder();
  const array = builder.addArray(kWasmI16, {mutable: true});
  const dataSegment = builder.addPassiveDataSegment([0, 1, 2, 3, 4, 5, 6, 7]);
  const invalidSmi = 0x40000000;
  const maxLength = 536870900;

  function addInitData(name, srcOffset, size) {
    builder.addFunction(name, kSig_v_v)
        .addBody([
          ...wasmI32Const(1),
          kGCPrefix, kExprArrayNewDefault, array,
          ...wasmI32Const(0),
          ...wasmI32Const(srcOffset),
          ...wasmI32Const(size),
          kGCPrefix, kExprArrayInitData, array, dataSegment,
        ])
        .exportFunc();
  }
  addInitData("init_data_src_invalid", invalidSmi, 1);
  addInitData("init_data_size_invalid", 1, invalidSmi);
  addInitData("init_data_order", invalidSmi, invalidSmi);

  for (const [name, length] of [
         ["new_data_below", maxLength - 1],
         ["new_data_at", maxLength],
         ["new_data_above", maxLength + 1]]) {
    builder.addFunction(name, kSig_v_v)
        .addBody([
          ...wasmI32Const(0),
          ...wasmI32Const(length),
          kGCPrefix, kExprArrayNewData, array, dataSegment,
          kExprDrop,
        ])
        .exportFunc();
  }

  const instance = builder.instantiate();
  assertTraps(kTrapDataSegmentOutOfBounds,
              () => instance.exports.init_data_src_invalid());
  assertTraps(kTrapArrayOutOfBounds,
              () => instance.exports.init_data_size_invalid());
  assertTraps(kTrapDataSegmentOutOfBounds,
              () => instance.exports.init_data_order());
  assertTraps(kTrapDataSegmentOutOfBounds,
              () => instance.exports.new_data_below());
  assertTraps(kTrapDataSegmentOutOfBounds,
              () => instance.exports.new_data_at());
  assertTraps(kTrapArrayTooLarge,
              () => instance.exports.new_data_above());
})();
