// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
    `Tests that exercise various result types from Debugger.evaluateOnCallFrame`);

(async function test(){
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  Protocol.Runtime.evaluate({expression: "debugger;"});
  const {params:{callFrames:[{callFrameId}]}} = await Protocol.Debugger.oncePaused();
  InspectorTest.runAsyncTestSuite([
    async function testCreateFunction() {
      await evalAndLog('function testFunc() {return "SUCCESS";}; testFunc();', callFrameId, /*returnByValue*/ true );
    },
    async function testNumericValue() {
      await evalAndLog('-578.28', callFrameId);
    },
    async function testUnserializableValues() {
      const unserializableExpressions = ['NaN', 'Infinity', '-Infinity', '-0'];
      for (const expression of unserializableExpressions)
        await evalAndLog(expression, callFrameId);
    },
    async function testBooleanValue() {
      await evalAndLog('Infinity > 0', callFrameId);
    },
    async function testObject() {
      await evalAndLog('({ })', callFrameId);
    },
    async function testConsoleLog() {
      Protocol.Debugger.evaluateOnCallFrame({ expression: `console.log(42)`, callFrameId });
      const result = await Protocol.Runtime.onceConsoleAPICalled();
      InspectorTest.logMessage(result);
    },
    async function testSymbol() {
      await evalAndLog(`const symbolTest = Symbol('foo'); symbolTest;`, callFrameId);
    },
    async function testSymbolReturnByValueError() {
      await evalAndLog(`const symbolTest = Symbol('foo'); symbolTest;`, callFrameId, /*returnByValue*/ true);
    },
    async function testPromiseResolveReturnByVal() {
      await evalAndLog('Promise.resolve(239)', callFrameId, /*returnByValue*/ true);
    },
    async function testPromiseResolve() {
      await evalAndLog('Promise.resolve(239)', callFrameId);
    },
    async function testReleaseObject() {
      await Protocol.Runtime.evaluate({ expression: 'var a = {x:3};', callFrameId });
      await Protocol.Runtime.evaluate({ expression: 'var b = {x:4};', callFrameId });
      const ids = [];
      let result = await Protocol.Runtime.evaluate({ expression: 'a', callFrameId });
      const id1 = result.result.result.objectId;
      ids.push(id1);
      result = await Protocol.Runtime.evaluate({ expression: 'b', callFrameId });
      const id2 = result.result.result.objectId;
      ids.push(id2);

      // Call Function on both objects and log:
      await objectGroupHelper(ids);
      Protocol.Runtime.releaseObject({ objectId: id1 });
      await objectGroupHelper(ids);
      Protocol.Runtime.releaseObject({ objectId: id2 });
      await objectGroupHelper(ids);
    },
    async function testReleaseObjectInvalid() {
      const releaseObjectResult = await Protocol.Runtime.releaseObject({});
      InspectorTest.log('ReleaseObject with invalid params.');
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(releaseObjectResult));
    },
    async function testObjectGroups() {
      await Protocol.Runtime.evaluate({ expression: 'var a = {x:3};', callFrameId });
      await Protocol.Runtime.evaluate({ expression: 'var b = {x:4};', callFrameId });
      const ids = [];
      let result = await Protocol.Runtime.evaluate({ expression: 'a', objectGroup: 'a', callFrameId });
      const id1 = result.result.result.objectId;
      ids.push(id1);
      result = await Protocol.Runtime.evaluate({ expression: 'b', objectGroup: 'b', callFrameId });
      const id2 = result.result.result.objectId;
      ids.push(id2);

      // Call Function on both objects and log:
      await objectGroupHelper(ids);
      Protocol.Runtime.releaseObjectGroup({ objectGroup: 'a' });
      await objectGroupHelper(ids);
      Protocol.Runtime.releaseObjectGroup({ objectGroup: 'b' });
      await objectGroupHelper(ids);
    },
    async function testReleaseObjectGroupInvalid() {
      const releaseObjectGroupResult = await Protocol.Runtime.releaseObjectGroup({});
      InspectorTest.log('ReleaseObjectGroup with invalid params');
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(releaseObjectGroupResult));
    },
    async function testEvaluateSyntaxError() {
      const result = await Protocol.Debugger.evaluateOnCallFrame({ expression: `[]]`, callFrameId });
      InspectorTest.logMessage(result.result.exceptionDetails.exception);
    },
    async function testEvaluateReferenceError() {
      const result = await Protocol.Debugger.evaluateOnCallFrame({ expression: `totalRandomNotRealVariable789`, callFrameId });
      InspectorTest.logMessage(result.result.exceptionDetails.exception);
    },
    async function testCallFrameIdTypeError() {
      const result = await Protocol.Debugger.evaluateOnCallFrame({ expression: `console.log(42)`, callFrameId: {} });
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
    },
    async function testCallFrameIdInvalidInput() {
      InspectorTest.log('Testing evaluateOnCallFrame with non-existent callFrameId');
      const result = await Protocol.Debugger.evaluateOnCallFrame({ expression: `console.log(42)`, callFrameId: '1234' });
      InspectorTest.logMessage(result);
    },
    async function testNullExpression() {
      await evalAndLog(null, callFrameId, /*returnByValue*/ true);
    }
  ]);

  async function evalAndLog(expression, callFrameId, returnByValue) {
    const result = await Protocol.Debugger.evaluateOnCallFrame({ expression, callFrameId, returnByValue });
    InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
  }

  // Helper function that calls a function on all objects with ids in objectIds, then returns
  async function objectGroupHelper(objectIds) {
    return new Promise(async resolve => {
      for (let objectId of objectIds) {
        const result = await Protocol.Runtime.callFunctionOn({ objectId, functionDeclaration: 'function(){ return this;}' });
        InspectorTest.logMessage(result);
      }
      resolve();
    });
  }
})();
