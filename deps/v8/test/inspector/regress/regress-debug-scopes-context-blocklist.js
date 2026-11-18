// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-debug-scopes-context-blocklist.');

contextGroup.addScript(`
var x = 'global_x';

function outer() {
  let y = 'y';
  () => y;  // force context allocation for outer

  // intermediate scope, no context needed
  function intermediate() {
    let x = 'intermediate_x';  // stack variable

    // innermost scope, paused here. No context needed.
    function inner() {
      debugger;
    }
    inner();
  }

  return intermediate;
}

let f = outer();
`);

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const callFrameId = message.params.callFrames[0].callFrameId;
    const {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'x', scopeNumber: 2});
    InspectorTest.logMessage(result);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();
