// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Test if pauses break execution when requested during instrumentation pause in js');

session.setupScriptMap();

function logPause(msg) {
  const top_frame = msg.params.callFrames[0];
  const reason = msg.params.reason;
  const url = session.getCallFrameUrl(top_frame);
  InspectorTest.log(`Paused at ${url} with reason "${reason}".`);
};

// Test that requests to pause during instrumentation pause cause the debugger
// to pause after it is done with the instrumentation pause.
async function testPauseDuringInstrumentationPause() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  await Protocol.Debugger.setInstrumentationBreakpoint(
      {instrumentation: 'beforeScriptExecution'});
  const runPromise = Protocol.Runtime.evaluate(
      {expression: 'console.log("Hi");\n//# sourceURL=foo.js'});
  InspectorTest.log(
      'Set instrumentation breakpoints and requested script evaluation.');

  // First pause: instrumentation
  {
    const pause = await Protocol.Debugger.oncePaused();
    logPause(pause);
    // Request debugger pause from within the instrumentation pause.
    await Protocol.Debugger.pause({});
    InspectorTest.log('Requested debugger pause.');
    await Protocol.Debugger.resume();
    InspectorTest.log('Resumed.');
  }

  // Second pause: explicit pause request
  {
    const pause = await Protocol.Debugger.oncePaused();
    logPause(pause);
    await Protocol.Debugger.resume();
    InspectorTest.log('Resumed.');
  }

  await runPromise;

  InspectorTest.log('Done.');
  await Protocol.Runtime.disable();
  await Protocol.Debugger.disable();
}

async function testInstrumentationRemoveDuringInstrumentationPause() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();

  const {result: {breakpointId}} =
      await Protocol.Debugger.setInstrumentationBreakpoint(
          {instrumentation: 'beforeScriptExecution'});
  const pause = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'console.log(\'Hi\')'});
  logPause(await pause);
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  InspectorTest.log('Removed instrumentation breakpoint');
  await Protocol.Debugger.resume();
  InspectorTest.log('Resumed');

  const {result: {result: {value}}} =
      await Protocol.Runtime.evaluate({expression: '42'});
  InspectorTest.log(`Evaluation result: ${value}`);
}

InspectorTest.runAsyncTestSuite([
  testPauseDuringInstrumentationPause,
  testInstrumentationRemoveDuringInstrumentationPause
]);
