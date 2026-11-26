// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests crbug.com/1259878.');

session.setupScriptMap();
contextGroup.addScript(
    `
function f() {
var C = class {};
}`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setBreakpointByUrl({url: 'test.js', lineNumber: 2});
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evalPromise = Protocol.Runtime.evaluate({expression: 'f()'});
  const {params: {callFrames}} = await pausedPromise;
  await session.logSourceLocation(callFrames[0].location);
  await Protocol.Debugger.resume();
  await evalPromise;
  InspectorTest.completeTest();
})();
