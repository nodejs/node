// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that CommandLineAPI evaluation ignores a setter on Array.prototype[0].');

contextGroup.addScript(`
Object.defineProperty(Array.prototype, 0, {
  set() {
    throw 42;
  }
});
`);

(async function test() {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    includeCommandLineAPI: true,
    expression: '',
  }));
  InspectorTest.completeTest();
})();
