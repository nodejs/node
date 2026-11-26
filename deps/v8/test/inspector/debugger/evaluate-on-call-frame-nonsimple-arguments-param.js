// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Debugger.evaluateOnCallFrame with non-simple arguments param');

Protocol.Debugger.enable();

InspectorTest.runAsyncTestSuite([async function testOptimizedAndUnoptimized() {
  contextGroup.addInlineScript(
      `
      let breakNow = false;
      function pause() {
        if (breakNow) debugger;
      }
      function f(arguments = 7) {
        pause();
      }
      %NeverOptimizeFunction(pause);

      function runUnoptimized() {
        breakNow = true;
        f();
        breakNow = false;
      }

      function runOptimized() {
        breakNow = false;
        %PrepareFunctionForOptimization(f);
        f();
        f();
        %OptimizeFunctionOnNextCall(f);
        breakNow = true;
        f();
        breakNow = false;
      }
    `,
      'test.js');

  InspectorTest.log('Testing unoptimized f:');
  Protocol.Runtime.evaluate({expression: 'runUnoptimized()'});
  let {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused in unoptimized frame');
  let evalUnopt = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId: callFrames[1].callFrameId, expression: 'arguments'});
  InspectorTest.logMessage(evalUnopt.result.result);
  await Protocol.Debugger.resume();

  InspectorTest.log('Testing optimized f:');
  Protocol.Runtime.evaluate({expression: 'runOptimized()'});
  ({params: {callFrames}} = await Protocol.Debugger.oncePaused());
  InspectorTest.log('Paused in optimized frame');
  let evalOpt = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId: callFrames[1].callFrameId, expression: 'arguments'});
  if (evalOpt.result.exceptionDetails) {
    InspectorTest.log(
        'Exception: ' +
        evalOpt.result.exceptionDetails.exception.description.split('\n')[0]);
  } else {
    InspectorTest.logMessage(evalOpt.result.result);
  }
  await Protocol.Debugger.resume();
}]);
