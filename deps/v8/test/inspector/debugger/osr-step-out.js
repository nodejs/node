// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping out of function called from OSR function');

contextGroup.addScript(
    `function bar() {
  // Should pause here when stepping out of foo() from osr_caller()
}
%NeverOptimizeFunction(bar);

function foo() {
  debugger;
}
%NeverOptimizeFunction(foo);

function osr_caller() {
  for (let i = 0; i < 20; i++) {
    if (i === 10) {
      %OptimizeOsr();
    }
  }
  foo();
  bar();
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
  if (fnName === 'foo') {
    Protocol.Debugger.stepOut();
  } else if (fnName === 'osr_caller') {
    Protocol.Debugger.stepInto();
  } else {
    Protocol.Debugger.resume();
    InspectorTest.completeTest();
  }
}
