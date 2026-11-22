// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that deep serialization limits the total length of strings for ' +
    'shared references.');

const source = `
  var s = 'x'.repeat(%StringMaxLength());
`;
contextGroup.addScript(source);

async function evaluate(expression) {
  const response = await Protocol.Runtime.evaluate(
      {expression, serializationOptions: {serialization: 'deep'}});
  if (response.error) {
    InspectorTest.logMessage('Error: ' + response.error.message);
  } else {
    InspectorTest.logMessage(response.result.result);
  }
}

InspectorTest.runAsyncTestSuite([
  async function testSmallStrings() {
    await evaluate('const t = "abc"; [t, t, {t}]');
  },

  async function testLargeStringInArray() {
    await evaluate("['a', s]");
  },

  async function testLargeStringInObject() {
    await evaluate('({a: s})');
  },

  async function testLargeStringInMap() {
    await evaluate("new Map([['a', s]])");
  },

  async function testLargeStringAfterBigInt() {
    await evaluate('[1n, s]');
  },
]);
