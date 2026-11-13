// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests optimized-debug-frame.');

contextGroup.addScript(`
function f() {
  debugger;
}

function g(x) {
  return f();
}

function warmup() {
  %PrepareFunctionForOptimization(g);
  g();
  g();
  %OptimizeFunctionOnNextCall(g);
}
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Runtime.evaluate({expression: 'warmup()'});
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[1].location);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'g()'});
  InspectorTest.completeTest();
})();
