// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests pause events for caught exceptions and breakpoints.');

contextGroup.addScript(
    `
function runCaughtException() {
  (function f() {
    try {
      x();
    } catch (e) {
      // Do nothing. Ignore exception.
    }
  })();
}

function a() {
  x = 1;
  y = 2;
  z = 3;
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames, reason, data}}) => {
    InspectorTest.log(
        'paused: reason=' + reason + ', func=' + callFrames[0].functionName +
        (data && 'uncaught' in data ? ', uncaught=' + data.uncaught : ''));
    await Protocol.Debugger.resume();
  });

  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.evaluate({expression: 'runCaughtException();'});
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});

  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 12,
    url: 'test.js',
  });
  await Protocol.Runtime.evaluate({expression: 'a();'});

  InspectorTest.completeTest();
})();
