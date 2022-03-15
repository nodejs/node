// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start(
    `Test that all 'other' reasons are explicitly encoded on a pause event if they overlap with another reason`);

function resumeOnPause({params: {reason, data}}) {
  InspectorTest.log(`Paused with reason: ${reason} and data: ${
      data ? JSON.stringify(data) : '{}'}.`)
  Protocol.Debugger.resume();
}

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
    const stepOnPause = (({params: {reason, data}}) => {
      InspectorTest.log(`Paused with reason: ${reason} and data: ${
          data ? JSON.stringify(data) : '{}'}.`);
      if (reason === 'instrumentation') {
        Protocol.Debugger.resume();
      } else {
        Protocol.Debugger.stepInto();
      }
    });
    Protocol.Debugger.onPaused(stepOnPause);

    const {result: {scriptId}} = await Protocol.Runtime.compileScript({
      expression: `setTimeout('console.log(3);//# sourceURL=bar.js', 0);`,
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
    const stepOnPause = (({params: {reason, data}}) => {
      InspectorTest.log(`Paused with reason: ${reason} and data: ${
          data ? JSON.stringify(data) : '{}'}.`);
      Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    });
    Protocol.Debugger.onPaused(stepOnPause);
    const expression =
        `debugger; setTimeout('console.log(3);//# sourceURL=bar.js', 0);`;
    const {result: {scriptId}} = await Protocol.Runtime.compileScript(
        {expression, sourceURL: 'foo.js', persistScript: true});
    await Protocol.Runtime.runScript({scriptId});
    await tearDownEnvironment();
  },
]);
