// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks stepping over tail calls.');

session.setupScriptMap();
InspectorTest.logProtocolCommandCalls('Debugger.pause');
InspectorTest.logProtocolCommandCalls('Debugger.stepInto');
InspectorTest.logProtocolCommandCalls('Debugger.stepOver');
InspectorTest.logProtocolCommandCalls('Debugger.stepOut');
InspectorTest.logProtocolCommandCalls('Debugger.resume');

let source = `
function f(x) {
  if (x == 2) debugger;
  if (x-- > 0) return f(x);
}
f(5);
`;

Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
InspectorTest.runAsyncTestSuite([
  async function testStepOver() {
    Protocol.Runtime.evaluate({expression: source});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testStepOut() {
    Protocol.Runtime.evaluate({expression: source});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testStepOutFromReturn() {
    Protocol.Runtime.evaluate({expression: source});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  }
]);

function logPauseLocation(message) {
  session.logCallFrames(message.params.callFrames);
  return session.logSourceLocation(message.params.callFrames[0].location);
}
