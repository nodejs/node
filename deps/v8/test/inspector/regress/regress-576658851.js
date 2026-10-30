// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that side-effect-free evaluation enforces side-effect checks ' +
    'while generating the preview for a thrown value.');

contextGroup.addScript(`
  var sideEffects = [];
  globalThis.Request = {
    [Symbol.hasInstance](value) {
      sideEffects.push('Request.hasInstance');
      return false;
    }
  };
  var objectWithBody = {};
  Object.defineProperty(objectWithBody, 'body', {
    get: Date.now,
    enumerable: true,
  });
  var arrayLike = {0: 'a', 1: 'b', length: 2};
  Object.defineProperty(arrayLike, 'popped', {
    get: Array.prototype.pop,
    enumerable: true,
  });
  var rejectedWithoutMessage = Promise.reject(arrayLike);
  rejectedWithoutMessage.catch(() => {});
  var rejectedWithMessage = (async () => { throw arrayLike; })();
  rejectedWithMessage.catch(() => {});
`);

async function throwWithPreview(name, expression) {
  const {result} = await Protocol.Runtime.evaluate({
    expression,
    throwOnSideEffect: true,
    generatePreview: true,
  });
  InspectorTest.log(
      `${name}: ${result.exceptionDetails ? 'exception' : 'no exception'}`);
}

async function logExpression(name, expression) {
  const {result} = await Protocol.Runtime.evaluate(
      {expression, returnByValue: true});
  InspectorTest.log(`${name}: ${JSON.stringify(result.result.value)}`);
}

InspectorTest.runAsyncTestSuite([
  async function testThrownValuePreviewRunsMutatingBuiltinGetter() {
    await Protocol.Runtime.enable();
    await throwWithPreview('throw arrayLike', 'throw arrayLike');
    await logExpression('length after preview', 'arrayLike.length');
    await Protocol.Runtime.disable();
  },

  async function testThrownValuePreviewRunsUserHasInstance() {
    await Protocol.Runtime.enable();
    await throwWithPreview('throw objectWithBody', 'throw objectWithBody');
    await logExpression('sideEffects after preview', 'sideEffects');
    await Protocol.Runtime.disable();
  },

  async function testRejectedPromiseWithoutMessagePreviewSideEffect() {
    await Protocol.Runtime.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'rejectedWithoutMessage',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await logExpression('length after preview', 'arrayLike.length');
    await Protocol.Runtime.disable();
  },

  async function testRejectedPromiseWithMessagePreviewSideEffect() {
    await Protocol.Runtime.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'rejectedWithMessage',
      awaitPromise: true,
      throwOnSideEffect: true,
    }));
    await logExpression('length after preview', 'arrayLike.length');
    await Protocol.Runtime.disable();
  },
]);
