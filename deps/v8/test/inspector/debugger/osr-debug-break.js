// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that OSR topmost frame is deoptimized after debugger break');

contextGroup.addScript(
    `
let top_frame_status_after_break = -1;
function check_deopt() {
  eval("");
  top_frame_status_after_break = %GetOptimizationStatus(osr_top);
}
%NeverOptimizeFunction(check_deopt);

function osr_top() {
  for (let i = 0; i < 20; i++) {
    if (i === 10) {
      %OptimizeOsr();
    }
  }
  debugger;
  check_deopt();
}

function run() {
  %PrepareFunctionForOptimization(osr_top);
  osr_top();
  // kTopmostFrameIsTurboFanned is bit 11 (1 << 11 = 2048) of
  // GetOptimizationStatus.
  const kTopmostFrameIsTurboFanned = 1 << 11;
  const isTopFrameTurboFanned =
      (top_frame_status_after_break & kTopmostFrameIsTurboFanned) !== 0;
  return isTopFrameTurboFanned;
}
`,
    0, 0, 'test.js');

Protocol.Debugger.onPaused(async message => {
  InspectorTest.log('Paused on debugger statement');
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Debugger.enable();
  const result = await Protocol.Runtime.evaluate(
      {expression: 'run()', returnByValue: true});
  InspectorTest.log('isTopFrameTurboFanned: ' + result.result.result.value);
  InspectorTest.completeTest();
})();
