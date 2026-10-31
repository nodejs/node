// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that a side-effect-free evaluation on a call frame paused inside ' +
    'a with statement over a deferred module namespace fails without ' +
    'starting the module evaluation.');

contextGroup.addModuleWithoutEvaluating(
    `
globalThis.deferredEvaluated = true;
export const foo = 1;
`,
    'deferred');

contextGroup.addModule(
    `
import defer * as ns from 'deferred';
globalThis.ns = ns;
`,
    'main');

function logEvaluateResult(response) {
  const {result, exceptionDetails} = response.result;
  if (exceptionDetails) {
    InspectorTest.log('Threw: ' + exceptionDetails.exception.description);
  } else {
    InspectorTest.log('Result: ' + result.value);
  }
}

async function logDeferredEvaluated() {
  const {result: {result}} = await Protocol.Runtime.evaluate(
      {expression: 'String(globalThis.deferredEvaluated)'});
  InspectorTest.log('globalThis.deferredEvaluated: ' + result.value);
}

InspectorTest.runAsyncTestSuite([
  async function testEvaluateOnCallFrameInWithScope() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();
    await InspectorTest.waitForPendingTasks();

    Protocol.Runtime.evaluate({
      expression: 'function runWith(o) { with (o) { debugger; } }\n' +
          'runWith(globalThis.ns);'
    });
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    InspectorTest.log('Side-effect-free Debugger.evaluateOnCallFrame of foo:');
    logEvaluateResult(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'foo', throwOnSideEffect: true}));
    await logDeferredEvaluated();

    InspectorTest.log('\nRegular Debugger.evaluateOnCallFrame of foo:');
    logEvaluateResult(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'foo'}));
    await logDeferredEvaluated();

    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable();
  },
]);
