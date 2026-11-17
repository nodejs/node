// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that evaluating on a call frame paused inside a with statement, ' +
    'whose subject is a proxy with a throwing trap, surfaces the trap error ' +
    'instead of leaving an exception pending while Context::Lookup keeps ' +
    'walking the context chain.');

function logEvaluateResult(response) {
  const {result, exceptionDetails} = response.result;
  if (exceptionDetails) {
    InspectorTest.log('Threw: ' + exceptionDetails.exception.description);
  } else {
    InspectorTest.log('Result: ' + result.value);
  }
}

InspectorTest.runAsyncTestSuite([
  async function testEvaluateOnCallFrameInWithScopeThrowingProxy() {
    await Protocol.Runtime.enable();
    await Protocol.Debugger.enable();

    // The debug-evaluate context looks up the wrapped with-context with
    // DONT_FOLLOW_CHAINS, which queries own-property attributes rather than
    // going through the `has` trap. So getOwnPropertyDescriptor is the trap
    // reached here.
    await Protocol.Runtime.evaluate({
      expression: `globalThis.p = new Proxy({}, {
        getOwnPropertyDescriptor(t, k) { throw new Error('error from the trap'); }
      });`
    });

    Protocol.Runtime.evaluate({
      expression: 'function runWith(o) { with (o) { debugger; } }\n' +
          'runWith(globalThis.p);'
    });
    const {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    InspectorTest.log('Side-effect-free Debugger.evaluateOnCallFrame of foo:');
    logEvaluateResult(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'foo', throwOnSideEffect: true}));

    InspectorTest.log('\nRegular Debugger.evaluateOnCallFrame of foo:');
    logEvaluateResult(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'foo'}));

    await Protocol.Debugger.resume();
    await Protocol.Debugger.disable();
  },
]);
