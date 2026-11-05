// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests evaluateOnCallFrame with nested let bindings.');

contextGroup.addScript(`
function checkOptimized(fn) {
  const status = %GetOptimizationStatus(fn);
  if ((status & (1 << 2)) !== 0 || (status & (1 << 20)) !== 0) return;
  if ((status & (1 << 3)) === 0) {
    throw new Error('Expected ' + fn.name + ' to be optimized');
  }
}

function f() {
  let a = 0;
  function g() {
    let a = 1;
    {
      let a = 2;
      debugger;  // Breakpoint.
      if (a !== 3) {
        // We cannot change stack locals in optimized frames.
        if (a !== 2) throw new Error('Expected a === 2, got ' + a);
        checkOptimized(g);
      }
    }
    if (a !== 1) throw new Error('Expected a === 1, got ' + a);
  }
  g.call(1);
  if (a !== 4) {
    // We cannot change stack locals in optimized frames.
    if (a !== 0) throw new Error('Expected a === 0, got ' + a);
    checkOptimized(f);
  }
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const scope0 = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'a',
      scopeNumber: 0,
    });
    InspectorTest.log(
        'frame(0).scope(0).evaluate("a"): ' + scope0.result.result.value);

    const scope1 = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'a',
      scopeNumber: 1,
    });
    InspectorTest.log(
        'frame(0).scope(1).evaluate("a"): ' + scope1.result.result.value);

    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'a = 3',
    });
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[1].callFrameId,
      expression: 'a = 4',
    });

    const modifyThis = await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'this = 2',
    });
    InspectorTest.log(
        'this = 2 throws: ' + Boolean(modifyThis.result.exceptionDetails));

    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log('hasException: ' + Boolean(result.exceptionDetails));
  InspectorTest.completeTest();
})();
