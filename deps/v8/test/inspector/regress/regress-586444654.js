// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-maglev

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/586444654.');

const nonDebuggableFnSource = `(function nonDebuggableFn(o, a, b, doCapture) {
  'use strict';
  const first = o.x;
  let res;
  try {
    res = a + b;
  } finally {
    const second = o.x;
    if (doCapture) {
      globalThis.captured = {
        interpreted: %IsBeingInterpreted(),
        unoptimized: (%GetOptimizationStatus(nonDebuggableFn) & 8) === 0,
        first,
        second,
      };
    }
  }
  return res;
})`;

const scriptSource = `
const o = {x: 2};
o.x = 1;
const longA = 'a'.repeat(%StringMaxLength() - 10);
const longB = 'b'.repeat(20);

function callNonDebuggable(fn, doCapture) {
  try {
    fn(o, longA, longB, doCapture);
  } catch (e) {
  }
}
`;

contextGroup.addScript(scriptSource, 0, 0, 'test.js');

(async function test() {
  const contextCreatedPromise = Protocol.Runtime.onceExecutionContextCreated();
  await Protocol.Runtime.enable();
  const {params: {context: {id: executionContextId}}} =
      await contextCreatedPromise;

  const installRes = await Protocol.Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: 'function(fn) { globalThis.nonDebuggableFn = fn; }',
    arguments: [{unserializableValue: nonDebuggableFnSource}],
  });
  if (installRes.result.exceptionDetails) {
    InspectorTest.logObject(installRes.result.exceptionDetails);
  }

  const warmupExpr = `
    globalThis.captured = null;
    %PrepareFunctionForOptimization(nonDebuggableFn);
    nonDebuggableFn(o, 'a', 'b', true);
    globalThis.captured = null;
    callNonDebuggable(nonDebuggableFn, true);
    globalThis.captured = null;
    nonDebuggableFn(o, 'a', 'b', false);
    callNonDebuggable(nonDebuggableFn, false);
    %OptimizeFunctionOnNextCall(nonDebuggableFn);
    nonDebuggableFn(o, 'a', 'b', false);
    (%GetOptimizationStatus(nonDebuggableFn) & 8) !== 0;
  `;
  const warmupRes = await Protocol.Runtime.evaluate({
    expression: warmupExpr,
    returnByValue: true,
  });
  if (warmupRes.result.exceptionDetails) {
    InspectorTest.logObject(warmupRes.result.exceptionDetails);
  }
  InspectorTest.log(`wasOptimized: ${warmupRes.result.result.value}`);

  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let pauseCount = 0;
  Protocol.Debugger.onPaused(async () => {
    pauseCount++;
    await Protocol.Runtime.evaluate({expression: 'o.x = 999;'});
    await Protocol.Debugger.resume();
  });

  const runExpr = `
    callNonDebuggable(nonDebuggableFn, true);
    globalThis.captured;
  `;
  const runRes = await Protocol.Runtime.evaluate({
    expression: runExpr,
    returnByValue: true,
  });
  if (runRes.result.exceptionDetails) {
    InspectorTest.logObject(runRes.result.exceptionDetails);
  }
  InspectorTest.log(`pauseCount: ${pauseCount}`);
  InspectorTest.logObject(runRes.result.result.value);
  InspectorTest.completeTest();
})();
