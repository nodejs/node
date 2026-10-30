// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-323936.');

contextGroup.addScript(`
var holder = null;

function f(e) {
  var x = "original";
  try {
    throw "error";
  } catch (e) {
    function write_0(v) { e = v }
    function write_1(v) { x = v }
    // Create functions for two functions so that context variable stays included
    write_0, write_1;
    debugger;
    if (e !== "foo") throw new Error("Expected 'foo', got " + e);
    holder = write_0;
  }
  if (x !== "modified") throw new Error("Expected 'modified', got " + x);
  if (e !== "argument") throw new Error("Expected 'argument', got " + e);
  holder("bar");
  if (e !== "argument") throw new Error("Expected 'argument', got " + e);
  function write_2(v) { e = v }
  write_2;
  debugger;
  if (e !== "bar") throw new Error("Expected 'bar', got " + e);
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  let step = 0;
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    if (step === 0) {
      const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'e'});
      InspectorTest.log('step 0 e: ' + result.value);
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'write_0(\'foo\')'});
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'write_1(\'modified\')'});
    } else {
      const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'e'});
      InspectorTest.log('step 1 e: ' + result.value);
      await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId, expression: 'write_2(\'bar\')'});
    }
    step++;
    Protocol.Debugger.resume();
  });

  const {result} =
      await Protocol.Runtime.evaluate({expression: 'f("argument")'});
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
