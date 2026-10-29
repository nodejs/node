// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that Error.stack works correctly in Explicit Resource Management.');

contextGroup.addScript(`
function testUsing() {
  using disposable = {
    value: 1,
    [Symbol.dispose]() {
      throw new Error('Error thrown in using.');
    }
  };
}

async function testAwaitUsing() {
  await using disposable = {
    value: 1,
    [Symbol.asyncDispose]() {
      throw new Error('Error thrown in await using.');
    }
  };
}
async function callTestAwaitUsing() {
  await testAwaitUsing();
}

function testDisposableStack() {
  let stack = new DisposableStack();
  const disposable = {
    value: 1,
    [Symbol.dispose]() {
      throw new Error('Error thrown in DisposableStack.');
    }
  };
  stack.use(disposable);
  stack.dispose();
}

async function testAsyncDisposableStack() {
  let stack = new AsyncDisposableStack();
  const disposable = {
    value: 1,
    [Symbol.asyncDispose]() {
      throw new Error('Error thrown in AsyncDisposableStack.');
    }
  };
  stack.use(disposable);
  await stack.disposeAsync();
}
async function callTestAsyncDisposableStack() {
  await testAsyncDisposableStack();
}
//# sourceURL=test.js
`);

InspectorTest.runAsyncTestSuite([
  async function testErrorStackWithRuntimeDisabled() {
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               expression: 'testUsing()'
                             })).result.exceptionDetails.exception);

    await Promise.all([
      Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 5}),
    ]);
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               awaitPromise: true,
                               expression: 'callTestAwaitUsing()'
                             })).result.exceptionDetails.exception);

    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               expression: 'testDisposableStack()'
                             })).result.exceptionDetails.exception);

    await Promise.all([
      Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 5}),
    ]);
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               awaitPromise: true,
                               expression: 'callTestAsyncDisposableStack()'
                             })).result.exceptionDetails.exception);
  },

  async function testErrorStackWithRuntimeEnabled() {
    await Protocol.Runtime.enable();
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               expression: 'testUsing()'
                             })).result.exceptionDetails.exception);

    await Promise.all([
      Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 5}),
    ]);
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               awaitPromise: true,
                               expression: 'callTestAwaitUsing()'
                             })).result.exceptionDetails.exception);

    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               expression: 'testDisposableStack()'
                             })).result.exceptionDetails.exception);

    await Promise.all([
      Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 5}),
    ]);
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
                               awaitPromise: true,
                               expression: 'callTestAsyncDisposableStack()'
                             })).result.exceptionDetails.exception);
    await Protocol.Runtime.disable();
  },
]);
