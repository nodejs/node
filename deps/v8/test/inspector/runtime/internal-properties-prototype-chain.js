// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start('Checks that only one of JSGlobalObject/JSGlobalProxy shows up in the prototype chain');

function findPrototypeObjectId(response) {
  const { result: { internalProperties } } = response;
  for (const prop of internalProperties || []) {
    if (prop.name === '[[Prototype]]') {
      return prop;
    }
  }
}

async function logPrototypeChain(objectId) {
  while (true) {
    const response = await Protocol.Runtime.getProperties({ objectId });
    const prototype = findPrototypeObjectId(response);
    if (!prototype) break;

    InspectorTest.logMessage(prototype);
    objectId = prototype.value.objectId;
  }
}

(async () => {
  InspectorTest.log('Prototype chain for "globalThis":');
  const { result: { result } } = await Protocol.Runtime.evaluate({ expression: 'globalThis' });
  InspectorTest.logMessage(result);
  await logPrototypeChain(result.objectId);

  InspectorTest.log('\nPrototype chain for "var weird = {}; weird.__proto__ = globalThis; weird;":')
  const { result: { result: result2 }  } = await Protocol.Runtime.evaluate({ expression: 'var weird = {}; weird.__proto__ = globalThis; weird;' });
  InspectorTest.logMessage(result2);
  await logPrototypeChain(result2.objectId);

  InspectorTest.completeTest();
})();
