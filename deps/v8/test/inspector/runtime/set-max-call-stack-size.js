// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Runtime.setMaxCallStackSizeToCapture.');

Protocol.Runtime.onConsoleAPICalled(
    ({params}) => InspectorTest.logMessage(params));

contextGroup.addScript(`
function testConsoleLog() {
  console.log("Log message.");
}

function testConsoleTrace() {
  function bar(callback) {
    console.trace("Nested call.");
    callback();
  }

  function foo(callback) {
    bar(callback);
  }

  return new Promise(function executor(resolve) {
    setTimeout(foo.bind(undefined, resolve), 0);
  });
}

function testThrow() {
  function bar() {
    throw new Error();
  }

  function foo() {
    bar();
  }

  foo();
}
//# sourceURL=test.js`);

InspectorTest.runAsyncTestSuite([
  async function testBeforeEnable() {
    const {error} =
        await Protocol.Runtime.setMaxCallStackSizeToCapture({size: 0});
    InspectorTest.logMessage(error);
  },

  async function testNegativeSize() {
    await Protocol.Runtime.enable();
    const {error} =
        await Protocol.Runtime.setMaxCallStackSizeToCapture({size: -42});
    InspectorTest.logMessage(error);
    await Protocol.Runtime.disable();
  },

  async function testConsoleLogBeforeEnable() {
    await Protocol.Runtime.evaluate({expression: 'testConsoleLog()'});
    await Protocol.Runtime.enable();
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testConsoleTrace() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 10}),
    ]);
    for (let size = 0; size <= 2; ++size) {
      await Protocol.Runtime.setMaxCallStackSizeToCapture({size});
      InspectorTest.log(`Test with max size ${size}.`);
      await Protocol.Runtime.evaluate(
          {expression: 'testConsoleTrace()', awaitPromise: true});
    }
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  },

  async function testException() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 0}),
    ]);
    for (let size = 0; size <= 2; ++size) {
      await Protocol.Runtime.setMaxCallStackSizeToCapture({size});
      InspectorTest.log(`Test with max size ${size}.`);
      const {result: {exceptionDetails}} =
          await Protocol.Runtime.evaluate({expression: 'testThrow()'});
      InspectorTest.logMessage(exceptionDetails);
    }
    await Promise.all([
      Protocol.Runtime.discardConsoleEntries(),
      Protocol.Runtime.disable(),
    ]);
  }
])
