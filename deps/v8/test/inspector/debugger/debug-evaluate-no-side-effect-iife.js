// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that declaring local variables in IIFEs works with
// side-effect free debug-evaluate.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-iife.');

contextGroup.addScript(`
function f() {
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  const evaluatePromise = Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const callFrameId = callFrames[0].callFrameId;

  async function success(expectation, source) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(expectation + ' === ' + result.result.value);
  }

  async function fail(source) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(
        result.exceptionDetails ? result.result.className : 'succeeded');
  }

  // Declaring 'a' sets a property to the global object.
  await fail('var a = 3');
  await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'var a = 2',
    throwOnSideEffect: false,
  });
  const checkA = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'a',
  });
  InspectorTest.log('a === ' + checkA.result.result.value);

  // Wrapping into an IIFE would be fine, since 'a' is local.
  await success(100, `(function(x) {
          var a = 0;
          for (var i = 0; i < x; i++) {
            a += x;
          }
          return a;
        })(10);`);
  await success(100, `(x => {
          let a = 0;
          for (var i = 0; i < x; i++) {
            a += x;
          }
          return a;
        })(10);`);
  // Not using 'var' to declare would make the access go to global object.
  await fail(`(function(x) {
          a = 0;
          for (var i = 0; i < x; i++) {
            a += x;
          }
          return a;
        })(10);`);

  await Protocol.Debugger.resume();
  await evaluatePromise;
  InspectorTest.completeTest();
})();
