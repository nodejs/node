// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests debug-scope-on-return.');

contextGroup.addScript(`
function f() {
  debugger;

  L: with ({x:12}) {
    break L;
  }

  return;
}
`);

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  Protocol.Debugger.onPaused(async message => {
    const frame = message.params.callFrames[0];
    await session.logSourceLocation(frame.location);
    InspectorTest.log(
        'scopes: ' + frame.scopeChain.map(s => s.type).join(', '));
    Protocol.Debugger.stepInto();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.completeTest();
})();
