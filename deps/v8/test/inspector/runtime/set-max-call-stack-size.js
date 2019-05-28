// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Runtime.setMaxCallStackSizeToCapture.');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(
    message => InspectorTest.logMessage(message.params));

contextGroup.addScript(`
function bar() {
  console.trace("Nested call.");
}

function foo() {
  bar();
}

async function test() {
  setTimeout(foo, 0);
}
//# sourceURL=test.js`);

Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 10});
(async function test() {
  await Protocol.Runtime.setMaxCallStackSizeToCapture({size: 0});
  InspectorTest.log('Test with max size 0.');
  await Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js'});
  await Protocol.Runtime.setMaxCallStackSizeToCapture({size: 1});
  InspectorTest.log('Test with max size 1.');
  await Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js'});
  await Protocol.Runtime.setMaxCallStackSizeToCapture({size: 2});
  InspectorTest.log('Test with max size 2.');
  await Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js'});
  InspectorTest.completeTest();
})();
