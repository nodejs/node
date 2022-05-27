// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} =
    InspectorTest.start('Checks that requesting to restart a non-existant frame fails cleanly');

session.setupScriptMap();

(async () => {
  await Protocol.Debugger.enable();

  await InspectorTest.evaluateAndWaitForPause(`
    (function foo() { debugger; })();
  `);

  InspectorTest.log('Attempting to restart frame with non-existent index 2');
  InspectorTest.logMessage(
    await Protocol.Debugger.restartFrame({ callFrameId: '1.1.2', mode: 'StepInto' }));
  InspectorTest.completeTest();
})();
