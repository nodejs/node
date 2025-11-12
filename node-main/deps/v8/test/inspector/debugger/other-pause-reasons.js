// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start(
    `Test that all 'other' reasons are explicitly encoded on a pause event if they overlap with another reason`);

function handlePause(
    noInstrumentationStepAction, options,
    {params: {reason, data, callFrames}}) {
  const scriptId = callFrames[0].functionLocation.scriptId;
  InspectorTest.log(`Paused with reason ${reason}, data ${
      data ? JSON.stringify(data) : '{}'} and scriptId: ${scriptId}.`);

  if (reason === 'instrumentation') {
    Protocol.Debugger.resume();
  } else {
    Protocol.Debugger[noInstrumentationStepAction](options);
  }
}

const resumeOnPause = handlePause.bind(null, 'resume', null);

async function setUpEnvironment() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
}

async function tearDownEnvironment() {
  await Protocol.Debugger.disable();
  await Protocol.Runtime.disable();
}

InspectorTest.runAsyncTestSuite([
  async function testBreakpointPauseReason() {
    await setUpEnvironment()
    Protocol.Debugger.onPaused(resumeOnPause);
    await Protocol.Debugger .setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptExecution'
    });
    const {result: { scriptId }} = await Protocol.Runtime.compileScript({
    expression: `console.log('foo');`, sourceURL: 'foo.js', persistScript: true });
    await Protocol.Debugger.setBreakpointByUrl({
        lineNumber: 0,
        columnNumber: 0,
        url: 'foo.js',
        });
    await Protocol.Runtime.runScript({ scriptId });
    tearDownEnvironment();
  },
  async function testTriggeredPausePauseReason() {
    await setUpEnvironment();
    Protocol.Debugger.onPaused(resumeOnPause);
    await Protocol.Debugger.setInstrumentationBreakpoint({
      instrumentation: 'beforeScriptExecution'
    });
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({
      expression: `console.log('foo');//# sourceURL=foo.js`});
    tearDownEnvironment();
  },
  async function testSteppingPauseReason() {
    await setUpEnvironment();
    await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: `setTimeout('console.log(3);//# sourceURL=bar.js', 0);`,
      sourceURL: 'foo.js',
      persistScript: true
    });
    await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 0,
      url: 'foo.js',
    });

    const runPromise = Protocol.Runtime.runScript({scriptId});
    // Pausing 5 times:
    // 2x instrumentation breaks,
    // 1x breakpoint,
    // 2x step ins: end of setTimeout function, start of inner script.
    for (var i = 0; i < 5; ++i) {
      const msg = await Protocol.Debugger.oncePaused();
      handlePause('stepInto', null, msg);
    }

    await runPromise;
    await tearDownEnvironment();
  },
  async function testOnlyReportOtherWithEmptyDataOnce() {
    await setUpEnvironment();
    Protocol.Debugger.onPaused(resumeOnPause);
    Protocol.Debugger.pause();
    const {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: 'console.log(foo);',
      sourceURL: 'foo.js',
      persistScript: true
    });
    await Protocol.Debugger.setBreakpointByUrl({
      lineNumber: 0,
      url: 'foo.js',
    });

    await Protocol.Runtime.runScript({scriptId});
    await tearDownEnvironment();
  },
  async function testDebuggerStatementReason() {
    await setUpEnvironment();
    Protocol.Debugger.onPaused(resumeOnPause);
    await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});

    const {result: {scriptId}} = await Protocol.Runtime.compileScript(
        {expression: 'debugger;', sourceURL: 'foo.js', persistScript: true});

    await Protocol.Runtime.runScript({scriptId});
    await tearDownEnvironment();
  },
  async function testAsyncSteppingPauseReason() {
    await setUpEnvironment();
    await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const expression =
        `debugger; setTimeout('console.log(3);//# sourceURL=bar.js', 0);`;
    const {result: {scriptId}} = await Protocol.Runtime.compileScript(
        {expression, sourceURL: 'foo.js', persistScript: true});
    const runPromise = Protocol.Runtime.runScript({scriptId});
    // Pausing 6 times:
    // 2x instrumentation breaks,
    // 1x debugger statement,
    // 3x steps in: start of setTimeout, start of inner script, end of inner script.
    for (var i = 0; i < 6; ++i) {
      const msg = await Protocol.Debugger.oncePaused();
      handlePause('stepInto', {breakOnAsyncCall: true}, msg);
    }
    await runPromise;
    await tearDownEnvironment();
  },
  async function testSteppingOutPauseReason() {
    await setUpEnvironment();
    await Protocol.Debugger.setInstrumentationBreakpoint(
        {instrumentation: 'beforeScriptExecution'});
    const expression = `
    function test() {
      debugger;
      eval('console.log(3);//# sourceURL=bar.js');
    }
    test();
    `
    const {result: {scriptId}} = await Protocol.Runtime.compileScript(
        {expression, sourceURL: 'foo.js', persistScript: true});

    const runPromise = Protocol.Runtime.runScript({scriptId});
    const stepOutOnPause = handlePause.bind(this, 'stepOut', null);
    Protocol.Debugger.onPaused(stepOutOnPause);

    await runPromise;
    await tearDownEnvironment();
  },
]);
