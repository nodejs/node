// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test catching exceptions in promise constructor in conjunction with deoptimization.');

const source = `
function foo(a, b) {
  let P = new Promise((resolve, reject) => {
    bar(a, b);
    resolve();
  });
  return P;
}

function bar(a, b) {
  %DeoptimizeFunction(foo);
  throw new Error("uncaught"); // EXCEPTION
}

%PrepareFunctionForOptimization(foo);

foo();
%PerformMicrotaskCheckpoint();

foo();
%PerformMicrotaskCheckpoint();

%OptimizeFunctionOnNextCall(foo);

// bar likely gets inlined into foo.
foo();
%PerformMicrotaskCheckpoint();

%NeverOptimizeFunction(bar);
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);

// bar does not get inlined into foo.
foo();
%PerformMicrotaskCheckpoint();
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let events = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames, data}}) => {
    events++;
    let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
        {scriptId: callFrames[0].location.scriptId});
    let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
    let isExceptionLine = /\/\/ EXCEPTION/.test(line);
    let message = data.description.split('\n')[0].replace(/^Error:\s*/, '');
    InspectorTest.log(`Paused on exception: uncaught=${data.uncaught}`);
    InspectorTest.log(`Exception message: ${message}`);
    InspectorTest.log(`At exception line: ${isExceptionLine}`);
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log(`Total exception events: ${events}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
