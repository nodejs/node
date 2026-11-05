// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests async debug basic.');

const source = `
let listenerComplete = false;
let breakPointCount = 0;
let exceptionThrown = false;

async function f() {
  await (async function() { var a = "a"; await 1; debugger; })();

  var b = "b";
}

var c = "c";
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  let breakpointCount = 0;
  let listenerDone = false;
  let exceptionThrown = false;

  Protocol.Debugger.onPaused(async ({
                               params: {callFrames, asyncStackTrace}
                             }) => {
    try {
      breakpointCount++;
      listenerDone = true;

      let evalA = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[0].callFrameId, expression: 'a'});
      InspectorTest.log(`frame(0) evaluate('a'): ${evalA.result.result.value}`);

      if (callFrames.length < 2) {
        throw new Error('frame(1) does not exist on synchronous stack');
      }
      let evalB = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[1].callFrameId, expression: 'b'});
      InspectorTest.log(`frame(1) evaluate('b'): ${evalB.result.result.value}`);

      if (callFrames.length < 3) {
        throw new Error('frame(2) does not exist on synchronous stack');
      }
      let evalC = await Protocol.Debugger.evaluateOnCallFrame(
          {callFrameId: callFrames[2].callFrameId, expression: 'c'});
      InspectorTest.log(`frame(2) evaluate('c'): ${evalC.result.result.value}`);
    } catch (e) {
      exceptionThrown = true;
    }

    InspectorTest.log(`listenerDone: ${listenerDone}`);
    InspectorTest.log(`exceptionThrown: ${exceptionThrown}`);
    InspectorTest.log(`breakpointCount: ${breakpointCount}`);
    InspectorTest.log(
        `asyncStackTrace present: ${asyncStackTrace !== undefined}`);
    if (asyncStackTrace) {
      InspectorTest.log(`async parent: ${
          asyncStackTrace.callFrames.map(f => f.functionName || '<anonymous>')
              .join(', ')}`);
    }

    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f();'});
  await InspectorTest.waitForPendingTasks();

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
