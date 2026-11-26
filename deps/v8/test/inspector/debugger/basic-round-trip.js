// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests basic protocol round trip.');

(async function test() {
  const {result: {result: evalResult}} = await Protocol.Runtime.evaluate({
    expression: 'function f() { return 2 }; 3',
  });
  InspectorTest.log('evaluate result: ' + evalResult.value);

  const {result: {result: fResult}} = await Protocol.Runtime.evaluate({
    expression: 'f()',
  });
  InspectorTest.log('f() result: ' + fResult.value);

  InspectorTest.completeTest();
})();
