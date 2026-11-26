// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests changing local variables in OSR frame via ' +
    'Debugger.setVariableValue.');

contextGroup.addScript(`
function g() { debugger; }
%NeverOptimizeFunction(g);

function ChangeSmiConstantAndOsr() {
  var j = 1;
  for (var i = 0; i < 4; i++) {
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeSmiConstantAndOsr);

function ChangeFloatConstantAndOsr() {
  var j = 0.1;
  for (var i = 0; i < 4; i++) {
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeFloatConstantAndOsr);

function ChangeFloatVarAndOsr() {
  var j = 0.1;
  for (var i = 0; i < 4; i++) {
    j = j + 0.1;
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeFloatVarAndOsr);

function ChangeIntVarAndOsr() {
  var j = 1;
  for (var i = 0; i < 4; i++) {
    j = j + 1|0;
    if (i == 2) {
      %OptimizeOsr();
      g();
    }
  }
  return j;
}
%PrepareFunctionForOptimization(ChangeIntVarAndOsr);
`);

(async function test() {
  await Protocol.Debugger.enable();

  let newVal = 'result';
  let changed = false;

  Protocol.Debugger.onPaused(async ({params}) => {
    const response = await Protocol.Debugger.setVariableValue({
      scopeNumber: 0,
      variableName: 'j',
      newValue: {value: newVal},
      callFrameId: params.callFrames[1].callFrameId,
    });
    changed = !response.error;
    Protocol.Debugger.resume();
  });

  const {result: {result: r1}} = await Protocol.Runtime.evaluate({
    expression: 'ChangeSmiConstantAndOsr()',
  });
  InspectorTest.log(`r1 (changed=${changed}): ${r1.value}`);

  const {result: {result: r2}} = await Protocol.Runtime.evaluate({
    expression: 'ChangeFloatConstantAndOsr()',
  });
  InspectorTest.log(`r2 (changed=${changed}): ${r2.value}`);

  const {result: {result: r3}} = await Protocol.Runtime.evaluate({
    expression: 'ChangeFloatVarAndOsr()',
  });
  InspectorTest.log(`r3 (changed=${changed}): ${r3.value}`);

  newVal = 100;
  const {result: {result: r4}} = await Protocol.Runtime.evaluate({
    expression: 'ChangeIntVarAndOsr()',
  });
  InspectorTest.log(`r4 (changed=${changed}): ${r4.value}`);

  InspectorTest.completeTest();
})();
