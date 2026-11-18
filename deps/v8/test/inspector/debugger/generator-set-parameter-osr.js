// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --maglev --maglev-osr

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that a generator parameter changed by the debugger in a live ' +
    'frame survives OSR and later suspends.');

contextGroup.addScript(
    `
let pause = false;

function* gen(a, do_osr) {
  yield a;
  if (pause) debugger;
  for (let i = 0; i < 10; i++) {
    if (do_osr && i == 5) %OptimizeOsr();
  }
  yield a;
  yield a;
}

function run() {
  %PrepareFunctionForOptimization(gen);
  // Warm up feedback so that the OSR code does not deopt before the suspend.
  for (let i = 0; i < 3; i++) [...gen(1, false)];
  pause = true;
  return [...gen(1, true)];
}
`,
    0, 0, 'test.js');

Protocol.Debugger.onPaused(async ({params: {callFrames: [{callFrameId}]}}) => {
  InspectorTest.log('Paused, setting a = 42');
  await Protocol.Debugger.setVariableValue(
      {scopeNumber: 0, variableName: 'a', newValue: {value: 42}, callFrameId});
  await Protocol.Debugger.resume();
});

(async () => {
  await Protocol.Debugger.enable();
  const {result: {result: {value}}} = await Protocol.Runtime.evaluate(
      {expression: 'run()', returnByValue: true});
  InspectorTest.log('Yielded: ' + JSON.stringify(value));
  InspectorTest.completeTest();
})();
