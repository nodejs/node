// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Break locations around function calls');

contextGroup.addScript(`
function foo1() {}
function foo2() {}
function foo3() {}
//# sourceURL=test.js`);

InspectorTest.logProtocolCommandCalls('Debugger.stepInto');
session.setupScriptMap();
InspectorTest.runAsyncTestSuite([
  async function testFunctionCallAsArgument() {
    await testExpression('foo2(foo1())');
  },

  async function testFunctionCallAsArgument() {
    await testExpression('foo2(foo1());');
  },

  async function testFunctionCallAsArguments() {
    await testExpression('foo3(foo1(), foo2());');
  },

  async function testFunctionCallInBinaryExpression() {
    await testExpression('foo3(foo1() + foo2());');
  },
]);

async function logPauseLocation() {
  let {params:{callFrames}} = await Protocol.Debugger.oncePaused();
  session.logSourceLocation(callFrames[0].location);
}

async function testExpression(expression) {
  await Protocol.Debugger.enable();
  let wrapper = `function test() {
${expression}
}
//# sourceURL=test-function.js`;
  Protocol.Runtime.evaluate({expression: wrapper});
  let {params:{scriptId}} = await Protocol.Debugger.onceScriptParsed();
  let {result:{locations}} = await Protocol.Debugger.getPossibleBreakpoints({
    start: {lineNumber: 0, columnNumber : 0, scriptId}});
  locations = locations.filter(location => location.lineNumber === 1);
  InspectorTest.log('Break locations in expression:');
  await session.logBreakLocations(locations);

  for (let location of locations) {
    InspectorTest.log('Breakpoint at:');
    await session.logSourceLocation(location);
    let {result:{breakpointId}} = await Protocol.Debugger.setBreakpoint({
      location});
    let evaluate = Protocol.Runtime.evaluate({
      expression: 'test();\n//# sourceURL=expr.js'});
    InspectorTest.log('Break at:');
    await logPauseLocation();
    Protocol.Debugger.stepInto();
    await logPauseLocation();
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    Protocol.Debugger.resume();
    await evaluate;
  }

  InspectorTest.log('Breakpoint at expression line.')
  let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 1, url: 'test-function.js'});
  let evaluate = Protocol.Runtime.evaluate({
    expression: 'test();\n//# sourceURL=expr.js'});
  await logPauseLocation();
  Protocol.Debugger.stepInto();
  await logPauseLocation();
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  Protocol.Debugger.resume();
  await evaluate;

  await Protocol.Debugger.disable();
}
