// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that mutating private method wrapper does not crash descriptionForPrivateMethod');

(async () => {
  await Protocol.Runtime.enable();

  const {result: {result: instance}} = await Protocol.Runtime.evaluate({
    expression: 'new class { #m() {} }',
  });

  const {result: {internalProperties}} = await Protocol.Runtime.getProperties({
    objectId: instance.objectId,
  });

  const privateMethodsProp =
      internalProperties.find(p => p.name === '[[PrivateMethods]]');
  InspectorTest.log('Found [[PrivateMethods]]: ' + !!privateMethodsProp);

  const {result: {result: privateMethodsList}} =
      await Protocol.Runtime.getProperties({
        objectId: privateMethodsProp.value.objectId,
      });

  const methodWrapper = privateMethodsList[0].value;
  InspectorTest.log('Method wrapper found: ' + !!methodWrapper);

  InspectorTest.log('Mutating wrapper.value to non-function...');
  await Protocol.Runtime.callFunctionOn({
    objectId: methodWrapper.objectId,
    functionDeclaration: 'function() { this.value = 42; }',
  });

  InspectorTest.log('Re-inspecting private methods list:');
  const reinspect = await Protocol.Runtime.getProperties({
    objectId: privateMethodsProp.value.objectId,
  });
  InspectorTest.logMessage(reinspect);

  InspectorTest.completeTest();
})();
