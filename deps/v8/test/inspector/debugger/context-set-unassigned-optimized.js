// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that Debugger.evaluateOnCallFrame and Debugger.setVariableValue ' +
    'on kNotAssigned context slots deoptimize optimized closures.');

contextGroup.addScript(
    `
const scriptConst = 10;
function readScriptConst() {
  return scriptConst;
}

function makeScope(initial) {
  let x = initial;
  function read() {
    return x;
  }
  function pauseInScope() {
    debugger;
  }
  return {read, pauseInScope};
}

const evalScope = makeScope(1);
const setVarScope = makeScope(2);

function optimize(fn) {
  %PrepareFunctionForOptimization(fn);
  fn();
  fn();
  %OptimizeFunctionOnNextCall(fn);
  return fn();
}
`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  // 1. Debugger.evaluateOnCallFrame mutating a kNotAssigned closure variable.
  let res = await Protocol.Runtime.evaluate(
      {expression: 'optimize(evalScope.read)', returnByValue: true});
  InspectorTest.log('evalScope.read before: ' + res.result.result.value);

  let pausedPromise = Protocol.Debugger.oncePaused();
  let runPromise =
      Protocol.Runtime.evaluate({expression: 'evalScope.pauseInScope()'});
  let {params: {callFrames: [topFrame]}} = await pausedPromise;

  await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: topFrame.callFrameId,
    expression: 'x = 100',
  });
  await Protocol.Debugger.resume();
  await runPromise;

  res = await Protocol.Runtime.evaluate(
      {expression: 'evalScope.read()', returnByValue: true});
  InspectorTest.log(
      'evalScope.read after evaluateOnCallFrame: ' + res.result.result.value);

  // Re-optimize after kNotAssigned was invalidated and mutate again.
  res = await Protocol.Runtime.evaluate(
      {expression: 'optimize(evalScope.read)', returnByValue: true});
  InspectorTest.log('evalScope.read re-optimized: ' + res.result.result.value);

  pausedPromise = Protocol.Debugger.oncePaused();
  runPromise =
      Protocol.Runtime.evaluate({expression: 'evalScope.pauseInScope()'});
  ({params: {callFrames: [topFrame]}} = await pausedPromise);

  await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: topFrame.callFrameId,
    expression: 'x = 200',
  });
  await Protocol.Debugger.resume();
  await runPromise;

  res = await Protocol.Runtime.evaluate(
      {expression: 'evalScope.read()', returnByValue: true});
  InspectorTest.log(
      'evalScope.read after second evaluateOnCallFrame: ' +
      res.result.result.value);

  // 2. Debugger.setVariableValue mutating kNotAssigned closure and script
  // variables.
  res = await Protocol.Runtime.evaluate(
      {expression: 'optimize(setVarScope.read)', returnByValue: true});
  InspectorTest.log('setVarScope.read before: ' + res.result.result.value);
  res = await Protocol.Runtime.evaluate(
      {expression: 'optimize(readScriptConst)', returnByValue: true});
  InspectorTest.log('readScriptConst before: ' + res.result.result.value);

  pausedPromise = Protocol.Debugger.oncePaused();
  runPromise =
      Protocol.Runtime.evaluate({expression: 'setVarScope.pauseInScope()'});
  ({params: {callFrames: [topFrame]}} = await pausedPromise);

  const closureScopeIndex =
      topFrame.scopeChain.findIndex(s => s.type === 'closure');
  await Protocol.Debugger.setVariableValue({
    scopeNumber: closureScopeIndex,
    variableName: 'x',
    newValue: {value: 300},
    callFrameId: topFrame.callFrameId,
  });

  const scriptScopeIndex =
      topFrame.scopeChain.findIndex(s => s.type === 'script');
  await Protocol.Debugger.setVariableValue({
    scopeNumber: scriptScopeIndex,
    variableName: 'scriptConst',
    newValue: {value: 400},
    callFrameId: topFrame.callFrameId,
  });

  await Protocol.Debugger.resume();
  await runPromise;

  res = await Protocol.Runtime.evaluate(
      {expression: 'setVarScope.read()', returnByValue: true});
  InspectorTest.log(
      'setVarScope.read after setVariableValue: ' + res.result.result.value);
  res = await Protocol.Runtime.evaluate(
      {expression: 'readScriptConst()', returnByValue: true});
  InspectorTest.log(
      'readScriptConst after setVariableValue: ' + res.result.result.value);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
