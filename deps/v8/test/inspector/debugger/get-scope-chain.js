// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.getScopeChain');

contextGroup.addInlineScript(
    `
function outer() {
  let outerLocal = 'outer';
  const result = inner('param');
  return [outerLocal, ...result];
}

function inner(param) {
  let stackLocal = 'stack';
  let contextLocal = 'context';
  const getContextLocal = () => contextLocal;
  {
    let blockLocal = 'block';
    debugger;
    return [param, stackLocal, getContextLocal(), blockLocal];
  }
}

function withEval() {
  eval('var evalLocal = "eval"');
  debugger;
  return typeof evalLocal;
}`,
    'test.js');

async function pause(expression = 'outer()') {
  const resultPromise =
      Protocol.Runtime.evaluate({expression, returnByValue: true});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  return {callFrames, resultPromise};
}

async function resume(resultPromise) {
  await Protocol.Debugger.resume();
  const {result: {result: {value}}} = await resultPromise;
  InspectorTest.log(`Result: ${JSON.stringify(value)}`);
}

async function getScopeChain({callFrameId}) {
  const {result: {scopeChain}} =
      await Protocol.Debugger.getScopeChain({callFrameId});
  return scopeChain;
}

async function logScopeChain(scopeChain) {
  for (const scope of scopeChain) {
    if (scope.type === 'global') {
      InspectorTest.log('  global');
      continue;
    }
    const {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    const variables = result.map(
        ({name, value}) => `${name}: ${
            value ? value.description ?? value.value : '<unavailable>'}`);
    InspectorTest.log(`  ${scope.type}: ${variables.join(', ')}`);
  }
}

// Serializes everything but the ids of the scope objects, which differ every
// time a scope chain is reported.
function serializeWithoutObjectIds(scopeChain) {
  return JSON.stringify(
      scopeChain, (key, value) => key === 'objectId' ? undefined : value);
}

Protocol.Debugger.enable();

InspectorTest.runAsyncTestSuite([
  async function testMatchesScopeChainOfPausedEvent() {
    const {callFrames, resultPromise} = await pause();
    for (const callFrame of callFrames) {
      const scopeChain = await getScopeChain(callFrame);
      InspectorTest.log(
          `Scope chain of ${callFrame.functionName || '(anonymous)'}:`);
      await logScopeChain(scopeChain);
      const matches = serializeWithoutObjectIds(scopeChain) ===
          serializeWithoutObjectIds(callFrame.scopeChain);
      InspectorTest.log(`  Matches Debugger.paused: ${matches}`);
    }

    await resume(resultPromise);
  },

  async function testEvaluateOnCallFrame() {
    const {callFrames, resultPromise} = await pause();
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'param = 1; stackLocal = 2; contextLocal = 3; blockLocal = 4;'
    });
    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[1].callFrameId,
      expression: 'outerLocal = "changed";'
    });
    InspectorTest.log('Scope chain of inner after evaluateOnCallFrame:');
    await logScopeChain(await getScopeChain(callFrames[0]));
    InspectorTest.log('Scope chain of outer after evaluateOnCallFrame:');
    await logScopeChain(await getScopeChain(callFrames[1]));

    await resume(resultPromise);
  },

  async function testSetVariableValue() {
    const {callFrames, resultPromise} = await pause();
    const [{callFrameId, scopeChain}] = callFrames;
    await Protocol.Debugger.setVariableValue({
      callFrameId,
      scopeNumber: scopeChain.findIndex(scope => scope.type === 'block'),
      variableName: 'blockLocal',
      newValue: {value: 'changed'}
    });
    await Protocol.Debugger.setVariableValue({
      callFrameId,
      scopeNumber: scopeChain.findIndex(scope => scope.type === 'local'),
      variableName: 'stackLocal',
      newValue: {value: 'changed'}
    });
    InspectorTest.log('Scope chain of inner after setVariableValue:');
    await logScopeChain(await getScopeChain(callFrames[0]));

    await resume(resultPromise);
  },

  async function testRemovedVariable() {
    const {callFrames, resultPromise} = await pause('withEval()');
    InspectorTest.log('Scope chain of withEval:');
    await logScopeChain(await getScopeChain(callFrames[0]));

    await Protocol.Debugger.evaluateOnCallFrame({
      callFrameId: callFrames[0].callFrameId,
      expression: 'delete evalLocal;'
    });
    InspectorTest.log('Scope chain of withEval after evaluateOnCallFrame:');
    await logScopeChain(await getScopeChain(callFrames[0]));

    await resume(resultPromise);
  },

  async function testScopeObjectsReleasedOnResume() {
    const {callFrames, resultPromise} = await pause();
    const scopeChain = await getScopeChain(callFrames[0]);
    const {objectId} = scopeChain.find(scope => scope.type === 'local').object;
    const {error: errorWhilePaused} =
        await Protocol.Runtime.getProperties({objectId});
    InspectorTest.log(`Scope object is alive while paused: ${
        errorWhilePaused === undefined}`);

    await resume(resultPromise);
    const {error} = await Protocol.Runtime.getProperties({objectId});
    InspectorTest.log(`Runtime.getProperties after resume: ${error.message}`);
  },

  async function testErrors() {
    const {callFrames, resultPromise} = await pause();
    const [{callFrameId}] = callFrames;
    InspectorTest.log('Missing call frame id:');
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(
        await Protocol.Debugger.getScopeChain({})));
    InspectorTest.log('Malformed call frame id:');
    InspectorTest.logMessage(
        await Protocol.Debugger.getScopeChain({callFrameId: 'invalid'}));
    InspectorTest.log('Call frame id of a non-existent frame:');
    InspectorTest.logMessage(await Protocol.Debugger.getScopeChain(
        {callFrameId: callFrameId.replace(/\d+$/, '1000')}));

    await resume(resultPromise);
    InspectorTest.log('Not paused:');
    InspectorTest.logMessage(
        await Protocol.Debugger.getScopeChain({callFrameId}));
  },

  async function testChangedScopeChain() {
    const {callFrames, resultPromise} = await pause();
    // Declaring the first variable of the script scope adds that scope to the
    // scope chain. Note that the variable stays around after this test.
    await Protocol.Runtime.evaluate({expression: 'let scriptLet = "script";'});
    InspectorTest.log('Scope chain of inner after declaring scriptLet:');
    await logScopeChain(await getScopeChain(callFrames[0]));

    await resume(resultPromise);
  },
]);
