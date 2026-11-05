// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/576575667.');

contextGroup.addScript(
    `async function donor({a1, a2, a3, a4, a5} = {}) { await 1; }
function victim() {
  let v = 0;
  v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0;
  v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0;
  v=0; v=0; v=0; v=0; v=0;
  corrupt();
  v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0; v=0;
  return 0;
}`,
    0, 0, 'test.js');

contextGroup.addScript(`
function corrupt() {
  const donor_sfi = Sandbox.readObjectField(donor, 'shared_function_info');
  Sandbox.corruptObjectField(victim, 'shared_function_info', donor_sfi);
}

function trigger() {
  victim();
}
`);

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 0,
    columnNumber: 0,
  });
  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 1,
    columnNumber: 0,
  });
  Protocol.Debugger.onPaused(async () => {
    await Protocol.Debugger.stepOver();
  });
  await Protocol.Runtime.evaluate({expression: 'trigger()'});
  InspectorTest.completeTest();
})();
