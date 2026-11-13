// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Regression test for crbug.com/483851884.');

const source = `
var s = "";
try {
  s = "a".repeat(256 * 1024 * 1024);
} catch(e) {
}
var expr = "[s, s, s, s, s, s, s, s, s, s]";
`;

(async function test() {
  await Protocol.Runtime.enable();
  contextGroup.addScript(source, 0, 0, 'test.js');

  const response = await Protocol.Runtime.evaluate({
    expression: '[s, s, s, s, s, s, s, s, s, s]',
    returnByValue: true,
  });
  InspectorTest.log('Has error: ' + !!response.error);
  if (response.error) {
    InspectorTest.log('Error message: ' + response.error.message);
  }
  InspectorTest.completeTest();
})();
