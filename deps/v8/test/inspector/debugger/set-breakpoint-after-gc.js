// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks if we keep alive breakpoint information for top-level functions.');

session.setupScriptMap();
var executionContextId;

const callGarbageCollector = `
  %CollectGarbage("");
  %CollectGarbage("");
  %CollectGarbage("");
  %CollectGarbage("");
`;

const topLevelFunction = `console.log('This is a top level function')`;
const moduleFunction =
    `function testFunc() { console.log('This is a module function') }`;

Protocol.Debugger.enable().then(onDebuggerEnabled);

function onDebuggerEnabled() {
  Protocol.Runtime.enable();
  Protocol.Runtime.onExecutionContextCreated(onExecutionContextCreated);
}

async function onExecutionContextCreated(messageObject) {
  executionContextId = messageObject.params.context.id;
  await testSetBreakpoint(executionContextId, topLevelFunction, 'topLevel.js');
  await testSetBreakpoint(executionContextId, moduleFunction, 'moduleFunc.js');
  InspectorTest.completeTest();
}

async function testSetBreakpoint(executionContextId, func, url) {
  const obj = await Protocol.Runtime.compileScript({
    expression: func,
    sourceURL: url,
    persistScript: true,
    executionContextId: executionContextId
  });
  const scriptId = obj.result.scriptId;
  await Protocol.Runtime.runScript({scriptId});
  // TODO(41480448): This relies on precise  garbage collection and may fail if
  // conservative stack scanning is used. We call callGarbageCollector twice
  // to ensure the weak reference hold by the script is collected.
  await Protocol.Runtime.evaluate({expression: `${callGarbageCollector}`});
  await Protocol.Runtime.evaluate({expression: `${callGarbageCollector}`});
  const {result: {locations}} =
      await Protocol.Debugger.setBreakpointByUrl({lineNumber: 0, url});
  InspectorTest.log(`Result of setting breakpoint in ${url}`);
  InspectorTest.log(JSON.stringify(locations));
}
