// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks if we can set a breakpoint on a one-line inline functions.');

session.setupScriptMap();

const testClassInitializer = `
function foo() {}
var bar = "bar";

class X {
  constructor() {
    this.x = 1;
  }
  [bar] = 2;
  baz = foo();
}
new X();
//# sourceURL=testInitializer.js`

Protocol.Debugger.enable().then(onDebuggerEnabled);

function onDebuggerEnabled() {
  Protocol.Runtime.enable();
  Protocol.Runtime.onExecutionContextCreated(onExecutionContextCreated);
}

async function onExecutionContextCreated(messageObject) {
  const executionContextId = messageObject.params.context.id;
  await runTest(executionContextId, testClassInitializer, 'testInitializer.js');
  InspectorTest.completeTest();
}

async function runTest(executionContextId, func, url) {
  const obj = await Protocol.Runtime.compileScript({
    expression: func,
    sourceURL: url,
    persistScript: true,
    executionContextId: executionContextId
  });
  const scriptId = obj.result.scriptId;

  InspectorTest.log('Setting breakpoint on `class X`');
  await setBreakpoint(4, 'testInitializer.js');

  InspectorTest.log(
      'Setting breakpoint on constructor, should resolve to same location');
  await setBreakpoint(5, 'testInitializer.js');

  InspectorTest.log('Setting breakpoint on computed properties in class');
  await setBreakpoint(8, 'testInitializer.js');

  InspectorTest.log('Setting breakpoint on initializer function');
  await setBreakpoint(9, 'testInitializer.js');

  Protocol.Runtime.runScript({scriptId});
  const numBreaks = 3;
  for (var i = 0; i < numBreaks; ++i) {
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    InspectorTest.log('Paused on location:');
    session.logCallFrames(callFrames);
    Protocol.Debugger.resume();
  }

  InspectorTest.completeTest();
};

async function setBreakpoint(lineNumber, url) {
  const {result: {locations}} =
      await Protocol.Debugger.setBreakpointByUrl({lineNumber, url});
  await session.logBreakLocations(locations);
}
