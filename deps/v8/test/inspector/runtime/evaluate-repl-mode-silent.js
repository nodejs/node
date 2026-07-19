// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} = InspectorTest.start(
  "Tests that Runtime.evaluate works with REPL mode in silent");

session.setupScriptMap();

(async function() {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({ state: 'uncaught' });

  Protocol.Debugger.onPaused(async ({ params: { callFrames, reason } }) => {
    InspectorTest.log(`Paused because of '${reason}' at`);
    await session.logSourceLocation(callFrames[0].location);
    Protocol.Debugger.resume();
  });

  // First, create an unresolved promise and wait for it in a silent REPL mode
  // evaluation.
  const evalPromise = Protocol.Runtime.evaluate({
    expression: `
      let silentReject;
      await new Promise((r, rej) => { silentReject = rej; });`,
    replMode: true,
    silent: true,
  });

  // Create two unresolved promises in non-silent REPL mode.
  await Protocol.Runtime.evaluate({
    expression: `
      let loudReject1, loudReject2;
      new Promise((r, rej) => { loudReject1 = rej; });
      new Promise((r, rej) => { loudReject2 = rej; });`,
    replMode: true,
  });

  // Resolve the first loud promise and expect a pause.
  await Protocol.Runtime.evaluate({
    expression: 'loudReject1(\'Rejecting loud promise 1\')',
    replMode: true,
  });

  // Resolve the silent promise and expect no pause.
  await Protocol.Runtime.evaluate({
    expression: 'silentReject(\'Rejecting silent promise\')',
    replMode: true,
  });
  InspectorTest.logMessage(await evalPromise);

  // Resolve the loud promise and expect a pause.
  await Protocol.Runtime.evaluate({
    expression: 'loudReject2(\'Rejecting loud promise 2\')',
    replMode: true,
  });

  InspectorTest.completeTest();
})();
