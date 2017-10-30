// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kozyatinskiy): on StepOut and probably StepNext at return position
// of async generator we should break at next instruction of resumed generator
// instead of next scheduled microtask.

let {session, contextGroup, Protocol} = InspectorTest.start('StepOut from return position of async function.');

contextGroup.addScript(`
  async function testFunction() {
    async function foo() {
      var p = Promise.resolve();
      await p;
      p.then(() => 1);
      debugger;
      return p;
    }
    await foo();
  }
`);

session.setupScriptMap();
Protocol.Debugger.enable();
InspectorTest.runAsyncTestSuite([
  async function testStepInto() {
    Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.resume();
  },

  async function testStepOver() {
    Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.resume();
  },

  async function testStepOut() {
    Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.resume();
  },
]);

function logPauseLocation(message) {
  return session.logSourceLocation(message.params.callFrames[0].location);
}
