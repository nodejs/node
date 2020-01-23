// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test for Debugger.stepInto with breakOnAsyncCall.');

InspectorTest.runAsyncTestSuite([
  async function testSetTimeout() {
    Protocol.Debugger.enable();
    Protocol.Debugger.pause();
    Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

    let pausedPromise = Protocol.Debugger.oncePaused();
    Protocol.Runtime.evaluate({
      expression: 'setTimeout(() => 42, 0)//# sourceURL=test.js'
    });
    await pausedPromise;
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    session.logCallFrames(callFrames);
    await Protocol.Debugger.disable();
  },

  async function testPromiseThen() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

    Protocol.Runtime.evaluate({expression: 'var p = Promise.resolve()'});
    Protocol.Debugger.pause();
    let pausedPromise = Protocol.Debugger.oncePaused();
    Protocol.Runtime.evaluate({expression: 'p.then(() => 42)//# sourceURL=test.js'});
    await pausedPromise;
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    let {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    session.logCallFrames(callFrames);
    await Protocol.Debugger.disable();
  }
]);
