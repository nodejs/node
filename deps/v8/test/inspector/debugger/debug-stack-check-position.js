// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Check that even though we schedule and break on function entry, we ' +
    'still pause on the first breakable position.');

session.setupScriptMap();

contextGroup.addScript(`
function f() {
  return 1;        // BREAK
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({
    expression: '%ScheduleBreak(); f();',
  });
  const {params: {callFrames}} = await pausedPromise;
  await session.logSourceLocation(callFrames[0].location);
  await Protocol.Debugger.resume();
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
