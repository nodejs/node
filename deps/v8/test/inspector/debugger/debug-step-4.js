// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests how debugger can step over not necessarily in the top frame.');

contextGroup.addScript(
    `
var state;

function f() {
  var a = 1978;
  for (state[2] = 0; state[2] < 3; state[2]++) {
    void String(a);
  }
}
function g() {
  for (state[1] = 0; state[1] < 3; state[1]++) {
    f();
  }
}
function h() {
  state = [-1, -1, -1];
  for (state[0] = 0; state[0] < 3; state[0]++) {
    g();
  }
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  // Warm-up -- make sure all is compiled and ready for breakpoint.
  await Protocol.Runtime.evaluate({expression: 'h();'});

  let listenerState = 0;
  let bpId;
  let stateSnapshot;

  Protocol.Debugger.onPaused(async message => {
    const loc = message.params.callFrames[0].location;
    InspectorTest.log(
        `Here (${listenerState}): ${loc.lineNumber}:${loc.columnNumber}`);
    if (listenerState === 0) {
      await Protocol.Debugger.removeBreakpoint({breakpointId: bpId});
      listenerState = 1;
      await Protocol.Debugger.stepOver();
    } else if (listenerState === 1) {
      const {result} = await Protocol.Runtime.evaluate({
        expression: 'String(state)',
      });
      stateSnapshot = result.result.value;
      InspectorTest.log('State: ' + stateSnapshot);
      listenerState = 2;
      await Protocol.Debugger.resume();
    } else {
      await Protocol.Debugger.resume();
    }
  });

  const {result} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 4,
    columnNumber: 0,
    url: 'test.js',
  });
  bpId = result.breakpointId;

  await Protocol.Runtime.evaluate({expression: 'h();'});

  if (stateSnapshot !== '0,0,-1') {
    InspectorTest.log('FAIL: unexpected state ' + stateSnapshot);
  }

  InspectorTest.completeTest();
})();
