// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-jitless

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function assertTrapAtOffset(call, expectedOffset) {
  let error;
  try {
    call();
  } catch (e) {
    error = e;
  }
  assertInstanceof(error, WebAssembly.RuntimeError);
  const match = /wasm-function\[0\]:0x([0-9a-f]+)/.exec(error.stack || "");
  assertNotNull(match);
  assertEquals(expectedOffset, parseInt(match[1], 16));
}

(function testFusedStoreTrapOffsets() {
  const bigOffset = [0xf0, 0xff, 0xff, 0x0f];

  for (const [load, store, width] of [
         [kExprI32LoadMem, kExprI32StoreMem, 4],
         [kExprI64LoadMem, kExprI64StoreMem, 8],
         [kExprF32LoadMem, kExprF32StoreMem, 4],
         [kExprF64LoadMem, kExprF64StoreMem, 8]]) {
    for (const memory64 of [false, true]) {
      for (const registerAddress of [false, true]) {
        for (const [loadTraps, storeTraps] of [
               [false, true], [true, false], [true, true], [false, false]]) {
          const builder = new WasmModuleBuilder();
          if (memory64) {
            builder.addMemory64(1, 1);
          } else {
            builder.addMemory(1, 1);
          }
          builder.exportMemoryAs("memory");
          const addressType = memory64 ? kWasmI64 : kWasmI32;
          const addressConst = memory64 ? kExprI64Const : kExprI32Const;
          const body = [addressConst, 16, addressConst, 0];
          if (registerAddress) {
            body.push(kExprLocalGet, 0,
                      memory64 ? kExprI64Add : kExprI32Add);
          }
          const loadOffset = body.length;
          body.push(load, 0, ...(loadTraps ? bigOffset : [0]));
          const storeOffset = body.length;
          body.push(store, 0, ...(storeTraps ? bigOffset : [0]));
          const func = builder.addFunction(
              "main", makeSig([addressType], []))
              .addBody(body)
              .exportFunc();
          const instance = builder.instantiate();
          const memory = new Uint8Array(instance.exports.memory.buffer);
          memory.set([1, 2, 3, 4, 5, 6, 7, 8]);
          if (loadTraps || storeTraps) {
            assertTrapAtOffset(
                () => instance.exports.main(memory64 ? 0n : 0),
                func.body_offset + (loadTraps ? loadOffset : storeOffset));
            assertEquals(new Array(width).fill(0),
                         Array.from(memory.slice(16, 16 + width)));
          } else {
            instance.exports.main(memory64 ? 0n : 0);
            assertEquals(Array.from(memory.slice(0, width)),
                         Array.from(memory.slice(16, 16 + width)));
          }
        }
      }
    }
  }

  const localStoreBuilder = new WasmModuleBuilder();
  localStoreBuilder.addMemory(1, 1);
  const localStoreFunction = localStoreBuilder.addFunction("main", kSig_v_i)
      .addBody([
        kExprI32Const, 0,
        kExprLocalGet, 0,
        kExprI32StoreMem, 0, ...bigOffset,
      ])
      .exportFunc();
  const localStoreInstance = localStoreBuilder.instantiate();
  assertTrapAtOffset(
      () => localStoreInstance.exports.main(0x7f),
      localStoreFunction.body_offset + 4);
})();

(function testFusedLoadAddressOverflow() {
  for (const [load, store] of [
         [kExprI32LoadMem, kExprI32StoreMem],
         [kExprI64LoadMem, kExprI64StoreMem],
         [kExprF32LoadMem, kExprF32StoreMem],
         [kExprF64LoadMem, kExprF64StoreMem]]) {
    for (const registerAddress of [false, true]) {
      const builder = new WasmModuleBuilder();
      builder.addMemory64(1, 1);
      const body = [kExprI64Const, 16, kExprI64Const, 0x7f];
      if (registerAddress) {
        body.push(kExprI64Const, 0, kExprI64Add);
      }
      // UINT64_MAX + offset 1 wraps to an otherwise in-bounds address.
      const loadOffset = body.length;
      body.push(load, 0, 1, store, 0, 0);
      const func = builder.addFunction("main", kSig_v_v)
          .addBody(body)
          .exportFunc();
      const instance = builder.instantiate();
      assertTrapAtOffset(
          () => instance.exports.main(), func.body_offset + loadOffset);
    }
  }
})();

(function testArraySegmentTrapOffsets() {
  const newDataBuilder = new WasmModuleBuilder();
  const newDataArray = newDataBuilder.addArray(kWasmI32, {mutable: true});
  const newDataSegment =
      newDataBuilder.addPassiveDataSegment([0, 1, 2, 3, 4, 5]);
  const newDataFunction = newDataBuilder.addFunction("main", kSig_v_v)
      .addBody([
        kExprI32Const, 1,
        kExprI32Const, 3,
        kGCPrefix, kExprArrayNewData, newDataArray, newDataSegment,
        kExprDrop,
      ])
      .exportFunc();
  const newDataInstance = newDataBuilder.instantiate();
  assertTrapAtOffset(
      () => newDataInstance.exports.main(),
      newDataFunction.body_offset + 4);

  const initDataBuilder = new WasmModuleBuilder();
  const initDataArray = initDataBuilder.addArray(kWasmI32, {mutable: true});
  const initDataSegment =
      initDataBuilder.addPassiveDataSegment([0, 1, 2, 3, 4, 5]);
  const initDataFunction = initDataBuilder.addFunction("main", kSig_v_v)
      .addBody([
        kExprI32Const, 4,
        kGCPrefix, kExprArrayNewDefault, initDataArray,
        kExprI32Const, 0,
        kExprI32Const, 1,
        kExprI32Const, 3,
        kGCPrefix, kExprArrayInitData, initDataArray, initDataSegment,
      ])
      .exportFunc();
  const initDataInstance = initDataBuilder.instantiate();
  assertTrapAtOffset(
      () => initDataInstance.exports.main(),
      initDataFunction.body_offset + 11);
})();
