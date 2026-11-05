// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into microtasks.');

const source = `
var exception = null;
var break_count = 0;
const expected_breaks = 10;

Promise.resolve(42)
  .then(promise1)
  .then(Object) // Should skip stepping into native.
  .then(Boolean) // Should skip stepping into native.
  .then(promise2)
  .catch(promise3)
  .then(promise4)
  .catch(function(e) {
    %AbortJS("FAIL: uncaught exception " + e);
  });

function promise1() {
  debugger; // Break 0.
  return exception || 1 // Break 1.
  ; // Break 2. StepOver.
}

function promise2() {
  throw new Error; // Break 3.
}

function promise3() {
  return break_count // Break 4.
  ; // Break 5.
}

function promise4() {
  finalize(); // Break 6. StepOver.
  return 0 // Break 7.
  ; // Break 8. StepOut.
}

function finalize() {
  Promise.resolve().then(function() {
    if (expected_breaks !== break_count) { // Break 9. StepOut.
      %AbortJS("FAIL: expected <" + expected_breaks + "> breaks instead of <" +
               break_count + ">");
    }
    if (exception !== null) {
      %AbortJS("FAIL: exception: " + exception);
    }
  });
}
`;

(async function test() {
  await Protocol.Debugger.enable();

  let break_count = 0;
  const lines = source.split('\n');

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let callFrame = callFrames[0];
    let line = lines[callFrame.location.lineNumber];
    InspectorTest.log('paused at: ' + line.trim());
    if (!line.includes('// Break ' + break_count + '.')) {
      InspectorTest.log('FAIL: Unexpected pause at: ' + line);
    }
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrame.callFrameId,
      expression: '++break_count',
    });
    break_count++;
    if (line.includes('StepOver.')) {
      await Protocol.Debugger.stepOver();
    } else if (line.includes('StepOut.')) {
      await Protocol.Debugger.stepOut();
    } else {
      await Protocol.Debugger.stepInto();
    }
  });

  contextGroup.addScript(source, 0, 0, 'test.js');

  await InspectorTest.waitForPendingTasks();
  InspectorTest.log('Total breaks: ' + break_count);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
