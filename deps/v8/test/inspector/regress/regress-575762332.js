// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that returnByValue serialization limits the total length of ' +
    'strings for shared references.');

contextGroup.addScript(`
  // Two references to this string exceed the maximum string length together.
  var s = 'x'.repeat(Math.floor(%StringMaxLength() / 2) + 1);
`);

async function evaluate(expression) {
  const response =
      await Protocol.Runtime.evaluate({expression, returnByValue: true});
  InspectorTest.logMessage(response.error ?? response.result.result);
}

InspectorTest.runAsyncTestSuite([
  async function testSmallStrings() {
    await evaluate('const t = "abc"; [t, t, {t}]');
  },

  async function testLargeStringInArray() {
    await evaluate('[s, s]');
  },

  async function testLargeStringInObject() {
    await evaluate('({a: s, b: s})');
  },
]);
