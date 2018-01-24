// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Checks that we can update return value on pause');

InspectorTest.runAsyncTestSuite([
  async function testError() {
    Protocol.Debugger.enable();
    let evaluation = Protocol.Runtime.evaluate({
      expression: 'function foo() { debugger; } foo()',
      returnByValue: true
    });
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Set return value not at return position');
    let result = await Protocol.Debugger.setReturnValue({
      newValue: { value: 42 },
    });
    InspectorTest.logMessage(result);
    await Protocol.Debugger.disable();
  },

  async function testUndefined() {
    Protocol.Debugger.enable();
    let evaluation = Protocol.Runtime.evaluate({
      expression: 'function foo() { debugger; } foo()',
      returnByValue: true
    });
    InspectorTest.log('Break at return position..');
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepInto();
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Update return value to 42..');
    Protocol.Debugger.setReturnValue({
      newValue: { value: 42 },
    });
    Protocol.Debugger.resume();
    let {result} = await evaluation;
    InspectorTest.log('Dump actual return value');
    InspectorTest.logMessage(result);
    await Protocol.Debugger.disable();
  },

  async function testArrow() {
    Protocol.Debugger.enable();
    Protocol.Debugger.pause();
    let evaluation = Protocol.Runtime.evaluate({
      expression: '(() => 42)()',
      returnByValue: true
    });
    InspectorTest.log('Break at return position..');
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepInto();
    await Protocol.Debugger.oncePaused();
    Protocol.Debugger.stepInto();
    let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Update return value to 239..');
    Protocol.Debugger.setReturnValue({
      newValue: { value: 239 },
    });
    Protocol.Debugger.resume();
    let {result} = await evaluation;
    InspectorTest.log('Dump actual return value');
    InspectorTest.logMessage(result);
    await Protocol.Debugger.disable();
  }
]);
