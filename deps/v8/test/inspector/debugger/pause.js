// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks Debugger.pause');

InspectorTest.setupScriptMap();
Protocol.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testPause() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testSkipFrameworks() {
    Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'var a = 42; //# sourceURL=framework.js'});
    Protocol.Runtime.evaluate({expression: 'var a = 239;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testSkipOtherContext1() {
    let contextGroupId = utils.createContextGroup();
    Protocol.Debugger.enable({}, contextGroupId);
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'var a = 42; //# sourceURL=framework.js'});
    Protocol.Runtime.evaluate({expression: 'var a = 239;'}, contextGroupId);
    Protocol.Runtime.evaluate({expression: 'var a = 1;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable({}, contextGroupId);
  },

  async function testSkipOtherContext2() {
    let contextGroupId = utils.createContextGroup();
    Protocol.Debugger.enable({}, contextGroupId);
    Protocol.Debugger.pause({}, contextGroupId);
    Protocol.Runtime.evaluate({expression: 'var a = 42; //# sourceURL=framework.js'});
    Protocol.Runtime.evaluate({expression: 'var a = 239;'}, contextGroupId);
    Protocol.Runtime.evaluate({expression: 'var a = 1;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable({}, contextGroupId);
  },

  async function testWithNativeBreakpoint() {
    utils.schedulePauseOnNextStatement('', '');
    await Protocol.Debugger.pause();
    utils.cancelPauseOnNextStatement();
    Protocol.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();

    await Protocol.Debugger.pause();
    utils.schedulePauseOnNextStatement('', '');
    utils.cancelPauseOnNextStatement();
    Protocol.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();

    utils.schedulePauseOnNextStatement('', '');
    utils.cancelPauseOnNextStatement();
    await Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'var a = 42;'});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testDisableBreaksShouldCancelPause() {
    await Protocol.Debugger.pause();
    await Protocol.Debugger.setBreakpointsActive({active: false});
    Protocol.Runtime.evaluate({expression: 'var a = 42;'})
      .then(() => Protocol.Debugger.setBreakpointsActive({active: true}))
      .then(() => Protocol.Runtime.evaluate({expression: 'debugger'}));
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  }
]);

async function waitPauseAndDumpLocation() {
  var message = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  await InspectorTest.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
