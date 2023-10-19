// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that console messages before Runtime.enable include a single stack frame');

contextGroup.addScript(`
function foo() {
  console.log("Hello from foo!");
}

function bar() {
  console.trace("Hello from bar!");
  foo();
}

console.error('Error on toplevel');
foo();
bar();
//# sourceURL=test.js`);

Protocol.Runtime.onConsoleAPICalled(
    ({params}) => InspectorTest.logMessage(params));

InspectorTest.runAsyncTestSuite([
  async function testEnable() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.disable();
  },

  async function testEnableAfterDiscard() {
    await Protocol.Runtime.discardConsoleEntries();
    await Protocol.Runtime.enable();
    await Protocol.Runtime.disable();
  }
]);
