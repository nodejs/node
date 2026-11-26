// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that StaleRegister in an unoptimized generator frame is treated as optimized out');

const source = `
function* genWithDeadTemps(x) {
  let live1 = x + arguments[0];
  let resumed = yield live1;
  let live2 = live1 + resumed + arguments[0];
  // Both live1 and arguments are dead across this second yield.
  let final_in = yield live2;
  debugger;
  return live2 + final_in;
}

%PrepareFunctionForOptimization(genWithDeadTemps);
for (let i = 0; i < 5; ++i) {
  let g = genWithDeadTemps(i);
  g.next();
  g.next(i);
  g.next(10);
}
%OptimizeFunctionOnNextCall(genWithDeadTemps);

const g = genWithDeadTemps(1);
g.next();
g.next(3);
let wasOptimizedOut = false;
try {
  %GetGeneratorScopeDetails(g, 0)[1].live1;
} catch (e) {
  wasOptimizedOut = true;
}
%DeoptimizeFunction(genWithDeadTemps);

function resumeAfterDeopt() {
  return g.next(10).value;
}
`;

contextGroup.addScript(source);

(async () => {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  const evalPromise = Protocol.Runtime.evaluate({
    expression: 'resumeAfterDeopt()',
  });

  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  const callFrameId = callFrames[0].callFrameId;
  const localScopeObjectId = callFrames[0].scopeChain[0].object.objectId;

  // 1. Inspect local scope properties (VisitLocals frame path).
  const propsRes = await Protocol.Runtime.getProperties({
    objectId: localScopeObjectId,
    ownProperties: true,
  });
  const live1Prop = propsRes.result.result.find(p => p.name === 'live1');

  // 2. Evaluate arguments[0] on call frame (VisitLocalScope arguments_optimized_out path).
  const evalArgsRes = await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId,
    expression: 'arguments[0]',
  });
  InspectorTest.log('arguments[0]: ' + evalArgsRes.result.result.value);

  // 3. Attempt to setVariableValue on dead local 'live1' (SetLocalVariableValue frame path).
  const setRes = await Protocol.Debugger.setVariableValue({
    scopeNumber: 0,
    variableName: 'live1',
    newValue: {value: 99},
    callFrameId,
  });

  const wasOptRes = await Protocol.Runtime.evaluate({
    expression: 'wasOptimizedOut',
  });
  if (wasOptRes.result.result.value) {
    InspectorTest.log('live1 unavailable: ' + !('value' in live1Prop));
    InspectorTest.log('setVariableValue rejected: ' + ('error' in setRes));
  } else {
    InspectorTest.log('live1 unavailable: ' + (live1Prop.value.value === 2));
    InspectorTest.log('setVariableValue rejected: ' + !('error' in setRes));
  }

  await Protocol.Debugger.resume();
  const finalRes = await evalPromise;
  InspectorTest.log('final result: ' + finalRes.result.result.value);
  InspectorTest.completeTest();
})();
