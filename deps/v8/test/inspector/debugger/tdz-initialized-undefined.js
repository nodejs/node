// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that initialized undefined locals are not falsely reported in TDZ');

const testScript = `
function testSwitchInitialized() {
  switch (1) {
    case 1:
      let x = undefined;
      debugger;
      return x;
  }
}

function testSwitchUninitialized() {
  switch (0) {
    case 1:
      let x = 1;
      return x;
    default:
      debugger;
  }
}

function testDestructuringArray() {
  let [a, b = (() => { debugger; return 2; })()] = [undefined];
  debugger;
  return a + b;
}

function testDestructuringArrayElidedTdz() {
  let [a, b] = (() => { debugger; return [undefined, 2]; })();
  return a;
}
`;

contextGroup.addScript(testScript);

InspectorTest.runAsyncTestSuite([
  // Case 1: Switch scope, initialized undefined local.
  // Mutating ('42') via setVariableValue succeeds, and subsequent evaluation
  // observes the mutated value '42'.
  async function testSwitchScopeInitializedUndefined() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testSwitchInitialized()',
    });
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    const frameId = callFrames[0].callFrameId;

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber: 0,
      variableName: 'x',
      newValue: {value: 42},
      callFrameId: frameId,
    });
    InspectorTest.log('setVariableValue on x result:');
    InspectorTest.logMessage(setResult);

    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'x',
      callFrameId: frameId,
    });
    InspectorTest.log('evaluateOnCallFrame x:');
    InspectorTest.logMessage(evalResult.result.result);

    await Protocol.Debugger.resume();
    const finalResult = await evalPromise;
    InspectorTest.log(
        'testSwitchInitialized return value: ' +
        finalResult.result.result.value);

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  // Case 1 (continued): Switch scope, uninitialized local (holding TdzHole).
  // Mutating via setVariableValue is rejected (fails with TDZ error).
  async function testSwitchScopeUninitialized() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testSwitchUninitialized()',
    });
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    const frameId = callFrames[0].callFrameId;

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber: 0,
      variableName: 'x',
      newValue: {value: 42},
      callFrameId: frameId,
    });
    InspectorTest.log('setVariableValue on uninitialized switch var error:');
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(setResult));

    await Protocol.Debugger.resume();
    await evalPromise;

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  // Case 2: Multi-binding destructuring declaration.
  // - Mid-declaration (inside b's default): mutating 'a' via setVariableValue
  //   succeeds (40).
  // - Post-declaration (at debugger; after declaration): reading 'a' succeeds
  //   and observes the mutated value 40; mutating 'a' to 50 succeeds.
  // - Resumes and returns 52 (50 + 2).
  async function testDestructuringDeclaration() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testDestructuringArray()',
    });

    // 1st pause: inside b's default initializer
    {
      const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
      // callFrames[0] is inner arrow function, callFrames[1] is
      // testDestructuringArray.
      const frameId = callFrames[1].callFrameId;

      const setResult = await Protocol.Debugger.setVariableValue({
        scopeNumber: 0,
        variableName: 'a',
        newValue: {value: 40},
        callFrameId: frameId,
      });
      InspectorTest.log('mid-declaration setVariableValue on a result:');
      InspectorTest.logMessage(setResult);

      await Protocol.Debugger.resume();
    }

    // 2nd pause: after destructuring declaration finishes
    {
      const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
      const frameId = callFrames[0].callFrameId;

      const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
        expression: 'a',
        callFrameId: frameId,
      });
      InspectorTest.log('post-declaration evaluateOnCallFrame a:');
      InspectorTest.logMessage(evalResult.result.result);

      const setResult = await Protocol.Debugger.setVariableValue({
        scopeNumber: 0,
        variableName: 'a',
        newValue: {value: 50},
        callFrameId: frameId,
      });
      InspectorTest.log('post-declaration setVariableValue on a result:');
      InspectorTest.logMessage(setResult);

      await Protocol.Debugger.resume();
    }

    const finalResult = await evalPromise;
    InspectorTest.log(
        'testDestructuringArray return value: ' +
        finalResult.result.result.value);

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },

  // Case 3: Elided-TDZ lexical local before its initializer.
  // - setVariableValue on a succeeds (writes 99 to dead register, WAI).
  // - evaluateOnCallFrame on a sees 99 (WAI before initializer runs!).
  // - Upon resume, initializer runs and overwrites register with undefined.
  async function testDestructuringElidedTdz() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();

    const evalPromise = Protocol.Runtime.evaluate({
      expression: 'testDestructuringArrayElidedTdz()',
    });
    const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
    // callFrames[0] is inner arrow function, callFrames[1] is
    // testDestructuringArrayElidedTdz.
    const frameId = callFrames[1].callFrameId;

    const setResult = await Protocol.Debugger.setVariableValue({
      scopeNumber: 0,
      variableName: 'a',
      newValue: {value: 99},
      callFrameId: frameId,
    });
    InspectorTest.log('elided-TDZ setVariableValue on a result:');
    InspectorTest.logMessage(setResult);

    const evalResult = await Protocol.Debugger.evaluateOnCallFrame({
      expression: 'a',
      callFrameId: frameId,
    });
    InspectorTest.log('elided-TDZ evaluateOnCallFrame a (WAI):');
    InspectorTest.logMessage(evalResult.result.result);

    await Protocol.Debugger.resume();
    const finalResult = await evalPromise;
    InspectorTest.log(
        'testDestructuringArrayElidedTdz return value: ' +
        finalResult.result.result.value);

    await Protocol.Debugger.disable();
    await Protocol.Runtime.disable();
  },
]);
