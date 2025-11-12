// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kozyatinskiy): fix or remove it later.
let {session, contextGroup, Protocol} = InspectorTest.start('Tests how async promise chains behave when reaching the limit of stacks');

(async function test(){
  InspectorTest.log('Checks correctness of promise chains when limit hit');
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

  await setMaxAsyncTaskStacks(3);
  runWithAsyncChainPromise(3, 'console.trace()');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  await setMaxAsyncTaskStacks(4);
  runWithAsyncChainPromise(3, 'console.trace()');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  await setMaxAsyncTaskStacks(5);
  runWithAsyncChainPromise(3, 'console.trace()');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  await setMaxAsyncTaskStacks(6);
  runWithAsyncChainPromise(3, 'console.trace()');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  await setMaxAsyncTaskStacks(7);
  runWithAsyncChainPromise(3, 'console.trace()');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  await setMaxAsyncTaskStacks(8);
  runWithAsyncChainPromise(3, 'console.trace()');
  InspectorTest.logMessage(await Protocol.Runtime.onceConsoleAPICalled());

  InspectorTest.completeTest();
})();

function runWithAsyncChainPromise(len, source) {
  InspectorTest.log(`Run expression '${source}' with async chain len: ${len}`);
  let asyncCall = `(function asyncCall(num) {
    if (num === 0) {
      ${source};
      return;
    }
    Promise.resolve().then(() => asyncCall(num - 1));
  })(${len})`;
  Protocol.Runtime.evaluate({expression: asyncCall});
}

async function setMaxAsyncTaskStacks(max) {
  let expression = `inspector.setMaxAsyncTaskStacks(${max})`;
  InspectorTest.log(expression);
  await Protocol.Runtime.evaluate({expression});
}
