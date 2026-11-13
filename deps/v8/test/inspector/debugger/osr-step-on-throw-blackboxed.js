// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-maglev

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests step on throw across blackboxed OSR caller');

contextGroup.addScript(
    `function target() {
  return 1;
}

function thrower() {
  eval("");
  debugger;
  throw new Error("test");
}
%NeverOptimizeFunction(thrower);

// Gather feedback for inlining
%PrepareFunctionForOptimization(target);
try { thrower(); } catch(e) { target(); }
try { thrower(); } catch(e) { target(); }
`,
    0, 0, 'target.js');

contextGroup.addScript(
    `function osr_caller(osr) {
  for (let i = 0; i < 20; i++) {
    if (osr && i === 10) %OptimizeOsr();
  }
  try { thrower(); } catch (e) { target(); }
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
  if (fnName === 'thrower') {
    Protocol.Debugger.stepInto();
  } else {
    Protocol.Debugger.resume();
    InspectorTest.log('Target was paused: ' + (fnName === 'target'));
    InspectorTest.completeTest();
  }
}
