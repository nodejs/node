// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Check that setting a breakpoint in an invalid function is not crashing.');

const invalidFunc = `console.l   og('hi');//# sourceURL=invalid.js`;

Protocol.Debugger.enable().then(onDebuggerEnabled);

function onDebuggerEnabled() {
  Protocol.Runtime.enable();
  Protocol.Runtime.onExecutionContextCreated(onExecutionContextCreated);
}

async function onExecutionContextCreated(messageObject) {
  const executionContextId = messageObject.params.context.id;
  await testSetBreakpoint(executionContextId, invalidFunc, 'invalid.js');
}

async function testSetBreakpoint(executionContextId, func, url) {
  const obj = await Protocol.Runtime.compileScript({
    expression: func,
    sourceURL: url,
    persistScript: true,
    executionContextId
  });
  const scriptId = obj.result.scriptId;
  const {result: {locations}} =
      await Protocol.Debugger.setBreakpointByUrl({lineNumber: 0, url});
  InspectorTest.log(JSON.stringify(locations));
  InspectorTest.completeTest();
};
