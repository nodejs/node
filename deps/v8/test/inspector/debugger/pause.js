// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks Debugger.pause');
let contextGroup1 = new InspectorTest.ContextGroup();
let session1 = contextGroup1.connect();
let Protocol1 = session1.Protocol;

session1.setupScriptMap();
Protocol1.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testPause() {
    Protocol1.Debugger.pause();
    Protocol1.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();
  },

  async function testSkipFrameworks() {
    Protocol1.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
    Protocol1.Debugger.pause();
    Protocol1.Runtime.evaluate({expression: 'var a = 42; //# sourceURL=framework.js'});
    Protocol1.Runtime.evaluate({expression: 'var a = 239;'});
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();
  },

  async function testSkipOtherContext1() {
    let contextGroup2 = new InspectorTest.ContextGroup();
    let session2 = contextGroup2.connect();
    let Protocol2 = session2.Protocol;
    Protocol2.Debugger.enable({});
    Protocol1.Debugger.pause();
    Protocol1.Runtime.evaluate({expression: 'var a = 42; //# sourceURL=framework.js'});
    Protocol2.Runtime.evaluate({expression: 'var a = 239;'});
    Protocol1.Runtime.evaluate({expression: 'var a = 1;'});
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();
    await Protocol2.Debugger.disable({});
  },

  async function testSkipOtherContext2() {
    let contextGroup2 = new InspectorTest.ContextGroup();
    let session2 = contextGroup2.connect();
    let Protocol2 = session2.Protocol;
    session2.setupScriptMap();
    Protocol2.Debugger.enable({});
    Protocol2.Debugger.pause({});
    Protocol1.Runtime.evaluate({expression: 'var a = 42; //# sourceURL=framework.js'});
    Protocol2.Runtime.evaluate({expression: 'var a = 239;'});
    Protocol1.Runtime.evaluate({expression: 'var a = 1;'});
    await waitPauseAndDumpLocation(session2);
    // should not resume pause from different context group id.
    Protocol1.Debugger.resume();
    Protocol2.Debugger.stepOver({});
    await waitPauseAndDumpLocation(session2);
    await Protocol2.Debugger.resume({});
    await Protocol2.Debugger.disable({});
  },

  async function testWithNativeBreakpoint() {
    contextGroup1.schedulePauseOnNextStatement('', '');
    await Protocol1.Debugger.pause();
    contextGroup1.cancelPauseOnNextStatement();
    Protocol1.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();

    await Protocol1.Debugger.pause();
    contextGroup1.schedulePauseOnNextStatement('', '');
    contextGroup1.cancelPauseOnNextStatement();
    Protocol1.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();

    contextGroup1.schedulePauseOnNextStatement('', '');
    contextGroup1.cancelPauseOnNextStatement();
    await Protocol1.Debugger.pause();
    Protocol1.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();
  },

  async function testDisableBreaksShouldCancelPause() {
    await Protocol1.Debugger.pause();
    await Protocol1.Debugger.setBreakpointsActive({active: false});
    Protocol1.Runtime.evaluate({expression: 'var a = 42;'})
      .then(() => Protocol1.Debugger.setBreakpointsActive({active: true}))
      .then(() => Protocol1.Runtime.evaluate({expression: 'debugger'}));
    await waitPauseAndDumpLocation(session1);
    await Protocol1.Debugger.resume();
  }
]);

async function waitPauseAndDumpLocation(session) {
  var message = await session.Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  await session.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
