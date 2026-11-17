// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests the scope chain at the return position of a function that ' +
    'returns from inside a block context while its own scope has no context.');

contextGroup.addScript(`
with ({withProp: 1}) {
  function returnInsideBlockInWith() {
    {
      let x = 1;
      (() => x)();
      debugger;
      return 42;
    }
  }
}

function returnInsideBlockInEval() {
  // The block in the outer eval starts at the same offset (14) as the block
  // inside the function in the inner eval.
  return eval(
      '              { let x = 5, a = 2, b = 3, c = 4; (() => x + a + b + c)(); ' +
      'eval("(function() { { let x = 1; (() => x)(); debugger; return 42; } })()"); }');
}
//# sourceURL=test.js`);

function describe(remoteObject) {
  return remoteObject.type === 'string' ? JSON.stringify(remoteObject.value) :
                                          remoteObject.description;
}

async function logScopeChain(callFrame) {
  for (let i = 0; i < callFrame.scopeChain.length; ++i) {
    const scope = callFrame.scopeChain[i];
    InspectorTest.log(`  [${i}] ${scope.type}`);
    if (scope.type === 'global') continue;
    const {result: {result: variables}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    for (const variable of variables) {
      const value =
          'value' in variable ? describe(variable.value) : '<value_unavailable>';
      InspectorTest.log(`      ${variable.name} : ${value}`);
    }
  }
}

async function logPause() {
  const {params: {callFrames: [callFrame]}} =
      await Protocol.Debugger.oncePaused();
  const {lineNumber, columnNumber} = callFrame.location;
  InspectorTest.log(`Paused in '${callFrame.functionName}' at ${lineNumber}:${
      columnNumber}`);
  await logScopeChain(callFrame);
  const {result: {result}} = await Protocol.Debugger.evaluateOnCallFrame(
      {callFrameId: callFrame.callFrameId, expression: 'x'});
  InspectorTest.log(`  x = ${describe(result)}`);
}

// Pauses at the `debugger` statement, then steps to the `return` statement
// and on to the return position of the function.
async function stepToReturnAndLogScopes(expression) {
  const evaluated = Protocol.Runtime.evaluate({expression});
  await logPause();
  for (let i = 0; i < 2; ++i) {
    Protocol.Debugger.stepOver();
    await logPause();
  }
  await Protocol.Debugger.resume();
  InspectorTest.log(`Result: ${describe((await evaluated).result.result)}`);
}

InspectorTest.runAsyncTestSuite([
  async function setUp() {
    await Protocol.Debugger.enable();
    await Protocol.Runtime.enable();
  },

  async function testReturnInsideBlockInWith() {
    await stepToReturnAndLogScopes('returnInsideBlockInWith()');
  },

  async function testReturnInsideBlockInDirectEval() {
    await stepToReturnAndLogScopes('returnInsideBlockInEval()');
  },
]);
