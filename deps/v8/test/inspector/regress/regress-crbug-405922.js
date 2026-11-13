// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-405922.');

contextGroup.addScript(`
function f(x) {
  if (x > 0) %Call(f, null, x-1);
}

function testRun() {
  debugger;
  f(2);
}
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    Protocol.Debugger.stepInto();
  });
  await Protocol.Runtime.evaluate({expression: 'testRun()'});
  InspectorTest.completeTest();
})();
