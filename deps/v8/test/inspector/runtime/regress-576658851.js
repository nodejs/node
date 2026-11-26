// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that when wrapping a thrown exception fails a side-effect check ' +
    'during side-effect-free evaluation, result.result wraps the resulting ' +
    'EvalError under a SideEffectCheckScope.');

contextGroup.addScript(`
  var sideEffects = [];
  Error.prepareStackTrace = () => {
    sideEffects.push('prepareStackTrace');
    return 'custom stack';
  };
  var arrayLike = {0: 'a', 1: 'b', length: 2};
  Object.defineProperty(arrayLike, 'popped', {
    get: Array.prototype.pop,
    enumerable: true,
  });
  var errWithGetter = new Error();
  Object.setPrototypeOf(errWithGetter, {
    get message() {
      sideEffects.push('message getter');
      return 'boom';
    }
  });
`);

async function logExpression(name, expression) {
  const {result} = await Protocol.Runtime.evaluate(
      {expression, returnByValue: true});
  InspectorTest.log(`${name}: ${JSON.stringify(result.result.value)}`);
}

InspectorTest.runAsyncTestSuite([
  async function testPreviewSideEffectWrapsEvalErrorInResult() {
    await Protocol.Runtime.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'throw arrayLike',
      throwOnSideEffect: true,
    }));
    await logExpression('length after evaluate', 'arrayLike.length');
    await logExpression('sideEffects after evaluate', 'sideEffects');
    await Protocol.Runtime.disable();
  },

  async function testMirrorCreateSideEffectWrapsEvalErrorInResult() {
    await Protocol.Runtime.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'throw errWithGetter',
      throwOnSideEffect: true,
    }));
    await logExpression('sideEffects after evaluate', 'sideEffects');
    await Protocol.Runtime.disable();
  },
]);
