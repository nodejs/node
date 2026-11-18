// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping over caught exception in inlined function.');

contextGroup.addScript(
    `
function f() {
  throw new Error();
}

function g() {
  try {
    f();
  } catch (e) {
    return 1;  // Break
  }
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
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (message.params.reason === 'exception') {
      Protocol.Debugger.stepOver();
    } else {
      Protocol.Debugger.resume();
    }
  });
  await Protocol.Runtime.evaluate(
      {expression: '%OptimizeFunctionOnNextCall(h); h();'});
  InspectorTest.completeTest();
})();
