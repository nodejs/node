// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-sparkplug --allow-natives-syntax

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-crbug-1357554.');

session.setupScriptMap();

const source = `
function run() {
  %ScheduleBreak();
  (function foo() {
    const x = 5;
    () => x; // context-allocate x.
  })();
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evaluatePromise = Protocol.Runtime.evaluate({expression: 'run()'});
  const {params: {callFrames}} = await pausedPromise;
  await session.logSourceLocation(callFrames[0].location);

  const stepPromise = Protocol.Debugger.oncePaused();
  await Protocol.Debugger.stepInto();
  const {params: {callFrames: stepFrames}} = await stepPromise;
  await session.logSourceLocation(stepFrames[0].location);

  await Protocol.Debugger.resume();
  await evaluatePromise;
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
