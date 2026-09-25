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
  const expr = "globalThis";
  InspectorTest.log(`Prototype chain for "${expr}":`);
  const { result: { result } } = await Protocol.Runtime.evaluate({ expression: expr });
  InspectorTest.logMessage(result);
  await logPrototypeChain(result.objectId);

  // Set weird[@@toStringTag] in order to avoid finding tag value installed on
  // globalThis object.
  const expr2 = "var weird = {[Symbol.toStringTag]: 'Object'}; weird.__proto__ = globalThis; weird;";
  InspectorTest.log(`Prototype chain for "${expr2}":`);
  const { result: { result: result2 }  } = await Protocol.Runtime.evaluate({ expression: expr2 });
  InspectorTest.logMessage(result2);
  await logPrototypeChain(result2.objectId);

  InspectorTest.completeTest();
})();
