// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Stepping with natives and frameworks.');

contextGroup.addScript(`
function callAll() {
  for (var f of arguments)
    f();
}
//# sourceURL=framework.js`);

session.setupScriptMap();
InspectorTest.logProtocolCommandCalls('Debugger.pause');
InspectorTest.logProtocolCommandCalls('Debugger.stepInto');
InspectorTest.logProtocolCommandCalls('Debugger.stepOver');
InspectorTest.logProtocolCommandCalls('Debugger.stepOut');
InspectorTest.logProtocolCommandCalls('Debugger.resume');

Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
InspectorTest.runAsyncTestSuite([
  async function testNativeCodeStepOut() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: '[1,2].map(v => v);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testNativeCodeStepOver() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: '[1,2].map(v => v);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
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

  async function testNativeCodeStepInto() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: '[1,2].map(v => v);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkCodeStepInto() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'callAll(() => 1, () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkCodeStepOver() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'callAll(() => 1, () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
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

  async function testFrameworkCodeStepOut() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'callAll(() => 1, () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkNextCallDeeperStepOut() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(() => 1, callAll.bind(null, () => 2));'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkNextCallDeeperStepOutSameFunction() {
    await Protocol.Runtime.evaluate({expression: 'foo = () => 1'});
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(foo, callAll.bind(null, foo));'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkNextCallDeeperStepInto() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(() => 1, callAll.bind(null, () => 2));'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
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

  async function testFrameworkNextCallDeeperStepOver() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(() => 1, callAll.bind(null, () => 2));'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
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

  async function testFrameworkCurrentCallDeeperStepOut() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(callAll.bind(null, () => 1), () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkCurrentCallDeeperStepOutSameFunction() {
    await Protocol.Runtime.evaluate({expression: 'foo = () => 1'});
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(callAll.bind(null, foo), foo);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkCurrentCallDeeperStepOver() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(callAll.bind(null, () => 1), () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
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

  async function testFrameworkCurrentCallDeeperStepInto() {
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(callAll.bind(null, () => 1), () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testFrameworkStepOverMixed() {
    await Protocol.Runtime.evaluate({expression: 'foo = () => 1'});
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(foo, foo, () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
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

  async function testFrameworkStepOutMixed() {
    await Protocol.Runtime.evaluate({expression: 'foo = () => 1'});
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(foo, foo, () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testStepOutFrameworkSameFunctionAtReturn() {
    await Protocol.Runtime.evaluate({expression: 'foo = () => 1'});
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({
      expression: 'callAll(foo, foo, () => 2);'});
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepInto();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOver();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    Protocol.Debugger.stepOut();
    await logPauseLocation(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  }
]);

function logPauseLocation(message) {
  return session.logSourceLocation(message.params.callFrames[0].location);
}
