// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests top-level await in REPL mode evaluation.');

(async function test() {
  await Protocol.Runtime.enable();

  async function evaluate(expression) {
    const {result} = await Protocol.Runtime.evaluate({
      expression,
      replMode: true,
    });
    return result;
  }

  // Test that the completion value of the REPL script is the resolve value of
  // the promise returned by evaluate.
  let res = await evaluate('5;');
  InspectorTest.log(
      '5; -> ' + res.result.value +
      ' (exception: ' + Boolean(res.exceptionDetails) + ')');

  // Test that top-level await in REPL mode works.
  res = await evaluate('let x = await Promise.resolve(42);');
  InspectorTest.log(
      'let x = await Promise.resolve(42); -> ' + res.result.type +
      ' (exception: ' + Boolean(res.exceptionDetails) + ')');
  let xRes = await Protocol.Runtime.evaluate({expression: 'x'});
  InspectorTest.log('x -> ' + xRes.result.result.value);

  // Test that a throwing REPL script results in a rejected promise.
  res = await evaluate('throw new Error("ba dum tsh");');
  InspectorTest.log(
      'throw new Error("ba dum tsh"); -> exception: ' +
      Boolean(res.exceptionDetails) + ' ' +
      res.result.description.split('\n')[0]);

  // Test that a rejected promise throws.
  res = await evaluate('await Promise.reject("Reject promise!");');
  InspectorTest.log(
      'await Promise.reject("Reject promise!"); -> exception: ' +
      Boolean(res.exceptionDetails) + ' ' + res.result.value);

  // Test that we can bind a promise in REPL mode.
  await evaluate('let y = Promise.resolve(21);');
  let yRes =
      await Protocol.Runtime.evaluate({expression: 'y', awaitPromise: true});
  InspectorTest.log('await y -> ' + yRes.result.result.value);

  InspectorTest.completeTest();
})();
