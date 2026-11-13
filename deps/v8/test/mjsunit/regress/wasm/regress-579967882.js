// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --wasm-shared --stress-incremental-marking
// Flags: --no-concurrent-marking

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

(function TestAtomicStoreTaggedPointerWriteBarrier() {
  const builder = new WasmModuleBuilder();
  const nodeType = builder.addStruct({
    fields: [
      makeField(wasmRefNullType(0), true),
      makeField(kWasmI32, true),
    ],
    shared: true,
  });

  const blackGlobal = builder.addGlobal(
      wasmRefNullType(nodeType), true, false, [kExprRefNull, nodeType]);
  const dummyGlobal = builder.addGlobal(
      wasmRefNullType(nodeType), true, false, [kExprRefNull, nodeType]);

  builder.addFunction('initGlobals', makeSig([], []))
    .addBody([
      kExprRefNull, nodeType,
      kExprI32Const, 1,
      kGCPrefix, kExprStructNew, nodeType,
      kExprGlobalSet, blackGlobal.index,
      kExprRefNull, nodeType,
      kExprI32Const, 2,
      kGCPrefix, kExprStructNew, nodeType,
      kExprGlobalSet, dummyGlobal.index,
    ])
    .exportFunc();

  builder.addFunction('makeNode',
      makeSig([wasmRefNullType(nodeType), kWasmI32], [wasmRefType(nodeType)]))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kGCPrefix, kExprStructNew, nodeType,
    ])
    .exportFunc();

  // Shade `blackGlobal` via `struct.set` and walk `flushChain` (100 nodes) so
  // `MarkingBarrier`'s 64-entry local worklist segment is flushed to the
  // global marking worklist.
  builder.addFunction('shadeBlackGlobal',
      makeSig([wasmRefNullType(nodeType)], []))
    .addBody([
      kExprGlobalGet, dummyGlobal.index,
      kExprGlobalGet, blackGlobal.index,
      kGCPrefix, kExprStructSet, nodeType, 0,
      kExprBlock, kWasmVoid,
        kExprLoop, kWasmVoid,
          kExprLocalGet, 0,
          kExprRefIsNull,
          kExprBrIf, 1,
          kExprGlobalGet, dummyGlobal.index,
          kExprLocalGet, 0,
          kGCPrefix, kExprStructSet, nodeType, 0,
          kExprLocalGet, 0,
          kGCPrefix, kExprStructGet, nodeType, 0,
          kExprLocalSet, 0,
          kExprBr, 0,
        kExprEnd,
      kExprEnd,
    ])
    .exportFunc();

  // Read `whiteObj = whiteHolder.next`, clear `whiteHolder.next = null`, and
  // store `whiteObj` into `blackGlobal.next` via `struct.atomic.set`.
  builder.addFunction('moveWhiteToBlack',
      makeSig([wasmRefNullType(nodeType)], []))
    .addLocals(wasmRefNullType(nodeType), 1)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, nodeType, 0,
      kExprLocalSet, 1,
      kExprLocalGet, 0,
      kExprRefNull, nodeType,
      kGCPrefix, kExprStructSet, nodeType, 0,
      kExprGlobalGet, blackGlobal.index,
      kExprLocalGet, 1,
      kAtomicPrefix, kExprStructAtomicSet, kAtomicSeqCst, nodeType, 0,
    ])
    .exportFunc();

  const wasm = builder.instantiate().exports;
  wasm.initGlobals();

  // Pre-compile all Wasm functions before GC so lazy compilation does not
  // trigger during incremental marking.
  const warmup = wasm.makeNode(null, 0);
  wasm.shadeBlackGlobal(warmup);
  wasm.moveWhiteToBlack(warmup);

  let flushChain = null;
  for (let i = 0; i < 100; i++) {
    flushChain = wasm.makeNode(flushChain, i);
  }

  const whiteHolder = wasm.makeNode(wasm.makeNode(null, 42), 3);

  gc({type: 'major'});

  for (let i = 0; i < 10; i++) {
    wasm.makeNode(null, i);
  }

  wasm.shadeBlackGlobal(flushChain);
  for (let i = 0; i < 20; i++) {
    new Array(1000);
  }

  wasm.moveWhiteToBlack(whiteHolder);

  gc({type: 'major'});
})();
