// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests evaluateOnCallFrame inside with blocks.');

contextGroup.addScript(`
function f() {
  var a = 1;
  var b = 2;
  with ({a:2}) {
    debugger;  // Breakpoint.
    x = {a:3,b:1};
    with (x) {
      debugger;  // Breakpoint.
    }
  }
}

var foo = "global";
function runEvalWith() {
  eval(
      "with({bar:'with'}) { " +
      "(function g() { var foo = 'local'; debugger; })(); }");
}
`);

async function evalOnFrame(frame, expression) {
  const {result} = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: frame.callFrameId,
    expression,
  });
  return result.result.value;
}

(async function test() {
  await Protocol.Debugger.enable();

  let breakPointCount = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    breakPointCount++;
    if (breakPointCount === 1 || breakPointCount === 2) {
      InspectorTest.log(
          'break ' + breakPointCount +
          ': a=' + await evalOnFrame(callFrames[0], 'a') +
          ', b=' + await evalOnFrame(callFrames[0], 'b'));
    } else if (breakPointCount === 3) {
      InspectorTest.log(
          'break 3: foo=' + await evalOnFrame(callFrames[0], 'foo'));
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f(); runEvalWith();'});
  InspectorTest.log('breakPointCount: ' + breakPointCount);
  InspectorTest.completeTest();
})();
