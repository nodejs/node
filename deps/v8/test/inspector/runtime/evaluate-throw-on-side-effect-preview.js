// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that side-effect-free evaluation enforces side-effect checks while generating a preview');

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
`);

async function evaluateWithPreview(name, expression) {
  const {result} = await Protocol.Runtime.evaluate({
    expression,
    throwOnSideEffect: true,
    generatePreview: true,
  });
  if (result.exceptionDetails) {
    InspectorTest.log(`${name}: exception`);
  } else {
    InspectorTest.log(`${name}: ok (${result.result.type})`);
  }
}

async function logExpression(name, expression) {
  const {result} = await Protocol.Runtime.evaluate(
      {expression, returnByValue: true});
  InspectorTest.log(`${name}: ${JSON.stringify(result.result.value)}`);
}

InspectorTest.runAsyncTestSuite([
  async function testPreviewRunsUserHasInstance() {
    await Protocol.Runtime.enable();
    await evaluateWithPreview(
        'preview with Request[Symbol.hasInstance]', 'objectWithBody');
    await logExpression('sideEffects after preview', 'sideEffects');
    await Protocol.Runtime.disable();
  },

  async function testPreviewRunsMutatingBuiltinGetter() {
    await Protocol.Runtime.enable();
    await evaluateWithPreview(
        'preview with Array.prototype.pop getter', 'arrayLike');
    await logExpression('length after preview', 'arrayLike.length');
    await Protocol.Runtime.disable();
  },
]);
