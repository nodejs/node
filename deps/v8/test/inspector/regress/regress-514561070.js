// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {contextGroup, Protocol} = InspectorTest.start(
    'Tests that CommandLineAPIScope ignores a getter on Array.prototype[0].');

contextGroup.addScript(`
Array.prototype.__defineGetter__(0, function() {
  return new Number();
});
`);

(async function test() {
  InspectorTest.logMessage(await Protocol.Runtime.evaluate({
    includeCommandLineAPI: true,
    expression: '',
  }));
  InspectorTest.completeTest();
})();
