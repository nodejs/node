// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests stepping out of inlined function.');

contextGroup.addScript(
    `
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

%PrepareFunctionForOptimization(h);
h();
h();
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  let step_count = 0;
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (step_count++ == 0) {
      Protocol.Debugger.stepOut();
    } else {
      Protocol.Debugger.resume();
    }
  });
  await Protocol.Runtime.evaluate(
      {expression: '%OptimizeFunctionOnNextCall(h); h();'});
  InspectorTest.completeTest();
})();
