// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping into constructors.');

session.setupScriptMap();

contextGroup.addScript(
    `
function X() {
}

function f() {
  debugger;
  new X();
}

function g() {
  debugger;
  new Date();
}
`,
    0, 0, 'test.js');

(async function() {
  await Protocol.Debugger.enable();

  let break_break_point_hit_count = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    break_break_point_hit_count++;
    InspectorTest.log(
        `Hit ${break_break_point_hit_count}: ${callFrames[0].functionName}`);
    await session.logSourceLocation(callFrames[0].location);
    if (callFrames.length > 1) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  InspectorTest.log('--- Testing f() ---');
  break_break_point_hit_count = 0;
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`Hits after f() #1: ${break_break_point_hit_count}`);
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`Hits after f() #2: ${break_break_point_hit_count}`);
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`Hits after f() #3: ${break_break_point_hit_count}`);

  InspectorTest.log('--- Testing g() ---');
  break_break_point_hit_count = 0;
  await Protocol.Runtime.evaluate({expression: 'g()'});
  InspectorTest.log(`Hits after g(): ${break_break_point_hit_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
