// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that side-effect-free evaluation enforces side-effect checks in createExceptionDetails');

const setupScript = `
  function pause() {
    debugger;
  }
`;
contextGroup.addScript(setupScript);

async function evaluateViaRuntime(params) {
  return await Protocol.Runtime.evaluate(params);
}

async function evaluateViaCallFrame(params) {
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'setTimeout(pause, 0)'});
  const {params: {callFrames: [{callFrameId}]}} =
      await Protocol.Debugger.oncePaused();
  const response = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    ...params,
  });
  await Protocol.Debugger.resume();
  await Protocol.Debugger.disable();
  return response;
}

async function testCase1(evaluator, label) {
  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.sideEffects = [];
      Error.prepareStackTrace = function(err, frames) {
        sideEffects.push('prepareStackTrace');
        return 'custom stack';
      };
      globalThis.precreatedError = new Error('boom');
      undefined;
    `,
  });
  const response = await evaluator({
    expression: 'throw precreatedError',
    throwOnSideEffect: true,
  });
  const {result: {result: {value: sideEffects}}} =
      await Protocol.Runtime.evaluate({
        expression: 'sideEffects',
        returnByValue: true,
      });
  await Protocol.Runtime.evaluate({
    expression: 'delete Error.prepareStackTrace; undefined;',
  });
  InspectorTest.log(`${label}: sideEffects = ${JSON.stringify(sideEffects)}`);
}

async function testCase2(evaluator, label) {
  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.sideEffects = [];
      Error.prepareStackTrace = function(err, frames) {
        sideEffects.push('prepareStackTrace');
        return 'custom stack';
      };
      undefined;
    `,
  });
  const response = await evaluator({
    expression: 'sideEffects.push("eval")',
    throwOnSideEffect: true,
  });
  const {result: {result: {value: sideEffects}}} =
      await Protocol.Runtime.evaluate({
        expression: 'sideEffects',
        returnByValue: true,
      });
  await Protocol.Runtime.evaluate({
    expression: 'delete Error.prepareStackTrace; undefined;',
  });
  InspectorTest.log(`${label}: sideEffects = ${JSON.stringify(sideEffects)}`);
}

async function testCase3(evaluator, label) {
  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.arrayLikeErr =
          Object.assign(new Error('arr'), {0: 'a', 1: 'b', length: 2});
      Object.defineProperty(globalThis.arrayLikeErr, 'name', {
        get: Array.prototype.pop,
        configurable: true,
      });
      undefined;
    `,
  });
  const response = await evaluator({
    expression: 'throw arrayLikeErr',
    throwOnSideEffect: true,
  });
  const {result: {result: {value: length}}} = await Protocol.Runtime.evaluate({
    expression: 'arrayLikeErr.length',
    returnByValue: true,
  });
  const descriptionsMatch = response.result?.result?.description ===
      response.result?.exceptionDetails?.exception?.description;
  InspectorTest.log(`${label}: arrayLikeErr.length = ${length}`);
  InspectorTest.log(`${label}: descriptionsMatch = ${descriptionsMatch}`);
}

async function testCase4(evaluator, label) {
  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.objectWithMutatingPreview = {0: 'a', 1: 'b', length: 2};
      Object.defineProperty(globalThis.objectWithMutatingPreview, 'popped', {
        get: Array.prototype.pop,
        enumerable: true,
        configurable: true,
      });
      undefined;
    `,
  });
  const response = await evaluator({
    expression: 'throw objectWithMutatingPreview',
    throwOnSideEffect: true,
    generatePreview: true,
  });
  const {result: {result: {value: length}}} = await Protocol.Runtime.evaluate({
    expression: 'objectWithMutatingPreview.length',
    returnByValue: true,
  });
  const resultSubtype = response.result?.result?.subtype;
  const descriptionsMatch = response.result?.result?.description ===
      response.result?.exceptionDetails?.exception?.description;
  InspectorTest.log(`${label}: objectWithMutatingPreview.length = ${length}`);
  InspectorTest.log(`${label}: subtype = ${resultSubtype}`);
  InspectorTest.log(`${label}: descriptionsMatch = ${descriptionsMatch}`);
}

InspectorTest.runAsyncTestSuite([
  async function testRuntimeEvaluate() {
    await testCase1(evaluateViaRuntime, 'Case 1 (Runtime.evaluate)');
    await testCase2(evaluateViaRuntime, 'Case 2 (Runtime.evaluate)');
    await testCase3(evaluateViaRuntime, 'Case 3 (Runtime.evaluate)');
    await testCase4(evaluateViaRuntime, 'Case 4 (Runtime.evaluate)');
  },

  async function testDebuggerEvaluateOnCallFrame() {
    await testCase1(
        evaluateViaCallFrame, 'Case 1 (Debugger.evaluateOnCallFrame)');
    await testCase2(
        evaluateViaCallFrame, 'Case 2 (Debugger.evaluateOnCallFrame)');
    await testCase3(
        evaluateViaCallFrame, 'Case 3 (Debugger.evaluateOnCallFrame)');
    await testCase4(
        evaluateViaCallFrame, 'Case 4 (Debugger.evaluateOnCallFrame)');
  },
]);
