// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping into through Array.prototype.forEach callbacks.');

session.setupScriptMap();

contextGroup.addScript(
    `
var bound_callback = callback.bind(null);

function run() {
  debugger; // Break 0.
  [1, 2].forEach(callback); // Break 1.
  [3, 4].forEach(bound_callback); // Break 6.
}

function callback(x) {
  return x // Break 2. // Break 4. // Break 7. // Break 9.
  ; // Break 3. // Break 5. // Break 8. // Break 10.
}
`,
    0, 0, 'test.js');

(async function() {
  await Protocol.Debugger.enable();

  let break_count = 0;
  const expected_breaks = 11;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    InspectorTest.log(
        `paused at: ${callFrames[0].functionName} (break ${break_count})`);
    await session.logSourceLocation(callFrames[0].location);
    break_count++;
    if (break_count < expected_breaks) {
      await Protocol.Debugger.stepInto();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  await Protocol.Runtime.evaluate({expression: 'run()'});
  InspectorTest.log(`Total breaks: ${break_count}`);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
