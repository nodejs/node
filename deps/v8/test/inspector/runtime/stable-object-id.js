// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that protocol returns the same RemoteObjectId for the same object');

InspectorTest.runAsyncTestSuite([
  async function testGlobal() {
    const {result:{result:{objectId:firstId}}} =
        await Protocol.Runtime.evaluate({expression: 'this'});
    const firstStableId = await stableObjectId(firstId);
    const {result:{result:{objectId:secondId}}} =
        await Protocol.Runtime.evaluate({expression: 'this'});
    const secondStableId = await stableObjectId(secondId);
    InspectorTest.log(
        `Compare global evaluated twice: ${firstStableId === secondStableId}`);
  },

  async function testObject() {
    const {result:{result:{objectId:firstId}}} =
        await Protocol.Runtime.evaluate({expression: 'this.a = {}, this.a'});
    const firstStableId = await stableObjectId(firstId);
    const {result:{result:{objectId:secondId}}} =
        await Protocol.Runtime.evaluate({expression: 'this.a'});
    const secondStableId = await stableObjectId(secondId);
    InspectorTest.log(
        `Compare object evaluated twice: ${firstStableId === secondStableId}`);
  },

  async function testObjectInArray() {
    await Protocol.Runtime.evaluate({expression: 'this.b = [this.a, this.a]'});
    const {result:{result:{objectId:firstId}}} =
        await Protocol.Runtime.evaluate({expression: 'this.b[0]'});
    const firstStableId = await stableObjectId(firstId);
    const {result:{result:{objectId:secondId}}} =
        await Protocol.Runtime.evaluate({expression: 'this.b[1]'});
    const secondStableId = await stableObjectId(secondId);
    InspectorTest.log(
        `Compare first and second element: ${firstStableId === secondStableId}`);
  },

  async function testObjectOnPause() {
    const {result:{result:{objectId:globalId}}} =
        await Protocol.Runtime.evaluate({expression: 'this'});
    const globalStableId = await stableObjectId(globalId);
    const {result:{result:{objectId:aId}}} =
        await Protocol.Runtime.evaluate({expression: 'this.a'});
    const aStableId = await stableObjectId(aId);
    await Protocol.Debugger.enable();
    Protocol.Runtime.evaluate({expression: 'debugger'});
    const {params:{callFrames:[topFrame]}} =
        await Protocol.Debugger.oncePaused();
    const topFrameThisStableId = await stableObjectId(topFrame.this.objectId);
    InspectorTest.log(
        `Compare global and this: ${globalStableId === topFrameThisStableId}`);

    const {result:{result:{objectId:globalIdOnPause}}} =
        await Protocol.Debugger.evaluateOnCallFrame({
          callFrameId: topFrame.callFrameId,
          expression: 'this'
        });
    const globalStableIdOnPause = await stableObjectId(globalIdOnPause);
    InspectorTest.log(
        `Compare global and global on pause: ${
            globalStableId === globalStableIdOnPause}`);

    const {result:{result: props}} = await Protocol.Runtime.getProperties({
      objectId: topFrame.scopeChain[0].object.objectId
    });
    const {value:{objectId: aIdOnPause}} = props.find(prop => prop.name === 'a');
    const aStableIdOnPause = await stableObjectId(aIdOnPause);
    InspectorTest.log(`Compare a and a on pause: ${
        aStableId === aStableIdOnPause}`);
  }
]);

async function stableObjectId(objectId) {
  const {result:{
    internalProperties: props
  }} = await Protocol.Runtime.getProperties({
    objectId,
    ownProperties: true,
    generatePreview: false
  });
  return props.find(prop => prop.name === '[[StableObjectId]]').value.value;
}
