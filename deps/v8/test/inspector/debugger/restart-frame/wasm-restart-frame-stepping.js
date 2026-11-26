// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Regression test for crbug.com/583832157 case 4 ' +
    '(restartFrame invariants).');

const builder = new WasmModuleBuilder();
const wasm_fn = builder.addFunction('wasmFn', kSig_v_v)
                    .addBody([
                      kExprNop,  // Breakpoint inside Wasm
                    ])
                    .exportFunc();

const wasm_bytes = JSON.stringify(builder.toArray());

const source = `
const wasm_bytes = new Uint8Array(${wasm_bytes});
let instance;

function helper() {
  return 1;
}

function target() {
  return helper() + helper();
}

function initWasm() {
  const module = new WebAssembly.Module(wasm_bytes);
  instance = new WebAssembly.Instance(module, {});
}

function caller() {
  instance.exports.wasmFn();
  let afterWasm = true;
  return afterWasm;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

InspectorTest.runAsyncTestSuite([
  async function testSubcaseA() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    InspectorTest.log('Subcase A: restartFrame mid-first-statement top frame:');
    // Set breakpoint in helper
    const {result: {breakpointId}} =
        await Protocol.Debugger.setBreakpointByUrl({
          url: 'test.js',
          lineNumber: 4,  // inside helper()
        });

    const evalPromise = Protocol.Runtime.evaluate({expression: 'target()'});
    const pauseMsg1 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(
        `Paused in: ${pauseMsg1.params.callFrames[0].functionName}`);

    // Step out to target() mid-statement
    Protocol.Debugger.stepOut();
    const pauseMsg2 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(
        `Paused in: ${pauseMsg2.params.callFrames[0].functionName}`);

    // Restart target()
    const targetCallFrameId = pauseMsg2.params.callFrames[0].callFrameId;
    InspectorTest.log('Restarting target() frame:');
    Protocol.Debugger.restartFrame({
      callFrameId: targetCallFrameId,
      mode: 'StepInto',
    });

    const pauseMsg3 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after restart in: ${
        pauseMsg3.params.callFrames[0].functionName}`);
    InspectorTest.log(
        `At line: ${pauseMsg3.params.callFrames[0].location.lineNumber}`);

    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Protocol.Debugger.resume();
    await evalPromise;
  },

  async function testSubcaseB() {
    const instantiatePromise =
        Protocol.Runtime.evaluate({expression: 'initWasm()'});
    const [, {params: wasm_script}] =
        await Protocol.Debugger.onceScriptParsed(2);
    await instantiatePromise;
    const scriptId = wasm_script.scriptId;

    const {result: {breakpointId}} = await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId,
        lineNumber: 0,
        columnNumber: wasm_fn.body_offset,
      },
    });

    InspectorTest.log('Subcase B: restartFrame on JS caller from Wasm pause:');
    const evalPromise = Protocol.Runtime.evaluate({expression: 'caller()'});
    const pauseMsg1 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(
        `Paused in: ${pauseMsg1.params.callFrames[0].functionName || 'wasm'}`);

    const callerCallFrameId = pauseMsg1.params.callFrames[1].callFrameId;
    InspectorTest.log('Restarting caller frame:');
    Protocol.Debugger.restartFrame({
      callFrameId: callerCallFrameId,
      mode: 'StepInto',
    });

    const pauseMsg2 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after restart in: ${
        pauseMsg2.params.callFrames[0].functionName}`);

    await Protocol.Debugger.removeBreakpoint({breakpointId});
    InspectorTest.log('Stepping over wasm call in caller:');
    Protocol.Debugger.stepOver();
    const pauseMsg3 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after stepOver in: ${
        pauseMsg3.params.callFrames[0].functionName}`);

    await Protocol.Debugger.resume();
    await evalPromise;
  },
]);
