// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-always-turbofan

const { contextGroup, Protocol } = InspectorTest.start(
  `Tests that exercise Debugger.setVariableValue`);

(async function test(){
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();
  contextGroup.addInlineScript(`
      function test() {
        let num = 5;
        let obj = {b: 4};
        let bool = true;
        let set_breakpoint_here = true;
        debugger;
      }
    `, 'test.js');
  Protocol.Runtime.evaluate({expression: "test();"});
  const {params:{callFrames:[{callFrameId}]}} = await Protocol.Debugger.oncePaused();
  InspectorTest.runAsyncTestSuite([
    async function testSetVariableValueMain() {
      // Set value to a Number
      let result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId });
      InspectorTest.logMessage(result);
      await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { value: 10 }, callFrameId });
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId });
      InspectorTest.logMessage(result);

      // Set Value to NaN
      await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { unserializableValue: 'NaN' }, callFrameId });
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId });
      InspectorTest.logMessage(result);

      // Set Value to boolean:true
      await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { value: true }, callFrameId });
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId });
      InspectorTest.logMessage(result);

      // Set Value to a new object
      await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { value: { a: 3 } }, callFrameId });
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId });
      InspectorTest.logMessage(result);
      let props = await Protocol.Runtime.getProperties({ objectId: result.result.result.objectId, ownProperties: true });
      InspectorTest.logMessage(props);

      // Set Value to new Array
      await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { value: ['1', '2', '3'] }, callFrameId });
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId });
      InspectorTest.logMessage(result);
      props = await Protocol.Runtime.getProperties({ objectId: result.result.result.objectId, ownProperties: true });
      InspectorTest.logMessage(props);

      // Set Value to existing object with objectId
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'obj', callFrameId: callFrameId });
      let objectId = result.result.result.objectId;
      await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { objectId: objectId }, callFrameId: callFrameId });
      result = await Protocol.Debugger.evaluateOnCallFrame({ expression: 'num', callFrameId: callFrameId });
      InspectorTest.logMessage(result);
      props = await Protocol.Runtime.getProperties({ objectId: result.result.result.objectId, ownProperties: true });
      InspectorTest.logMessage(props);
    },
    async function testInvalidFrame() {
      let result = await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { unserializableValue: 'NaN' }, callFrameId: 'fakeCallFrame' });
      InspectorTest.log('setVariableValue with invalid callFrameId');
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
      result = await Protocol.Debugger.setVariableValue({ scopeNumber: 'invalidScopeType', variableName: 'num', newValue: { unserializableValue: 'NaN' }, callFrameId });
      InspectorTest.log('setVariableValue with invalid scopeNumber')
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
      result = await Protocol.Debugger.setVariableValue({ scopeNumber: 1000, variableName: 'num', newValue: { unserializableValue: 'NaN' }, callFrameId });
      InspectorTest.log('setVariableValue with invalid scopeNumber');
      InspectorTest.logMessage(result);
      result = await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'FakeObjectName', newValue: { unserializableValue: 'NaN' }, callFrameId });
      InspectorTest.log('setVariableValue with invalid variableName');
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
    },
    async function testNewValueErrors() {
      let result = await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { unserializableValue: 'not unserializable value' }, callFrameId });
      InspectorTest.log('setVariableValue with invalid unserializableValue');
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
      result = await Protocol.Debugger.setVariableValue({ scopeNumber: 0, variableName: 'num', newValue: { objectId: 2000 }, callFrameId });
      InspectorTest.log('setVariableValue with invalid objectId');
      InspectorTest.logMessage(InspectorTest.trimErrorMessage(result));
    }
  ]);

})();
