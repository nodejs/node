// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Checks that error.stack works correctly');

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
  async function testErrorStackWithRuntimeDisabled() {
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'foo()'}));
  },

  async function testErrorStackWithRuntimeEnabled() {
    await Protocol.Runtime.enable();
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'foo()'}));
    for (let size = 0; size <= 10; size += 5) {
      await Protocol.Runtime.setMaxCallStackSizeToCapture({size});
      InspectorTest.logMessage(
          await Protocol.Runtime.evaluate({expression: 'foo()'}));
    }
    await Protocol.Runtime.disable();
  },
]);
