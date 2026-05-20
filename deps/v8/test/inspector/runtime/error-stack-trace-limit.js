// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Checks that Error.stackTraceLimit works correctly');

contextGroup.addScript(`
function recurse(f, n) {
  if (n-- > 0) return recurse(f, n);
  return f();
}

function foo() {
  recurse(() => {
    throw new Error('Thrown from foo!');
  }, 20);
}
//# sourceURL=test.js
`);

InspectorTest.runAsyncTestSuite([
  async function testErrorStackTraceLimitWithRuntimeDisabled() {
    await Protocol.Runtime.evaluate({expression: 'Error.stackTraceLimit = 2'});
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'foo()'}));
  },

  async function testErrorStackTraceLimitWithRuntimeEnabled() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.evaluate({expression: 'Error.stackTraceLimit = 2'});
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'foo()'}));
    for (let size = 0; size <= 10; size += 5) {
      await Protocol.Runtime.evaluate(
          {expression: `Error.stackTraceLimit = ${size}`});
      await Protocol.Runtime.setMaxCallStackSizeToCapture({size});
      InspectorTest.logMessage(
          await Protocol.Runtime.evaluate({expression: 'foo()'}));
    }
    await Protocol.Runtime.disable();
  },

  async function testErrorStackTraceLimitNonNumber() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.evaluate(
        {expression: 'Error.stackTraceLimit = "Invalid"'});
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'foo()'}));
    await Protocol.Runtime.disable();
  },

  async function testErrorStackTraceLimitDeleted() {
    await Protocol.Runtime.enable();
    await Protocol.Runtime.evaluate(
        {expression: 'delete Error.stackTraceLimit'});
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'foo()'}));
    await Protocol.Runtime.disable();
  }
]);
