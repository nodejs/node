// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Test if breakpoints are hit that are set on instrumentation pause in js');

session.setupScriptMap();
function setBreakpoint(msg, condition) {
  const reason = msg.params.reason;
  if (reason === 'instrumentation') {
    const top_frame = msg.params.callFrames[0];
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

function handlePause(msg) {
  const top_frame = msg.params.callFrames[0];
  const reason = msg.params.reason;
  const url = session.getCallFrameUrl(top_frame);
  InspectorTest.log(`Paused at ${url} with reason "${reason}".`);
  InspectorTest.log(
      `Hit breakpoints: ${JSON.stringify(msg.params.hitBreakpoints)}`);
  return Protocol.Debugger.resume();
};

// Helper function to check if we can successfully set and evaluate breakpoints
// on an instrumentation pause.
async function runSetBreakpointOnInstrumentationTest(condition) {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  InspectorTest.log('set breakpoint and evaluate script..');
  await Protocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});
  const runPromise =
      Protocol.Runtime.evaluate({expression: '//# sourceURL=foo.js'});

  // First pause: instrumentation
  const msg = await Protocol.Debugger.oncePaused();
  await setBreakpoint(msg, condition);
  await handlePause(msg);

  // Second pause: if condition evaluates to true
  if (!condition || eval(condition)) {
    await handlePause(await Protocol.Debugger.oncePaused());
  }

  InspectorTest.log('Done.');
  await runPromise;
  await Protocol.Runtime.disable();
  await Protocol.Debugger.disable();
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
        await runSetBreakpointOnInstrumentationTest('4 > 3');
      },

  // Test if we can set a conditional breakpoint on the first breakable location
  // which evaluates to false, and therefore does not trigger a pause.
  async function
      testSetConditionalBreakpointFalseConditionOnInstrumentationPause() {
        await runSetBreakpointOnInstrumentationTest('3 > 4');
      }
]);
