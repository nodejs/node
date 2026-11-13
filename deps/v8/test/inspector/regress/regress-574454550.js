// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that returnByValue serialization of a large self-referential ' +
    'sparse array fails quickly.');

(async function test() {
  await Protocol.Runtime.enable();
  const response = await Protocol.Runtime.evaluate({
    expression: 'var a = []; a[1e7 - 1] = 0; a[0] = a; a',
    returnByValue: true,
  });
  InspectorTest.logMessage(response.error);
  InspectorTest.completeTest();
})();
