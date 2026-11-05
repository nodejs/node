// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test fails without defeating TDZ check elision because 'return x;'
// below, without a ThrowReferenceErrorIfHole bytecode, becomes a return
// position breakpoint. Return position breakpoints can only inspect the
// function scope because their source position is always at the end of the
// function.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests scope chain at return position in generator switch.');

contextGroup.addScript(
    `
function* serialize() {
  debugger;
  switch (0) {
    case 0:
      let x = 1;  // Defeat TDZ check elision
    case 1:
      return x;  // Check scopes
  }
}
`,
    0, 0, 'test.js');

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  let step_count = 0;
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    if (step_count++ < 3) {
      Protocol.Debugger.stepOver();
    } else {
      InspectorTest.logMessage(
          message.params.callFrames[0].scopeChain.map(s => s.type));
      Protocol.Debugger.resume();
    }
  });
  await Protocol.Runtime.evaluate(
      {expression: 'let gen = serialize(); gen.next();'});
  InspectorTest.completeTest();
})();
