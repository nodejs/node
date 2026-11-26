// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --wasm-in-js-inlining-body --no-maglev
// Flags: --wasm-in-js-inlining-opt --turbo-inline-js-wasm-calls

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Wasm breakpoints and instrumentation breakpoints with ' +
    'Wasm-in-JS body inlining.');
session.setupScriptMap();

const builder1 = new WasmModuleBuilder();
const struct1 = builder1.addStruct([makeField(kWasmI32, true)]);
builder1.addFunction('createStruct', makeSig([kWasmI32], [kWasmExternRef]))
    .addBody([
      kExprLocalGet,
      0,
      kGCPrefix,
      kExprStructNew,
      struct1,
      kGCPrefix,
      kExprExternConvertAny,
    ])
    .exportFunc();
const getFieldFunc =
    builder1.addFunction('getField', makeSig([kWasmExternRef], [kWasmI32]))
        .addBody([
          kExprLocalGet,
          0,
          kGCPrefix,
          kExprAnyConvertExtern,
          kGCPrefix,
          kExprRefCast,
          struct1,
          kGCPrefix,
          kExprStructGet,
          struct1,
          0,
        ])
        .exportFunc();
const wasmBytes1 = JSON.stringify(builder1.toArray());

const builder2 = new WasmModuleBuilder();
const struct2 = builder2.addStruct([makeField(kWasmI32, true)]);
builder2
    .addFunction(
        'getFieldWithDummy', makeSig([kWasmExternRef, kWasmI32], [kWasmI32]))
    .addBody([
      kExprLocalGet,
      1,
      kExprDrop,
      kExprLocalGet,
      0,
      kGCPrefix,
      kExprAnyConvertExtern,
      kGCPrefix,
      kExprRefCast,
      struct2,
      kGCPrefix,
      kExprStructGet,
      struct2,
      0,
    ])
    .exportFunc();
const wasmBytes2 = JSON.stringify(builder2.toArray());

const source = `
const wasmBytes1 = new Uint8Array(${wasmBytes1});
const module1 = new WebAssembly.Module(wasmBytes1);
const instance1 = new WebAssembly.Instance(module1, {});
const getField1 = instance1.exports.getField;
const s = instance1.exports.createStruct(42);

function isOptimized(fn) {
  return (%GetOptimizationStatus(fn) & (1 << 3)) !== 0;
}

function js_fn1(x) {
  return getField1(x);
}

function js_fn2(x) {
  return getField1(x);
}

function js_fn3(x) {
  return getField1(x);
}

function optimize(fn, arg) {
  %PrepareFunctionForOptimization(fn);
  for (let i = 0; i < 10; ++i) fn(arg);
  %OptimizeFunctionOnNextCall(fn);
  return fn(arg);
}

const wasmBytes2 = new Uint8Array(${wasmBytes2});
const module2Pre = new WebAssembly.Module(wasmBytes2);
const instance2Pre = new WebAssembly.Instance(module2Pre, {});
const getField2Pre = instance2Pre.exports.getFieldWithDummy;
function js_fn_instr_pre(x, y) {
  return getField2Pre(x, y);
}
function optimizeInstrPre() {
  %PrepareFunctionForOptimization(js_fn_instr_pre);
  for (let i = 0; i < 10; ++i) js_fn_instr_pre(s, 0);
  %OptimizeFunctionOnNextCall(js_fn_instr_pre);
  return js_fn_instr_pre(s, 0);
}

let instance2;
let getField2;
function instantiateModule2() {
  const m = new WebAssembly.Module(wasmBytes2);
  instance2 = new WebAssembly.Instance(m, {});
  getField2 = instance2.exports.getFieldWithDummy;
}
function js_fn_instr(x, y) {
  return getField2(x, y);
}
function warmUpInstrWithSymbol() {
  %PrepareFunctionForOptimization(js_fn_instr);
  for (let i = 0; i < 10; ++i) {
    try {
      js_fn_instr(s, Symbol());
    } catch (e) {}
  }
  %OptimizeFunctionOnNextCall(js_fn_instr);
}
`;

