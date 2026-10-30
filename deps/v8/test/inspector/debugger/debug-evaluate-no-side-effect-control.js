// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-control.');

contextGroup.addScript(`
var o = { p : 1 };

function f() {
  debugger;
}
`);

var successes = [
  [
    45, `(function() {
      var sum = 0;
      for (var i = 0; i < 10; i++) sum += i;
      return sum;
    })()`
  ],
  [
    '0012', `(function() {
      var sum = 0;
      for (var i in [1, 2, 3]) sum += i;
      return sum;
    })()`
  ],
  [
    15, `(function() {
      var sum = 1;
      while (sum < 12) sum += sum + 1;
      return sum;
    })()`
  ],
  [
    15, `(function() {
      var sum = 1;
      do { sum += sum + 1; } while (sum < 12);
      return sum;
    })()`
  ],
  [
    '023', `(function() {
      var sum = "";
      for (var i = 0; i < 4; i++) {
        switch (i) {
          case 0:
          case 1:
            if (i == 0) sum += i;
            break;
          default:
          case 3:
            sum += i;
            break;
        }
      }
      return sum;
    })()`
  ],
  [
    'oups', `(function() {
      try {
        if (Math.sin(1) < 1) throw new Error("oups");
      } catch (e) {
        return e.message;
      }
    })()`
  ],
  [
    6, `(function() {  // Iterator.prototype.next performs stores.
      var sum = 0;
      for (let i of [1, 2, 3]) sum += i;
      return sum;
    })()`
  ]
];

var fails = [
  `(function() {  // Store to scope object.
     with (o) {
       p = 2;
     }
   })()`,
];

(async function test() {
  await Protocol.Debugger.enable();
  const evaluatePromise = Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const callFrameId = callFrames[0].callFrameId;

  for (const [expectation, source] of successes) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: source,
      throwOnSideEffect: true,
    });
    InspectorTest.log(expectation + ' === ' + result.result.value);
  }

  for (const testExpr of fails) {
    const {result} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: testExpr,
      throwOnSideEffect: true,
    });
    InspectorTest.log(
        result.exceptionDetails ? result.result.className : 'succeeded');
  }

  await Protocol.Debugger.resume();
  await evaluatePromise;
  InspectorTest.completeTest();
})();
