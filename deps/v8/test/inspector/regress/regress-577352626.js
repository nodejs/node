// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --liftoff

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/577352626.');

contextGroup.addScript(utils.read('test/mjsunit/wasm/wasm-module-builder.js'));

contextGroup.addScript(`
function setup() {
  console.log('setup');
  inspector.runNestedMessageLoop();
}

function jsCaller() {
  return instanceA.exports.goAllInlined(1);
}

function jsCallerViaWasm() {
  return instanceA.exports.wasmCaller(1);
}

const builderA = new WasmModuleBuilder();
const sig = builderA.addType(kSig_i_v);
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

// Case 1: 'go' has a speculatively-inlined call_indirect before the loop and a
// non-inlined call_indirect after the loop. The pause at the loop stack check
// must be deferred to the non-inlined Liftoff callee '$f'.
builderA.addFunction('go', kSig_i_i)
  .addLocals(kWasmI32, 1)
  .addBody([
    kExprLocalGet, 0,
    kExprIf, kWasmVoid,
      kExprCallFunction, $setup,
    kExprEnd,
    // Call #1: monomorphic (A.f), speculatively inlined by Turboshaft.
    kExprI32Const, 0,
    kExprCallIndirect, sig, table.index,
    kExprDrop,
    // Loop with stack check where the pending Debugger.pause interrupt fires.
    kExprI32Const, 5,
    kExprLocalSet, 1,
    kExprLoop, kWasmVoid,
      kExprLocalGet, 1,
      kExprI32Const, 1,
      kExprI32Sub,
      kExprLocalTee, 1,
      kExprBrIf, 0,
    kExprEnd,
    // Store at ByteArray offset 28, invalidating only the cached target.
    kExprI32Const, 1,
    kExprGlobalSet, collide.index,
    // Call #2: must not use a stale cached implicit_arg with a mutated target.
    kExprLocalGet, 0,
    kExprIf, kWasmI32,
      kExprI32Const, 0,
      kExprCallIndirect, sig, table.index,
    kExprElse,
      kExprI32Const, 0,
    kExprEnd,
  ]).exportFunc();

// Case 2: 'goAllInlined' warms up both call_indirect sites so both are
// speculatively inlined. When it makes no further calls, the pause at the loop
// stack check must be deferred until it returns to its caller frame.
const goAllInlined = builderA.addFunction('goAllInlined', kSig_i_i)
  .addLocals(kWasmI32, 1)
  .addBody([
    kExprLocalGet, 0,
    kExprIf, kWasmVoid,
      kExprCallFunction, $setup,
    kExprEnd,
    kExprI32Const, 0,
    kExprCallIndirect, sig, table.index,
    kExprDrop,
    kExprI32Const, 5,
    kExprLocalSet, 1,
    kExprLoop, kWasmVoid,
      kExprLocalGet, 1,
      kExprI32Const, 1,
      kExprI32Sub,
      kExprLocalTee, 1,
      kExprBrIf, 0,
    kExprEnd,
    kExprI32Const, 1,
    kExprGlobalSet, collide.index,
    kExprI32Const, 0,
    kExprCallIndirect, sig, table.index,
  ]).exportFunc();

builderA.addFunction('wasmCaller', kSig_i_i)
  .addBody([
    kExprLocalGet, 0,
    kExprCallFunction, goAllInlined.index,
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
`);

Protocol.Debugger.onPaused(async message => {
  const topFrame = message.params.callFrames[0];
  InspectorTest.log('paused in: ' + topFrame.functionName);
  await Protocol.Runtime.evaluate({
    expression: 'instanceA.exports.T.set(0, instanceC.exports.g)',
  });
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Runtime.enable();

  // Test 1: Pause is deferred from optimized '$go' to Liftoff callee '$f'.
  let consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  let evalPromise = Protocol.Runtime.evaluate({
    expression: 'instanceA.exports.go(1)',
    returnByValue: true,
  });

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  let {result: {result}} = await evalPromise;
  InspectorTest.log('go(1) returned: ' + result.value);
  await Protocol.Debugger.disable();

  // Reset T[0] back to A.f, collect call_indirect feedback for both sites, and
  // tier up 'goAllInlined' synchronously.
  await Protocol.Runtime.evaluate({
    expression: `
      instanceA.exports.T.set(0, instanceA.exports.f);
      instanceA.exports.goAllInlined(0);
      %WasmTierUpFunction(instanceA.exports.goAllInlined);
    `,
  });

  // Test 2: When both calls in '$goAllInlined' are inlined and it makes no
  // further calls, pause is deferred until return to its JS caller ('jsCaller').
  consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  evalPromise = Protocol.Runtime.evaluate({
    expression: 'jsCaller()',
    returnByValue: true,
  });

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  ({result: {result}} = await evalPromise);
  InspectorTest.log('jsCaller() returned: ' + result.value);
  await Protocol.Debugger.disable();

  // Reset T[0] back to A.f and ensure 'goAllInlined' is tiered up, leaving
  // 'wasmCaller' in non-debugging Liftoff so flooding skips '$wasmCaller' and
  // floods 'jsCallerViaWasm'.
  await Protocol.Runtime.evaluate({
    expression: `
      instanceA.exports.T.set(0, instanceA.exports.f);
      instanceA.exports.goAllInlined(0);
      %WasmTierUpFunction(instanceA.exports.goAllInlined);
    `,
  });

  // Test 3: Same as Test 2, with a non-inspectable Wasm caller in between.
  consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  evalPromise = Protocol.Runtime.evaluate({
    expression: 'jsCallerViaWasm()',
    returnByValue: true,
  });

  await consolePromise;
  await Protocol.Debugger.enable();
  Protocol.Debugger.pause();
  await Protocol.Runtime.runIfWaitingForDebugger();

  ({result: {result}} = await evalPromise);
  InspectorTest.log('jsCallerViaWasm() returned: ' + result.value);
  InspectorTest.completeTest();
})();
