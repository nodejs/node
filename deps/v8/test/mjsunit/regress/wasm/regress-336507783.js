// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-inlining --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestDecompressTaggedParamsAndReturns() {
  const builder = new WasmModuleBuilder();
  const struct = builder.addStruct([
    makeField(kWasmI32, true),
    makeField(kWasmAnyRef, true),
  ]);
  const structRef = wasmRefNullType(struct);

  // Callee returning 1 tagged value (in register) and 4 tagged values (in
  // registers + on the stack).
  const sigSingleRet = builder.addType(makeSig([kWasmI32], [structRef]));
  const sigMultiRet = builder.addType(
      makeSig([kWasmI32], [structRef, structRef, structRef, structRef]));

  // Callee taking 8 tagged parameters (covering both register and stack
  // parameter locations) mixed with untagged parameters.
  // - Some parameters are dereferenced via `struct.get` (requiring full
  //   64-bit pointer decompression).
  // - Other parameters are only stored into a tagged struct field via
  //   `struct.set` or compared via `ref.eq` (exercising the Turboshaft
  //   decompression optimization), including across a call that triggers GC.
  const sigManyParams = builder.addType(makeSig(
      [
        structRef, kWasmI32, structRef, structRef, structRef, structRef,
        structRef, structRef, structRef,
      ],
      [kWasmI32]));

  const impGcAndCount = builder.addImport('m', 'gcAndCount', sigManyParams);

  const makeStruct = builder.addFunction('makeStruct', sigSingleRet)
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, struct,
        kGCPrefix, kExprStructNew, struct,
      ])
      .exportFunc();

  const makeStructs4 = builder.addFunction('makeStructs4', sigMultiRet)
      .addBody([
        kExprLocalGet, 0,
        kExprRefNull, struct,
        kGCPrefix, kExprStructNew, struct,
        kExprLocalGet, 0,
        kExprI32Const, 1,
        kExprI32Add,
        kExprRefNull, struct,
        kGCPrefix, kExprStructNew, struct,
        kExprLocalGet, 0,
        kExprI32Const, 2,
        kExprI32Add,
        kExprRefNull, struct,
        kGCPrefix, kExprStructNew, struct,
        kExprLocalGet, 0,
        kExprI32Const, 3,
        kExprI32Add,
        kExprRefNull, struct,
        kGCPrefix, kExprStructNew, struct,
      ])
      .exportFunc();

  const consumeParams = builder.addFunction('consumeParams', sigManyParams)
      .addBody([
        // Call imported JS function first (triggering GC while incoming stack
        // parameters and spilled compressed/uncompressed parameters are live).
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprLocalGet, 0,
        kExprLocalGet, 6,
        kExprLocalGet, 0,
        kExprLocalGet, 8,
        kExprCallFunction, impGcAndCount,
        kExprDrop,
        // Store param 7 (stack param) and param 2 (register param) into param 0
        // without dereferencing them (compressed uses only, live across the
        // GC-triggering import call above).
        kExprLocalGet, 0,
        kExprLocalGet, 7,
        kGCPrefix, kExprStructSet, struct, 1,
        kExprLocalGet, 3,
        kExprLocalGet, 2,
        kGCPrefix, kExprStructSet, struct, 1,
        // Compare param 4 and param 5 via ref.eq (compressed use only).
        kExprLocalGet, 4,
        kExprLocalGet, 5,
        kExprRefEq,
        // Dereference param 0, param 6 (stack), and param 8 (stack).
        kExprLocalGet, 0,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprI32Add,
        kExprLocalGet, 1,
        kExprI32Add,
        kExprLocalGet, 6,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprI32Add,
        kExprLocalGet, 8,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprI32Add,
      ])
      .exportFunc();

  const tableSingle = builder.addTable(wasmRefNullType(sigSingleRet), 1, 1);
  builder.addActiveElementSegment(
      tableSingle.index, wasmI32Const(0),
      [[kExprRefFunc, makeStruct.index]], wasmRefNullType(sigSingleRet));

  const tableMulti = builder.addTable(wasmRefNullType(sigMultiRet), 1, 1);
  builder.addActiveElementSegment(
      tableMulti.index, wasmI32Const(0),
      [[kExprRefFunc, makeStructs4.index]], wasmRefNullType(sigMultiRet));

  // Indirect tail call (`kExprReturnCallIndirect`) returning a single tagged
  // reference.
  const testTailCallIndirect =
      builder.addFunction('testTailCallIndirect', sigSingleRet)
          .addBody([
            kExprLocalGet, 0,
            kExprI32Const, 0,
            kExprReturnCallIndirect, sigSingleRet, tableSingle.index,
          ])
          .exportFunc();

  // Caller testing direct calls (`kExprCallFunction`, including to a function
  // that performs an indirect tail call) and indirect calls
  // (`kExprCallIndirect`) with both dereferenced and compressed-only uses of
  // single and multi-value tagged returns.
  builder.addFunction('testCalls', makeSig([kWasmI32], [kWasmI32]))
      .addLocals(structRef, 5)
      .addBody([
        // 1. Direct call (`kExprCallFunction`) to a function with an indirect
        // tail call, single tagged return.
        kExprLocalGet, 0,
        kExprCallFunction, testTailCallIndirect.index,
        kExprLocalTee, 1,
        kGCPrefix, kExprStructGet, struct, 0,

        // 2. Indirect call (`kExprCallIndirect`), single tagged return
        // dereferenced.
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kExprCallIndirect, sigSingleRet, tableSingle.index,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprI32Add,

        // 3. Indirect call (`kExprCallIndirect`), single tagged return only
        // stored into a struct field (compressed use optimization).
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kExprCallIndirect, sigSingleRet, tableSingle.index,
        kGCPrefix, kExprStructSet, struct, 1,

        // 4. Indirect call (`kExprCallIndirect`), 4 tagged returns (register +
        // stack return slots), mixing dereferenced and compressed-only uses.
        kExprLocalGet, 0,
        kExprI32Const, 0,
        kExprCallIndirect, sigMultiRet, tableMulti.index,
        kExprLocalSet, 5,  // stack return: dereferenced below
        kExprLocalSet, 4,  // stack return: stored as compressed below
        kExprLocalSet, 3,  // reg return: stored as compressed below
        kExprLocalSet, 2,  // reg return: dereferenced below

        kExprLocalGet, 1,
        kExprLocalGet, 3,
        kGCPrefix, kExprStructSet, struct, 1,
        kExprLocalGet, 2,
        kExprLocalGet, 4,
        kGCPrefix, kExprStructSet, struct, 1,

        kExprLocalGet, 2,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprI32Add,
        kExprLocalGet, 5,
        kGCPrefix, kExprStructGet, struct, 0,
        kExprI32Add,

        // 5. Pass 8 tagged arguments to `consumeParams` (without using locals 3
        // and 4, so locals 3 and 4 remain compressed-only uses).
        kExprLocalGet, 1,
        kExprLocalGet, 0,
        kExprLocalGet, 2,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kExprLocalGet, 2,
        kExprLocalGet, 5,
        kExprLocalGet, 2,
        kExprLocalGet, 5,
        kExprCallFunction, consumeParams.index,
        kExprI32Add,
      ])
      .exportFunc();

  let gcCount = 0;
  function gcAndCount(a0, i1, a2, a3, a4, a5, a6, a7, a8) {
    if (gcCount++ < 4) gc();
    assertSame(a0, a2);
    assertNotNull(a8);
    return i1;
  }

  const instance = builder.instantiate({m: {gcAndCount}});

  // Run in Liftoff first, then tier up to Turboshaft.
  assertEquals(90, instance.exports.testCalls(10));
  const s1 = instance.exports.testTailCallIndirect(42);
  const multi1 = instance.exports.makeStructs4(10);
  assertEquals(4, multi1.length);
  assertEquals(
      128,
      instance.exports.consumeParams(s1, 1, s1, s1, s1, s1, s1, s1, s1));

  %WasmTierUpFunction(instance.exports.consumeParams);
  %WasmTierUpFunction(instance.exports.testCalls);
  %WasmTierUpFunction(instance.exports.testTailCallIndirect);
  %WasmTierUpFunction(instance.exports.makeStruct);
  %WasmTierUpFunction(instance.exports.makeStructs4);

  assertEquals(90, instance.exports.testCalls(10));
  const s2 = instance.exports.testTailCallIndirect(42);
  const multi2 = instance.exports.makeStructs4(20);
  assertEquals(4, multi2.length);
  assertEquals(
      128,
      instance.exports.consumeParams(s2, 1, s2, s2, s2, s2, s2, s2, s2));
})();
