// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that returnByValue serialization limits the total number of ' +
    'values for shared (non-cyclic) references.');

async function evaluate(expression) {
  const response =
      await Protocol.Runtime.evaluate({expression, returnByValue: true});
  InspectorTest.logMessage(response.error ?? response.result.result);
}

InspectorTest.runAsyncTestSuite([
  async function testSharedReferences() {
    await evaluate('const o = {a: 1}; [o, o, {b: o}]');
  },

  async function testExponentialSharedArrays() {
    // Each level references the previous level twice, so the serialized
    // value would have 2^3 * 4e6 elements, although the object graph is small.
    await evaluate(`
      let x = 0;
      for (let i = 0; i < 3; ++i) {
        const a = [];
        a[4e6 - 1] = 0;
        a[0] = a[1] = x;
        x = a;
      }
      x`);
  },

  async function testNestedLargeArrays() {
    await evaluate(
        'const a = [], b = []; a[6e6 - 1] = b[6e6 - 1] = 0; a[0] = b; a');
  },
]);
