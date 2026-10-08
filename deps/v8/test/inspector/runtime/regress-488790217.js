// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let { session, contextGroup, Protocol } = InspectorTest.start('Tests that prototype tampering does not affect native getters/setters');

(async function () {
  await Protocol.Runtime.enable();

  await Protocol.Runtime.evaluate({
    expression: `
      globalThis.__target = inspector.createObjectWithNativeDataProperty('shared', false);
      __target.owner = 1;
      __target.tail1 = 2;
      __target.tail2 = 3;
      Object.defineProperty(Object.prototype, 'name', {
        configurable: true,
        set(v) {
          if (v === 'shared') {
            delete __target.owner;
            delete __target.tail1;
          }
        }
      });
      __target;
    `
  });

  const { result: { result: { objectId } } } = await Protocol.Runtime.evaluate({ expression: '__target' });

  InspectorTest.log('Runtime.getProperties with generatePreview: true');
  InspectorTest.logMessage(await Protocol.Runtime.getProperties({ objectId, ownProperties: true, generatePreview: true }));
  InspectorTest.log('Runtime.getProperties with ownProperties: true');
  InspectorTest.logMessage(await Protocol.Runtime.getProperties({ objectId, ownProperties: true }));
  InspectorTest.log('Runtime.getProperties with accessorPropertiesOnly: false');
  InspectorTest.logMessage(await Protocol.Runtime.getProperties({ objectId, accessorPropertiesOnly: false }));

  InspectorTest.completeTest();
})();
