// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sparkplug --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test stepping out from baseline function into optimized function.');

(async function test() {
  await session.setupScriptMap();

  const script = `
    function f() {
      debugger;
      return 1;
    }

    function g() {
      return f();  // Break
    }

    function h() {
      return g();
    }

    // Ensure FeedbackVector to consider f for inlining.
    %EnsureFeedbackVectorForFunction(f);
    %CompileBaseline(g);

    %PrepareFunctionForOptimization(h);
    h();
    h();
  `;

  contextGroup.addScript(script, 0, 0, 'test.js');

  await Protocol.Debugger.enable();

  Protocol.Runtime.evaluate({
    expression: `
      %OptimizeFunctionOnNextCall(h);
      h();
    `
  });

  const {params: {callFrames: callFrames1}} =
      await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.stepOut();

  const {params: {callFrames: callFrames2}} =
      await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(callFrames2[0].location);
  await Protocol.Debugger.resume();

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