async function evalValue(expression) {
  const {result} = await Protocol.Runtime.evaluate({expression});
  if (result.exceptionDetails) {
    InspectorTest.logMessage(result.exceptionDetails);
  }
  return result.result.value;
}

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  const pausedEvents = [];
  Protocol.Debugger.onPaused(async msg => {
    const topFrame = msg.params.callFrames[0];
    pausedEvents.push({
      reason: msg.params.reason,
      functionName: topFrame.functionName || '(wasm)',
      url: session.getCallFrameUrl(topFrame),
      columnNumber: topFrame.location.columnNumber,
    });
    await Protocol.Debugger.resume();
  });

  // Load main script and wait for initial wasm:// scriptParsed for module1.
  let resolveWasmScript1;
  const wasmScript1Promise = new Promise(resolve => {
    resolveWasmScript1 = resolve;
  });
  Protocol.Debugger.onScriptParsed(msg => {
    if (msg.params.url.startsWith('wasm://')) {
      resolveWasmScript1(msg.params.scriptId);
    }
  });
  contextGroup.addScript(source, 0, 0, 'test.js');
  const wasmScriptId1 = await wasmScript1Promise;

  // --- Order A: Optimize JS caller first, then set Wasm breakpoint ---
  InspectorTest.log('--- Order A ---');
  InspectorTest.log(
      `optimize(js_fn1, s) = ${await evalValue('optimize(js_fn1, s)')}`);
  InspectorTest.log(`js_fn1 optimized before breakpoint: ${
      await evalValue('isOptimized(js_fn1)')}`);

  pausedEvents.length = 0;
  const bp1 = await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId: wasmScriptId1,
      lineNumber: 0,
      columnNumber: getFieldFunc.body_offset,
    },
  });
  InspectorTest.log(`js_fn1 optimized after breakpoint: ${
      await evalValue('isOptimized(js_fn1)')}`);
  InspectorTest.log(`js_fn1(s) = ${await evalValue('js_fn1(s)')}`);
  InspectorTest.log(`Order A pauses: ${pausedEvents.length}`);
  if (pausedEvents.length > 0) {
    InspectorTest.log(`Paused in: ${pausedEvents[0].functionName} (${
        pausedEvents[0].reason})`);
  }

  // --- Order B: Set Wasm breakpoint first, then optimize JS caller ---
  InspectorTest.log('--- Order B ---');
  pausedEvents.length = 0;
  InspectorTest.log(
      `optimize(js_fn2, s) = ${await evalValue('optimize(js_fn2, s)')}`);
  InspectorTest.log(`js_fn2 optimized with breakpoint: ${
      await evalValue('isOptimized(js_fn2)')}`);
  InspectorTest.log(`Order B warmup+opt pauses: ${pausedEvents.length}`);
  pausedEvents.length = 0;
  InspectorTest.log(`js_fn2(s) = ${await evalValue('js_fn2(s)')}`);
  InspectorTest.log(`Order B call pauses: ${pausedEvents.length}`);
  if (pausedEvents.length > 0) {
    InspectorTest.log(`Paused in: ${pausedEvents[0].functionName} (${
        pausedEvents[0].reason})`);
  }

  // Remove breakpoint and verify newly optimized js_fn3 can inline again.
  InspectorTest.log('--- After removeBreakpoint ---');
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp1.result.breakpointId});
  pausedEvents.length = 0;
  InspectorTest.log(
      `optimize(js_fn3, s) = ${await evalValue('optimize(js_fn3, s)')}`);
  InspectorTest.log(`js_fn3 optimized after removeBreakpoint: ${
      await evalValue('isOptimized(js_fn3)')}`);
  InspectorTest.log(`Pauses after removeBreakpoint: ${pausedEvents.length}`);

  // Verify js_fn3 actually inlined getField1: re-setting breakpoint
  // deoptimizes js_fn3 and calling js_fn3(s) hits the breakpoint.
  const bp2 = await Protocol.Debugger.setBreakpoint({
    location: {
      scriptId: wasmScriptId1,
      lineNumber: 0,
      columnNumber: getFieldFunc.body_offset,
    },
  });
  InspectorTest.log(`js_fn3 optimized after re-adding breakpoint: ${
      await evalValue('isOptimized(js_fn3)')}`);
  pausedEvents.length = 0;
  InspectorTest.log(`js_fn3(s) = ${await evalValue('js_fn3(s)')}`);
  InspectorTest.log(
      `Pauses after re-adding breakpoint: ${pausedEvents.length}`);
  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: bp2.result.breakpointId});

  // --- Order C: Instrumentation breakpoint (beforeScriptExecution) ---
  InspectorTest.log('--- Instrumentation breakpoint ---');
  InspectorTest.log(
      `optimizeInstrPre() = ${await evalValue('optimizeInstrPre()')}`);
  InspectorTest.log(`js_fn_instr_pre optimized before instrumentation: ${
      await evalValue('isOptimized(js_fn_instr_pre)')}`);
  const instrBp = await Protocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});
  pausedEvents.length = 0;
  await evalValue('instantiateModule2()');
  // Calling instantiateModule2() triggers a JS instrumentation pause on eval,
  // clear that before testing the Wasm entry instrumentation pause.
  pausedEvents.length = 0;
  InspectorTest.log(`js_fn_instr_pre optimized after instrumentation: ${
      await evalValue('isOptimized(js_fn_instr_pre)')}`);

  await evalValue('warmUpInstrWithSymbol()');
  InspectorTest.log(
      `js_fn_instr(s, 0) = ${await evalValue('js_fn_instr(s, 0)')}`);
  const wasmInstrPauses = pausedEvents.filter(e => e.url.startsWith('wasm://'));
  InspectorTest.log(`Wasm instrumentation pauses: ${wasmInstrPauses.length}`);
  if (wasmInstrPauses.length > 0) {
    InspectorTest.log(`Paused in: ${wasmInstrPauses[0].functionName} (${
        wasmInstrPauses[0].reason})`);
  }

  // Subsequent call should not trigger instrumentation pause again.
  pausedEvents.length = 0;
  InspectorTest.log(
      `js_fn_instr(s, 0) second = ${await evalValue('js_fn_instr(s, 0)')}`);
  const wasmSecondPauses =
      pausedEvents.filter(e => e.url.startsWith('wasm://'));
  InspectorTest.log(`Second call Wasm pauses: ${wasmSecondPauses.length}`);

  await Protocol.Debugger.removeBreakpoint(
      {breakpointId: instrBp.result.breakpointId});

  InspectorTest.completeTest();
})();
