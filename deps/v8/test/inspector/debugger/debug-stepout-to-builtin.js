// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test step out from function without local variables to builtin.');

session.setupScriptMap();

contextGroup.addScript(
    `
var obj = {key: 10};

function replacer(key, value) {
  if (key === 'key') {
    debugger;
  }
  return value;
}

function testStepOutToBuiltIn() {
  JSON.stringify(obj, replacer);
}  // expected line
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  let state = 1;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (state === 1) {
      InspectorTest.log('Paused in replacer');
      state = 2;
      await Protocol.Debugger.stepOut();
    } else if (state === 2) {
      InspectorTest.log('Paused after stepOut:');
      InspectorTest.log(`Function name: ${callFrames[0].functionName}`);
      await session.logSourceLocation(callFrames[0].location);
      state = 3;
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'testStepOutToBuiltIn()'});
  InspectorTest.log(`Final state: ${state}`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
