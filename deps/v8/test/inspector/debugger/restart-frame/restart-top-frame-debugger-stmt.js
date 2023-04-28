// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} =
    InspectorTest.start('Checks that restarting the top frame hits a debugger statement twice');

session.setupScriptMap();

const source = `
function foo() {
  const x = 1;
  debugger;
  const y = 2;
}
foo();
`;

(async () => {
  await Protocol.Debugger.enable();

  const { callFrames } = await InspectorTest.evaluateAndWaitForPause(source);

  await InspectorTest.restartFrameAndWaitForPause(callFrames, 0);

  Protocol.Debugger.resume();  // Resuming hits the 'debugger' stmt again.
  await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();
