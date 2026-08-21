// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

utils.load('test/inspector/wasm-inspector-test.js');

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Test if breakpoints are hit that are set on instrumentation pause in wasm.');
session.setupScriptMap();

function setBreakpoint(msg, condition) {
  const top_frame = msg.params.callFrames[0];
  const reason = msg.params.reason;
  const url = session.getCallFrameUrl(top_frame);
  if (reason === 'instrumentation' && url.startsWith('wasm://')) {
    const scriptId = top_frame.location.scriptId;
    const columnNumber = top_frame.location.columnNumber;

    InspectorTest.log('Setting breakpoint at instrumentation break location');
    const breakpoint_info = {
      'location': {scriptId, 'lineNumber': 0, columnNumber}
    };
    if (condition) {
      breakpoint_info.condition = condition;
    }
    return Protocol.Debugger.setBreakpoint(breakpoint_info);
  }
  return Promise.resolve();
}

async function handlePause(msg) {
  const top_frame = msg.params.callFrames[0];
  const reason = msg.params.reason;
  const url = session.getCallFrameUrl(top_frame);
  InspectorTest.log(`Paused at ${url} with reason "${reason}".`);
  if (!url.startsWith('v8://test/')) {
    await session.logSourceLocation(top_frame.location);
  }
  InspectorTest.log(
      `Hit breakpoints: ${JSON.stringify(msg.params.hitBreakpoints)}`)
  return Protocol.Debugger.resume();
};

// Helper function to run tests to check if we can successfully set and evaluate
// breakpoints on an instrumentation pause.
async function runSetBreakpointOnInstrumentationTest(condition) {
  const builder = new WasmModuleBuilder();
  const start_fn = builder.addFunction('start', kSig_v_v).addBody([kExprNop]);
  builder.addStart(start_fn.index);

  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  InspectorTest.log('Setting instrumentation breakpoint');
  await Protocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});
  InspectorTest.log('Compiling wasm module.');
  WasmInspectorTest.compile(builder.toArray());

  // First pause: compile script.
  await handlePause(await Protocol.Debugger.oncePaused());

  InspectorTest.log('Instantiating module.');
  const evalPromise = WasmInspectorTest.evalWithUrl(
      'new WebAssembly.Instance(module)', 'instantiate');

  // Second pause: instantiate script.
  await handlePause(await Protocol.Debugger.oncePaused());

  // Third pause: wasm script. This will set a breakpoint. Pass on a condition.
  const msg = await Protocol.Debugger.oncePaused();
  await setBreakpoint(msg, condition);
  await handlePause(msg);

  // Fourth pause: wasm script, if condition evaluates to true.
  if (!condition || eval(condition)) {
    await handlePause(await Protocol.Debugger.oncePaused());
  }

  InspectorTest.log('Done.');
  await evalPromise;
  await Protocol.Debugger.disable();
  await Protocol.Runtime.disable();
}

InspectorTest.runAsyncTestSuite([
  // Test if we can set a breakpoint on the first breakable location (which is
  // the same location as where the instrumentation breakpoint hits) and
  // successfully hit the breakpoint.
  async function testSetBreakpointOnInstrumentationPause() {
    await runSetBreakpointOnInstrumentationTest();
  },

  // Test if we can set a conditional breakpoint on the first breakable location
  // and successfully hit the breakpoint.
  async function
      testSetConditionalBreakpointTrueConditionOnInstrumentationPause() {
        await runSetBreakpointOnInstrumentationTest('3 < 5');
      },

  // Test if we can set a conditional breakpoint on the first breakable location
  // which evaluates to false, and therefore does not trigger a pause.
  async function
      testSetConditionalBreakpointFalseConditionOnInstrumentationPause() {
        await runSetBreakpointOnInstrumentationTest('3 > 5');
      },
]);
