// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

InspectorTest.runAsyncTestSuite([
  async function testStepIntoAtReturnPosition() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    const evalPromise =
        Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Promise.all([
      Protocol.Debugger.resume(),
      evalPromise,
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testStepOverAtReturnPosition() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    const evalPromise =
        Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Promise.all([
      Protocol.Debugger.resume(),
      evalPromise,
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testStepOutAtReturnPosition() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    const evalPromise =
        Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Promise.all([
      Protocol.Debugger.resume(),
      evalPromise,
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testStepOut() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    const evalPromise =
        Protocol.Runtime.evaluate({expression: 'testFunction()'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Promise.all([
      Protocol.Debugger.resume(),
      evalPromise,
      Protocol.Debugger.disable(),
      Protocol.Runtime.disable(),
    ]);
  },
]);

function logPauseLocation(message) {
  return session.logSourceLocation(message.params.callFrames[0].location);
}
