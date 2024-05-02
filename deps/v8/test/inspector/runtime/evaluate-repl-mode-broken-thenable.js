// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
  "Tests that REPL mode still works even with a broken Promise.prototype.then");

(async function() {
  contextGroup.addScript(`
    Promise.prototype.then = () => {throw Error('you shall not evaluate')};
  `);

  const { result: { result }} = await Protocol.Runtime.evaluate({
    expression: '42',
    replMode: true,
  });
  InspectorTest.logMessage(result);

  InspectorTest.completeTest();
})();
