// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Checks that console preview does not run user code');

contextGroup.addScript(`
function testLogObject() {
  let getterCalled = false;
  const obj = {};
  Object.defineProperty(obj, 'test', {
    get: (() => {
      getterCalled = true;
      return 'value';
    }).bind(null),
  });

  console.log(obj);
  console.clear();

  return {getterCalled};
}

//# sourceURL=test.js
`);

InspectorTest.runAsyncTestSuite([
  async function ObjectGetter() {
    await Protocol.Runtime.enable();
    const result = await Protocol.Runtime.evaluate({expression: 'testLogObject()', returnByValue: true});
    InspectorTest.logObject(result.result.result.value);
    await Protocol.Runtime.disable();
  }
]);
