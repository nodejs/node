// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-662935.');

contextGroup.addScript(`
function overflow() {
  return new Promise(function foo() { foo() });
}
`);

(async function test() {
  await Protocol.Debugger.enable();

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'overflow().constructor === Promise',
  });
  InspectorTest.logMessage(result);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
