// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} =
  InspectorTest.start('Checks that restarting an inlined frame works.');

session.setupScriptMap();

contextGroup.addScript(`
function f(a, b) {
  console.log('f');
  return a + b;  // Breakpoint is set here.
}

function g(a, b) {  // We want to restart 'g'.
  console.log('g');
  return 2 + f(a, b);
}

function h() {
  console.log('h');
  return 1 + g(3, 2);
}

function isOptimized(f) {
  return (%GetOptimizationStatus(f) & 16) ? "optimized" : "unoptimized";
}

%NeverOptimizeFunction(f); // Inline 'g' into 'h', but never 'f'.

%PrepareFunctionForOptimization(h);
for (let i = 0; i < 10; ++i) h();
%OptimizeFunctionOnNextCall(h);
h();
`, 0, 0, 'test.js');

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  const consoleMessages = [];
  Protocol.Runtime.onConsoleAPICalled(({ params }) => {
    consoleMessages.push(params.args[0].value);
  });

  let { result: { result: { value } } } = await Protocol.Runtime.evaluate({ expression: 'isOptimized(h)' });
  InspectorTest.log(`Optimization status for function "h"? ${value}`);

  await Protocol.Debugger.setBreakpointByUrl({ url: 'test.js', lineNumber: 4 });

  const { callFrames, evaluatePromise } = await InspectorTest.evaluateAndWaitForPause('h()');

  ({ result: { result: { value } } } = await Protocol.Runtime.evaluate({ expression: 'isOptimized(h)' }));
  InspectorTest.log(`Optimization status for function "h" after we paused? ${value}`);

  await InspectorTest.restartFrameAndWaitForPause(callFrames, 1);

  // Restarting the frame means we hit the breakpoint a second time.
  Protocol.Debugger.resume();
  await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.resume();

  await evaluatePromise;

  InspectorTest.log(`Called functions: ${consoleMessages.join()}`);

  InspectorTest.completeTest();
})();
