// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Regression test for crbug.com/583832157 case 3 ' +
    '(Wasm stepping across throws/traps).');

const builder = new WasmModuleBuilder();
const import_js_throw_catch =
    builder.addImport('env', 'jsThrowCatch', kSig_v_v);

const trap_func =
    builder.addFunction('trap', kSig_v_v).addBody([kExprUnreachable]);

const wasm_fn_a = builder.addFunction('wasmFnA', kSig_v_v)
                      .addBody([
                        kExprNop,  // Breakpoint A
                        kExprCallFunction,
                        trap_func.index,
                      ])
                      .exportFunc();

const callee_b = builder.addFunction('calleeB', kSig_v_v).addBody([
  kExprNop,  // Breakpoint B
  kExprUnreachable,
]);

const caller_b = builder.addFunction('callerB', kSig_v_v)
                     .addBody([
                       kExprCallFunction,
                       callee_b.index,
                     ])
                     .exportFunc();

const wasm_fn_c = builder.addFunction('wasmFnC', kSig_v_v)
                      .addBody([
                        kExprNop,  // Breakpoint C
                        kExprCallFunction,
                        import_js_throw_catch,
                        kExprNop,
                      ])
                      .exportFunc();

const wasm_bytes = JSON.stringify(builder.toArray());

const source = `
const wasm_bytes = new Uint8Array(${wasm_bytes});
let instance;

function jsThrowCatch() {
  try {
    throw new Error('internal');
  } catch (e) {
    let caughtInternally = true;
  }
}

function initWasm() {
  const module = new WebAssembly.Module(wasm_bytes);
  instance = new WebAssembly.Instance(module, {env: {jsThrowCatch}});
}

function jsCallerA() {
  try {
    instance.exports.wasmFnA();
  } catch (e) {
    let inCatchA = true;
    return inCatchA;
  }
}

function jsCallerB() {
  try {
    instance.exports.callerB();
  } catch (e) {
    let inCatchB = true;
    return inCatchB;
  }
}

function jsCallerC() {
  instance.exports.wasmFnC();
  let afterWasmC = true;
  return afterWasmC;
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

let wasmScriptId;

InspectorTest.runAsyncTestSuite([
  async function testSubcaseA() {
    await Protocol.Debugger.enable();
    const instantiatePromise =
        Protocol.Runtime.evaluate({expression: 'initWasm()'});
    const [, {params: wasm_script}] =
        await Protocol.Debugger.onceScriptParsed(2);
    await instantiatePromise;
    wasmScriptId = wasm_script.scriptId;

    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId: wasmScriptId,
        lineNumber: 0,
        columnNumber: wasm_fn_a.body_offset,
      },
    });

    InspectorTest.log('Subcase A: stepOver across Wasm trap:');
    const evalPromise = Protocol.Runtime.evaluate({expression: 'jsCallerA()'});
    const pauseMsg1 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(
        `Paused in: ${pauseMsg1.params.callFrames[0].functionName || 'wasm'}`);

    Protocol.Debugger.stepOver();
    const pauseMsg2 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after stepOver in: ${
        pauseMsg2.params.callFrames[0].functionName}`);

    Protocol.Debugger.stepOver();
    const pauseMsg3 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after second stepOver in: ${
        pauseMsg3.params.callFrames[0].functionName}`);

    await Protocol.Debugger.resume();
    await evalPromise;
  },

  async function testSubcaseB() {
    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId: wasmScriptId,
        lineNumber: 0,
        columnNumber: callee_b.body_offset,
      },
    });

    InspectorTest.log('Subcase B: stepOut from Wasm callee that traps:');
    const evalPromise = Protocol.Runtime.evaluate({expression: 'jsCallerB()'});
    const pauseMsg1 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(
        `Paused in: ${pauseMsg1.params.callFrames[0].functionName || 'wasm'}`);

    Protocol.Debugger.stepOut();
    const pauseMsg2 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after stepOut in: ${
        pauseMsg2.params.callFrames[0].functionName}`);
    await Protocol.Debugger.resume();
    await evalPromise;
  },

  async function testSubcaseC() {
    await Protocol.Debugger.setBreakpoint({
      location: {
        scriptId: wasmScriptId,
        lineNumber: 0,
        columnNumber: wasm_fn_c.body_offset,
      },
    });

    InspectorTest.log(
        'Subcase C: stepOut from Wasm to JS with internal catch:');
    const evalPromise = Protocol.Runtime.evaluate({expression: 'jsCallerC()'});
    const pauseMsg1 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(
        `Paused in: ${pauseMsg1.params.callFrames[0].functionName || 'wasm'}`);

    Protocol.Debugger.stepOut();
    const pauseMsg2 = await Protocol.Debugger.oncePaused();
    InspectorTest.log(`Paused after stepOut in: ${
        pauseMsg2.params.callFrames[0].functionName}`);
    await Protocol.Debugger.resume();
    await evalPromise;
  },
]);
