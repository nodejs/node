// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/808973.');

contextGroup.addScript(`
function sleep() {
  return new Promise(resolve => setTimeout(resolve, 0));
}
async function thrower() {
  await sleep();
  throw "a"; // Exception a
}
`);

session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
  const pausedPromise = Protocol.Debugger.oncePaused();
  await Protocol.Runtime.evaluate(
      {expression: '(async function() { thrower(); })();'});
  const message = await pausedPromise;
  await session.logSourceLocation(message.params.callFrames[0].location);
  await Protocol.Debugger.resume();
  InspectorTest.completeTest();
})();
