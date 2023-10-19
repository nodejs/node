// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks if we correctly handle exceptions thrown on setBreakpointByUrl if script is invalid.');

session.setupScriptMap();
var executionContextId;

const invalidFunction = `console.lo        g('This is a top level function')`;
const moduleFunction =
    `function testFunc() { console.log('This is a module function') }`;

Protocol.Debugger.enable().then(onDebuggerEnabled);

function onDebuggerEnabled() {
  Protocol.Runtime.enable();
  Protocol.Runtime.onExecutionContextCreated(onExecutionContextCreated);
}

async function onExecutionContextCreated(messageObject) {
  executionContextId = messageObject.params.context.id;
  await testSetBreakpoint(
      executionContextId, invalidFunction, 'invalidFunc.js');
  await testSetBreakpoint(executionContextId, moduleFunction, 'moduleFunc.js');
  InspectorTest.completeTest();
}

async function testSetBreakpoint(executionContextId, func, url) {
  await Protocol.Runtime.compileScript({
    expression: func,
    sourceURL: url,
    persistScript: true,
    executionContextId: executionContextId
  });
  const {result: {locations}} =
      await Protocol.Debugger.setBreakpointByUrl({lineNumber: 0, url});
  InspectorTest.logMessage(locations);
}
