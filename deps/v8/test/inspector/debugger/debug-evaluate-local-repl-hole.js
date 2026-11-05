// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that *local* debug-evaluate properly works for REPL 'let'
// redeclarations.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that local debug-evaluate works for REPL let re-declarations.');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'x'});
    InspectorTest.log(`x in foo: ${result.value}`);
    await Protocol.Debugger.resume();
  });

  // First script introduces a let-binding 'x'. The value of
  // 'x' lives in the ScriptContext of this REPL script.
  await Protocol.Runtime.evaluate({expression: 'let x = 21;', replMode: true});

  // The second script redeclares 'x', but then breaks and
  // evaluates x.
  await Protocol.Runtime.evaluate({
    expression: `
      let x = 42;
      (function foo() {
        debugger;
      })();
    `,
    replMode: true
  });

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
