// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-wasm-trap-handler
// Flags: --stress-wasm-memory-moving

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/581361633.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
const builder = new WasmModuleBuilder();
builder.addMemory(1, 10);
builder.exportMemoryAs('mem');
const structType = builder.addStruct([makeField(kWasmI32, true)]);
const globalStruct =
    builder.addGlobal(wasmRefNullType(structType), true, false);
const tag = builder.addTag(kSig_v_v);
const setupIdx = builder.addImport('env', 'setup', kSig_v_v);
const intToStringIdx = builder.addImport(
    'env', 'intToString',
    makeSig([kWasmI32, kWasmI32], [kWasmExternRef]));

builder.addFunction('mutateStruct', kSig_v_i)
    .addBody([
      kExprGlobalGet, globalStruct.index,
      kExprLocalGet, 0,
      kGCPrefix, kExprStructSet, structType, 0,
    ])
    .exportFunc();

builder.addFunction('probe', kSig_i_v)
    .addLocals(wasmRefNullType(structType), 1)
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprCallFunction, setupIdx,

      // Allocate a fresh struct, store it in a global, and read its field
      // before the first try_table so WasmLoadEliminationReducer caches
      // field 0 = 10.
      kExprI32Const, 10,
      kGCPrefix, kExprStructNew, structType,
      kExprLocalTee, 0,
      kExprGlobalSet, globalStruct.index,
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, structType, 0,
      kExprDrop,

      // Pre-throw memory access so LateLoadEliminationReducer caches
      // memory0_start before the first try_table.
      kExprI32Const, 0,
      kExprI32Const, 0x11,
      kExprI32StoreMem, 2, 0,

      // 1. Throw via WasmThrow inside try_table.
      kExprBlock, kWasmVoid,
        kExprTryTable, kWasmVoid, 1,
        kCatchAllNoRef, 0,
          kExprThrow, tag,
        kExprEnd,
      kExprEnd,

      // Post-catch memory access and struct field load after WasmThrow:
      // both must reload after the catch block (and this also re-caches
      // memory0_start and field 0 before the second try_table).
      kExprI32Const, 0,
      kExprI32Const, 0x22,
      kExprI32StoreMem, 2, 0,
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, structType, 0,
      kExprLocalSet, 1,

      // 2. Throw via well-known import WasmIntToString (invalid radix 37)
      // inside try_table.
      kExprBlock, kWasmVoid,
        kExprTryTable, kWasmVoid, 1,
        kCatchAllNoRef, 0,
          kExprI32Const, 42,
          kExprI32Const, 37,
          kExprCallFunction, intToStringIdx,
          kExprDrop,
        kExprEnd,
      kExprEnd,

      // Post-catch memory access and struct field load after WasmIntToString.
      kExprI32Const, 0,
      kExprI32Const, 0x33,
      kExprI32StoreMem, 2, 0,
      kExprLocalGet, 1,
      ...wasmI32Const(100),
      kExprI32Mul,
      kExprLocalGet, 0,
      kGCPrefix, kExprStructGet, structType, 0,
      kExprI32Add,
    ])
    .exportFunc();

function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

const intToString = Function.prototype.call.bind(Number.prototype.toString);
const instance = builder.instantiate({env: {setup, intToString}});
const mem = instance.exports.mem;
%WasmTierUpFunction(instance.exports.probe);

function runProbe() {
  const res = instance.exports.probe();
  return res * 100 + new Uint8Array(mem.buffer)[0];
}
`);

let pauseCount = 0;
Protocol.Debugger.onPaused(async () => {
  pauseCount++;
  const newFieldVal = pauseCount === 1 ? 40 : 50;
  await Protocol.Runtime.evaluate({
    expression: `mem.grow(1); instance.exports.mutateStruct(${newFieldVal});`
  });
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise = Protocol.Runtime.evaluate({expression: 'runProbe()'});

  await consolePromise;
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.runIfWaitingForDebugger();

  const {result} = await evalPromise;
  InspectorTest.log('pauseCount: ' + pauseCount);
  InspectorTest.log('runProbe() -> ' + result.result.value);
  InspectorTest.completeTest();
})();
