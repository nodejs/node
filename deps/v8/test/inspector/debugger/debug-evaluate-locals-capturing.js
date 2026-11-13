// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests capturing and modifying local variable from evaluateOnCallFrame ' +
    'inside catch block.');

contextGroup.addScript(
    `
var f = null;
var i = null;

function runTest() {
  var localBefore;
  var localAfter;
  (function () {
    var i = 0;
    try {
      throw new Error();
    } catch (e) {
      localBefore = i;
      debugger;
      localAfter = i;
    }
  }());
  var globalBefore = i;
  f();
  var globalAfter = i;
  return [localBefore, localAfter, globalBefore, globalAfter];
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'f = function() { i = 5; }, f(), i'
    });
    InspectorTest.log(`evaluated i: ${result.value}`);
    await Protocol.Debugger.resume();
  });

  const {result: {result}} = await Protocol.Runtime.evaluate(
      {expression: 'runTest()', returnByValue: true});
  InspectorTest.log(`results: ${JSON.stringify(result.value)}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
