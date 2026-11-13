// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests default parameters debugging.');

const source = `
function f(a = function modeFn(mode) { debugger; return mode; },
           b = a("default"),
           c = (function() { debugger; })()) {
  var d = 'local';
  debugger;
};
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let breakPointCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    breakPointCount++;
    InspectorTest.log(`Paused #${breakPointCount}`);
    if (breakPointCount === 1) {
      // Break point in initializer for parameter \`a\`, invoked by
      // initializer for parameter \`b\`
      let {result: {result: modeResult}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrames[0].callFrameId, expression: 'mode'});
      InspectorTest.log(`frame(0) evaluate('mode'): ${modeResult.value}`);

      let evalB = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[1].callFrameId, expression: 'b'});
      InspectorTest.log(`frame(1) evaluate('b') threw: ${
          evalB.result.exceptionDetails !== undefined}`);

      let evalC = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[1].callFrameId, expression: 'c'});
      InspectorTest.log(`frame(1) evaluate('c') threw: ${
          evalC.result.exceptionDetails !== undefined}`);
    } else if (breakPointCount === 2) {
      // Break point in IIFE initializer for parameter \`c\`
      let {result: {result: aNameResult}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrames[1].callFrameId, expression: 'a.name'});
      InspectorTest.log(`frame(1) evaluate('a.name'): ${aNameResult.value}`);

      let {result: {result: bResult}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrames[1].callFrameId, expression: 'b'});
      InspectorTest.log(`frame(1) evaluate('b'): ${bResult.value}`);

      let evalC = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[1].callFrameId, expression: 'c'});
      InspectorTest.log(`frame(1) evaluate('c') threw: ${
          evalC.result.exceptionDetails !== undefined}`);
    } else if (breakPointCount === 3) {
      // Break point in function body --- \`c\` parameter is shadowed
      let {result: {result: aNameResult}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrames[0].callFrameId, expression: 'a.name'});
      InspectorTest.log(`frame(0) evaluate('a.name'): ${aNameResult.value}`);

      let {result: {result: bResult}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrames[0].callFrameId, expression: 'b'});
      InspectorTest.log(`frame(0) evaluate('b'): ${bResult.value}`);

      let {result: {result: dResult}} =
          await Protocol.Debugger.evaluateOnCallFrame(
              {callFrameId: callFrames[0].callFrameId, expression: 'd'});
      InspectorTest.log(`frame(0) evaluate('d'): ${dResult.value}`);
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f();'});
  InspectorTest.log(`Total breakpoints hit: ${breakPointCount}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
