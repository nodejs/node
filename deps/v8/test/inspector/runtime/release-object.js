// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {Protocol} = InspectorTest.start(
  `Tests that Runtime can properly release objects and object groups.`);

(async function test(){
  await Protocol.Runtime.enable();
  InspectorTest.runAsyncTestSuite([
    async function testReleaseObject() {
      await logAndEvaluate('var a = {x:3};');
      await logAndEvaluate('var b = {x:4};');
      const ids = [];
      let result = await Protocol.Runtime.evaluate({ expression: 'a' });
      const id1 = result.result.result.objectId;
      ids.push({id: id1, name: 'a'});
      result = await Protocol.Runtime.evaluate({ expression: 'b' });
      const id2 = result.result.result.objectId;
      ids.push({id: id2, name: 'b'});

      // Call Function on both objects and log:
      await objectGroupHelper(ids);
      InspectorTest.log('Release "a"');
      Protocol.Runtime.releaseObject({ objectId: id1 });
      await objectGroupHelper(ids);
      InspectorTest.log('Release "b"');
      Protocol.Runtime.releaseObject({ objectId: id2 });
      await objectGroupHelper(ids);
    },
    async function testReleaseObjectInvalid() {
      const releaseObjectResult = await Protocol.Runtime.releaseObject({});
      InspectorTest.log('ReleaseObject with invalid params.');
      InspectorTest.logMessage(releaseObjectResult);
    },
    async function testObjectGroups() {
      await logAndEvaluate('var a = {x:3};');
      await logAndEvaluate('var b = {x:4};');
      const ids = [];
      InspectorTest.log('Evaluate "a" in objectGroup "x"');
      let result = await Protocol.Runtime.evaluate({ expression: 'a', objectGroup: 'x' });
      const id1 = result.result.result.objectId;
      ids.push({id: id1, name: 'a'});
      InspectorTest.log('Evaluate "b" in objectGroup "y"');
      result = await Protocol.Runtime.evaluate({ expression: 'b', objectGroup: 'y' });
      const id2 = result.result.result.objectId;
      ids.push({id: id2, name: 'b'});

      // Call Function on both objects and log:
      await objectGroupHelper(ids);
      InspectorTest.log('Release objectGroup "x"');
      Protocol.Runtime.releaseObjectGroup({ objectGroup: 'x' });
      await objectGroupHelper(ids);
      InspectorTest.log('Release objectGroup "y"');
      Protocol.Runtime.releaseObjectGroup({ objectGroup: 'y' });
      await objectGroupHelper(ids);
    },
    async function testReleaseObjectGroupInvalid() {
      const releaseObjectGroupResult = await Protocol.Runtime.releaseObjectGroup({});
      InspectorTest.log('ReleaseObjectGroup with invalid params');
      InspectorTest.logMessage(releaseObjectGroupResult);
    }
  ]);

  // Helper to log and evaluate an expression
  async function logAndEvaluate(expression) {
    InspectorTest.logMessage(`Evaluating '${expression}'`);
    await Protocol.Runtime.evaluate({ expression });
  }

  // Helper function that calls a function on all objects with ids in objectIds, then returns
  async function objectGroupHelper(objectIds) {
      for (const {id , name } of objectIds) {
        InspectorTest.log(`Evaluate 'this' for object ${name}`);
        const result = await Protocol.Runtime.callFunctionOn({ objectId: id, functionDeclaration: 'function(){ return this;}' });
        InspectorTest.logMessage(result);
      }
  }
})();
