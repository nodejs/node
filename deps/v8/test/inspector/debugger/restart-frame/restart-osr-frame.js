// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} =
  InspectorTest.start('Checks that restarting an OSR-optimized frame works.');

session.setupScriptMap();

contextGroup.addScript(`
function foo() {
  debugger;
}

function osr_caller() {
  for (let i = 0; i < 1000; i++) {
    if (i == 10) %OptimizeOsr();
  }
  foo();
}
`, 0, 0, 'test.js');

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  const { callFrames } = await InspectorTest.evaluateAndWaitForPause(
      '%PrepareFunctionForOptimization(osr_caller); osr_caller();');

  InspectorTest.log('Restarting osr_caller frame...');
  await InspectorTest.restartFrameAndWaitForPause(callFrames, 1);

  InspectorTest.log('Resuming...');
  Protocol.Debugger.resume();
  await Protocol.Debugger.oncePaused();
  await Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();
