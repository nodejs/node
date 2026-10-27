// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests step into on throw in OSR function');

contextGroup.addScript(
    `function caught_target() {
  eval("");
}

function thrower() {
  eval("");
  debugger;
  throw new Error("test");
}

%NeverOptimizeFunction(thrower);
%NeverOptimizeFunction(caught_target);

function osr_caller() {
  for (let i = 0; i < 20; i++) {
    if (i === 10) {
      %OptimizeOsr();
    }
  }
  try {
    thrower();
  } catch (e) {
    caught_target();
  }
}
%PrepareFunctionForOptimization(osr_caller);
`,
    0, 0, 'test.js');

Protocol.Debugger.enable();

Protocol.Debugger.onPaused(handlePaused);
Protocol.Runtime.evaluate({expression: 'osr_caller();'});

function handlePaused(message) {
  const callFrames = message.params.callFrames;
  const fnName = callFrames[0].functionName;
  InspectorTest.log('Paused in: ' + fnName);
  if (fnName !== 'caught_target') {
    Protocol.Debugger.stepInto();
  } else {
    Protocol.Debugger.resume();
    InspectorTest.completeTest();
  }
}
