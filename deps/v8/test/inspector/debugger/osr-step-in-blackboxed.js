// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping with blackboxing and inlining across OSR');

contextGroup.addScript(
    `function target() {
  return 1;
}

function callee() {
  eval("");
  debugger;
}
%NeverOptimizeFunction(callee);

// Gather feedback for inlining
%PrepareFunctionForOptimization(target);
callee();
target();
callee();
target();
`,
    0, 0, 'target.js');

contextGroup.addScript(
    `function osr_caller(osr) {
  for (let i = 0; i < 20; i++) {
    if (osr && i === 10) %OptimizeOsr();
  }
  callee();
  target();
}
%PrepareFunctionForOptimization(osr_caller);
osr_caller(false);
osr_caller(false);
`,
    0, 0, 'osr_caller.js');

Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({patterns: ['osr_caller\\.js']});

Protocol.Debugger.onPaused(handlePaused);
Protocol.Runtime.evaluate({expression: 'osr_caller(true);'});

function handlePaused(message) {
  const callFrames = message.params.callFrames;
  const fnName = callFrames[0].functionName;
  InspectorTest.log('Paused in: ' + fnName);
  if (fnName === 'callee') {
    Protocol.Debugger.stepInto();
  } else {
    Protocol.Debugger.resume();
    InspectorTest.log('Target was paused: ' + (fnName === 'target'));
    InspectorTest.completeTest();
  }
}
