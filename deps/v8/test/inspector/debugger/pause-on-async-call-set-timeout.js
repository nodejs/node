// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Debugger.pauseOnAsynCall with setTimeout.');
session.setupScriptMap();
Protocol.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testSetTimeout() {
    Protocol.Runtime.evaluate({expression: 'debugger; setTimeout(() => 1, 0);'});
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let parentStackTraceId = await waitPauseAndDumpLocation();
    Protocol.Debugger.pauseOnAsyncCall({parentStackTraceId});
    Protocol.Debugger.resume();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testDebuggerStmtBeforeCallback1() {
    Protocol.Runtime.evaluate({expression: 'debugger; setTimeout(() => 1, 0);debugger;'});
    Protocol.Runtime.evaluate({expression: 'setTimeout(\'debugger//should-break-here\', 0)'});
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let parentStackTraceId = await waitPauseAndDumpLocation();
    Protocol.Debugger.pauseOnAsyncCall({parentStackTraceId});
    await Protocol.Debugger.resume();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testDebuggerStmtBeforeCallback2() {
    Protocol.Runtime.evaluate({expression: 'debugger;\nsetTimeout(\'debugger//should-break-here\', 0);\nsetTimeout(() => 1, 0);'});
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepOver();
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let parentStackTraceId = await waitPauseAndDumpLocation();
    Protocol.Debugger.pauseOnAsyncCall({parentStackTraceId});
    Protocol.Debugger.resume();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
    await InspectorTest.waitForPendingTasks();
  },

  async function testSetTimeoutWithoutJS() {
    Protocol.Runtime.evaluate({expression: 'debugger; setTimeout(\'}\', 0);\nsetTimeout(\'var a = 239;\', 0);\nsetTimeout(\'debugger//should-break-here\', 0);'});
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let parentStackTraceId = await waitPauseAndDumpLocation();
    Protocol.Debugger.pauseOnAsyncCall({parentStackTraceId});
    Protocol.Debugger.resume();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testResume() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'setTimeout(() => 42, 0)'});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let parentStackTraceId = await waitPauseAndDumpLocation();
    Protocol.Debugger.pauseOnAsyncCall({parentStackTraceId});
    Protocol.Debugger.resume();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  }
]);

async function waitPauseAndDumpLocation() {
  var {params: {callFrames, asyncCallStackTraceId}} =
      await Protocol.Debugger.oncePaused();
  if (!asyncCallStackTraceId) {
    InspectorTest.log('paused at:');
    await session.logSourceLocation(callFrames[0].location);
  }
  return asyncCallStackTraceId;
}
