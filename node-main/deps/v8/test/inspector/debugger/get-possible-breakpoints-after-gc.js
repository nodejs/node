// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks if we keep alive breakpoint information for top-level functions when calling getPossibleBreakpoints.');

session.setupScriptMap();
var executionContextId;

const callGarbageCollector = `
  %CollectGarbage("");
  %CollectGarbage("");
  %CollectGarbage("");
  %CollectGarbage("");
`;

const moduleFunction =
    `function testFunc() { console.log('This is a module function') }`;
let mixedFunctions = ` function A() {}
  console.log('This is a top level function');
`;

Protocol.Debugger.enable().then(onDebuggerEnabled);

function onDebuggerEnabled() {
  Protocol.Runtime.enable();
  Protocol.Runtime.onExecutionContextCreated(onExecutionContextCreated);
}

async function onExecutionContextCreated(messageObject) {
  executionContextId = messageObject.params.context.id;
  await testGetPossibleBreakpoints(
      executionContextId, moduleFunction, 'moduleFunc.js');
  await testGetPossibleBreakpoints(
      executionContextId, mixedFunctions, 'mixedFunctions.js');
  InspectorTest.completeTest();
}

async function testGetPossibleBreakpoints(executionContextId, func, url) {
  const obj = await Protocol.Runtime.compileScript({
    expression: func,
    sourceURL: url,
    persistScript: true,
    executionContextId: executionContextId
  });
  const scriptId = obj.result.scriptId;
  const location = {start: {lineNumber: 0, columnNumber: 0, scriptId}};
  await Protocol.Runtime.runScript({scriptId});
  await Protocol.Runtime.evaluate({expression: `${callGarbageCollector}`});
  const {result: {locations}} =
      await Protocol.Debugger.getPossibleBreakpoints(location);
  InspectorTest.log(`Result of get possible breakpoints in ${url}`);
  InspectorTest.log(JSON.stringify(locations));
}
