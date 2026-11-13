// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting breakpoints on generators that are currently on the stack.');

contextGroup.addScript(
    `
function* g() {
  setbreaks();
  yield 1;  // B1
}

function* f() {
  yield* g();
  return 2;  // B2
}

function setbreaks() {
  debugger;
}
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();

  let pauseCount = 0;
  Protocol.Debugger.onPaused(async (message) => {
    pauseCount++;
    if (pauseCount === 1) {
      await Protocol.Debugger.setBreakpointByUrl({
        lineNumber: 3,
        url: 'test.js',
      });
      await Protocol.Debugger.setBreakpointByUrl({
        lineNumber: 8,
        url: 'test.js',
      });
    } else {
      await session.logSourceLocation(message.params.callFrames[0].location);
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'for (let _ of f()) {}'});
  InspectorTest.completeTest();
})();
