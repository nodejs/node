// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --liftoff

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/580839926.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

function run() {
  return instanceA.exports.go(1);
}

const builderA = new WasmModuleBuilder();
const sig = builderA.addType(kSig_i_v);
const tag = builderA.addTag(kSig_v_v);
const $setup = builderA.addImport('env', 'setup', kSig_v_v);
const table = builderA.addTable(kWasmAnyFunc, 1, 1).exportAs('T');

// Place 'collide' at untagged globals offset 20 (ByteArray offset 28), which
// matches WasmDispatchTable::kEntriesOffset + WasmDispatchTable::kTargetBias.
builderA.addGlobal(kWasmI64, true, false, wasmI64Const(0x41414141n));
builderA.addGlobal(kWasmI64, true, false, wasmI64Const(0x42424242n));
builderA.addGlobal(kWasmI32, true, false, wasmI32Const(0x43434343));
const collide = builderA.addGlobal(kWasmI32, true, false, wasmI32Const(0));

const f = builderA.addFunction('f', sig)
  .addBody([kExprI32Const, 11])
  .exportFunc();

builderA.addActiveElementSegment(table.index, wasmI32Const(0), [f.index]);

builderA.addFunction('go', kSig_i_i)
  .addBody([
    kExprLocalGet, 0,
    kExprIf, kWasmVoid,
      kExprCallFunction, $setup,
    kExprEnd,
    // Call #1: monomorphic (A.f), speculatively inlined by Turboshaft.
    kExprI32Const, 0,
    kExprCallIndirect, sig, table.index,
    kExprDrop,
    // Throw inside try_table (catch_all) while the optimized Wasm frame is on
    // top of the stack. Debug::OnException must not pause on this non-Liftoff
    // Wasm frame.
    kExprLocalGet, 0,
    kExprIf, kWasmVoid,
      kExprBlock, kWasmVoid,
        kExprTryTable, kWasmVoid, 1,
        kCatchAllNoRef, 0,
          kExprThrow, tag,
        kExprEnd,
      kExprEnd,
    kExprEnd,
    // Store at ByteArray offset 28, colliding with WasmDispatchTable target.
    kExprI32Const, 1,
    kExprGlobalSet, collide.index,
    // Call #2: must not use a mutated table entry from an exception pause.
    kExprLocalGet, 0,
    kExprIf, kWasmI32,
      kExprI32Const, 0,
      kExprCallIndirect, sig, table.index,
    kExprElse,
      kExprI32Const, 0,
    kExprEnd,
  ]).exportFunc();

const instanceA = builderA.instantiate({env: {setup}});

const builderC = new WasmModuleBuilder();
const sigC = builderC.addType(kSig_i_v);
const g0C = builderC.addGlobal(kWasmI32, true, false, wasmI32Const(0x1337));
builderC.addFunction('g', sigC)
  .addBody([kExprGlobalGet, g0C.index])
  .exportFunc();
const instanceC = builderC.instantiate();

// Collect call_indirect feedback in Liftoff and tier up 'go' synchronously.
instanceA.exports.go(0);
%WasmTierUpFunction(instanceA.exports.go);

let prepareStackTraceCalls = 0;
Error.prepareStackTrace = () => {
  prepareStackTraceCalls++;
  instanceA.exports.T.set(0, instanceC.exports.g);
  instanceC.exports.g();
  return 'stack';
};
`);

Protocol.Debugger.onPaused(async message => {
  const topFrame = message.params.callFrames[0];
  InspectorTest.log('paused in: ' + topFrame.functionName);
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  const evalPromise = Protocol.Runtime.evaluate({
    expression: 'run.call(new Error("seed"))',
    returnByValue: true,
  });

  await consolePromise;
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.runIfWaitingForDebugger();

  const {result: {result}} = await evalPromise;
  InspectorTest.log('go(1) returned: ' + result.value);
  const {result: {result: calls}} = await Protocol.Runtime.evaluate({
    expression: 'prepareStackTraceCalls',
    returnByValue: true,
  });
  InspectorTest.log('prepareStackTraceCalls: ' + calls.value);
  InspectorTest.completeTest();
})();
