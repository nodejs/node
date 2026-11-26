// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'This test tests that full code compiled without debug break slots ' +
    'is recompiled with debug break slots when debugging is started.');

contextGroup.addScript(
    `
var done = false;
var set_bp = false;

function f() {
  var a = 0;
  if (set_bp) {
    debugger;
  }
  var i = 1;
  var j = 2;
  done = true;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let breakpointId = null;
  let step_count = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    // If paused at the debugger statement inside f, set breakpoint and resume.
    if (callFrames[0].location.lineNumber === 7) {
      const res = await Protocol.Debugger.setBreakpointByUrl({
        url: 'test.js',
        lineNumber: 9,
      });
      breakpointId = res.result.breakpointId;
      await Protocol.Debugger.resume();
      return;
    }

    step_count++;
    const {result: {result: doneRes}} = await Protocol.Runtime.evaluate({
      expression: 'done',
    });
    if (!doneRes.value) {
      await Protocol.Debugger.stepOver();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  // Prepare and run f before breakpoint.
  await Protocol.Runtime.evaluate({
    expression: 'done = false; set_bp = false; f();',
  });

  // Make f set a breakpoint with an activation on the stack.
  step_count = 0;
  await Protocol.Runtime.evaluate({
    expression: 'done = false; set_bp = true; f();',
  });
  InspectorTest.log(`step_count: ${step_count}`);

  await Protocol.Debugger.removeBreakpoint({breakpointId});

  // Set a breakpoint on the first var statement (line 9).
  ({result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 9,
  }));

  // Step through the function ensuring that the var statements are hit.
  step_count = 0;
  await Protocol.Runtime.evaluate({
    expression: 'done = false; set_bp = false; f();',
  });
  InspectorTest.log(`step_count: ${step_count}`);

  // Clear the breakpoint and check that no stepping happens.
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  step_count = 0;
  await Protocol.Runtime.evaluate({
    expression: 'done = false; set_bp = false; f();',
  });
  InspectorTest.log(`step_count: ${step_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
