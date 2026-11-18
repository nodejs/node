// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-3225.');

contextGroup.addScript(`
function* generator(a, b) {
  function set_a_to_5() { a = 5 }
  // Make sure set_a_to_5 is 'used' so that it is visible to the debugger.
  set_a_to_5;
  var b = 3;  // Shadows a parameter.
  debugger;
  yield a;
  yield b;
  debugger;
  yield a;
  return b;
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  let debugStep = 0;
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    if (debugStep === 0) {
      const aRes = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'a'});
      const bRes = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'b'});
      InspectorTest.log(`step 0: a=${aRes.result.result.value}, b=${
          bRes.result.result.value}`);
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'a = 4'});
      debugStep++;
    } else {
      const aRes = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'a'});
      const bRes = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'b'});
      InspectorTest.log(`step 1: a=${aRes.result.result.value}, b=${
          bRes.result.result.value}`);
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'set_a_to_5()'});
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'b = 5'});
    }
    Protocol.Debugger.resume();
  });

  const {result: {result}} = await Protocol.Runtime.evaluate({
    expression: `
      var foo = generator(1, 2);
      JSON.stringify([
        foo.next().value,
        foo.next().value,
        foo.next().value,
        foo.next().value
      ]);
    `
  });
  InspectorTest.log('yielded: ' + result.value);
  InspectorTest.completeTest();
})();
