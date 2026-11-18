// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests elided-instruction.');

contextGroup.addScript(`
function f() {
  var a = 1;                     // Break 2. 10.
  return a;                      // Break 3. 11.
}

function testRun() {
  debugger;                      // Break 0. 0.
  f();                           // Break 1. 0.
}                                // Break 4. 0.
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
