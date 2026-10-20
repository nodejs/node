// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that Map/Set entry preview does not trigger Proxy traps on prototype chain.');

contextGroup.addScript(`
function testMapSetEntryPreview() {
  let ownKeysCalled = 0;
  let gOPDCalled = 0;
  const target = { foo: 1 };
  const evilProto = new Proxy(target, {
    ownKeys(t) {
      ownKeysCalled++;
      return Reflect.ownKeys(t);
    },
    getOwnPropertyDescriptor(t, prop) {
      gOPDCalled++;
      return Reflect.getOwnPropertyDescriptor(t, prop);
    }
  });
  const entryVal = {};
  Object.setPrototypeOf(entryVal, evilProto);
  const map = new Map([['key', entryVal]]);
  const set = new Set([entryVal]);
  return { map, set, getTrapsCalled: () => ({ ownKeysCalled, gOPDCalled }) };
}
`);

InspectorTest.runAsyncTestSuite([
  async function testMapSetEntryPreviewSideEffects() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.evaluate({
      expression: 'var { map, set, getTrapsCalled } = testMapSetEntryPreview(); map',
      generatePreview: true
    });
    await Protocol.Runtime.evaluate({
      expression: 'set',
      generatePreview: true
    });
    const result = await Protocol.Runtime.evaluate({
      expression: 'getTrapsCalled()',
      returnByValue: true
    });
    InspectorTest.logObject(result.result.result.value);
    await Protocol.Runtime.disable();
  }
]);
