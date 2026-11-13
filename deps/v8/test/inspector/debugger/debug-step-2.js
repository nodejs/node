// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'This test tests that full code compiled without debug break slots ' +
    'is recompiled with debug break slots when debugging is started.');

contextGroup.addScript(
    `
var done = false;

function f() {
  var i = 1;
  var j = 2;
  done = true;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let step_count = 0;
  Protocol.Debugger.onPaused(async () => {
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

  // Test function to step through before any breakpoint.
  await Protocol.Runtime.evaluate({expression: 'done = false; f();'});

  // Set a breakpoint on the first var statement (line 4 in test.js).
  let {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 4,
  });

  step_count = 0;
  await Protocol.Runtime.evaluate({expression: 'done = false; f();'});
  InspectorTest.log(`step_count: ${step_count}`);

  await Protocol.Debugger.removeBreakpoint({breakpointId});

  // Set a breakpoint on the first var statement again.
  ({result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 4,
  }));

  // Step through the function ensuring that the var statements are hit.
  step_count = 0;
  await Protocol.Runtime.evaluate({expression: 'done = false; f();'});
  InspectorTest.log(`step_count: ${step_count}`);

  // Clear the breakpoint and check that no stepping happens.
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  step_count = 0;
  await Protocol.Runtime.evaluate({expression: 'done = false; f();'});
  InspectorTest.log(`step_count: ${step_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
