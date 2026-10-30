// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-enable-one-shot-optimization

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-evaluate-no-side-effect-array-fill.');

contextGroup.addScript(`
var array = [1, 2, 3];
function f() {
  debugger;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;

    const failRes = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'array.fill(1)', throwOnSideEffect: true});
    InspectorTest.log('array.fill(1): ' + failRes.result.result.className);

    const successRes = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId,
      expression: '[1, 2, 3].fill(1)',
      throwOnSideEffect: true,
      returnByValue: true
    });
    InspectorTest.log(
        '[1, 2, 3].fill(1): ' + JSON.stringify(successRes.result.result.value));

    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();
