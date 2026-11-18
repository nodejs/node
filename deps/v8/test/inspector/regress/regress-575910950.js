// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that returnByValue serialization charges undefined-valued ' +
    'properties against the protocol value budget.');

const source = `
  const o = {a: undefined};
  const a = [];
  a[10000000 - 3] = 0;
`;
contextGroup.addScript(source);

(async function test() {
  const response = await Protocol.Runtime.evaluate({
    expression: '[o, a]',
    returnByValue: true,
  });
  InspectorTest.logMessage(response.error ?? response.result.result);
  InspectorTest.completeTest();
})();
