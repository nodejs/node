// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Runtime.getProperties.');

InspectorTest.runAsyncTestSuite([
  async function testObject() {
    await getProperties('({a: 1})', { ownProperties: true });
    await getProperties(`(function(){
      let b = {};
      Object.defineProperty(b, 'a', {
        configurable: false,
        enumerable: false,
        value: 42,
        writable: false
      });
      Object.defineProperty(b, 'b', {
        configurable: false,
        enumerable: false,
        value: 42,
        writable: true
      });
      Object.defineProperty(b, 'c', {
        configurable: true,
        enumerable: false,
        value: 42,
        writable: false
      });
      Object.defineProperty(b, 'd', {
        configurable: false,
        enumerable: true,
        value: 42,
        writable: false
      });
      Object.defineProperty(b, 'e', {
        set: () => 0,
        get: () => 42
      });
      Object.defineProperty(b, Symbol(42), {
        value: 239
      });
      return b;
    })()`, { ownProperties: true });
  }
]);

async function getProperties(expression, options) {
  try {
    const { result: { result: { objectId } } } =
        await Protocol.Runtime.evaluate({ expression });
    const result = await Protocol.Runtime.getProperties({
      objectId,
      ownProperties: options.ownProperties
    });
    InspectorTest.logMessage(result);
  } catch (e) {
    InspectorTest.log(e.stack);
  }
}
