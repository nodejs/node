// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that trying to OSR does not skip loop interrupts for Runtime.evaluate timeout.');

(async function() {
  await Protocol.Runtime.enable();
  const result = await Protocol.Runtime.evaluate({
    expression: 'while(true) {}',
    throwOnSideEffect: true,
    timeout: 1000,
  });
  InspectorTest.logMessage(result);
  InspectorTest.completeTest();
})();
