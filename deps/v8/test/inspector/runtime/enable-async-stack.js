// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that async stack is captured when Runtime.setAsyncCallStackDepth is called with an argument greater than zero.');

Protocol.Runtime.enable();
Protocol.Runtime.onConsoleAPICalled(
    message => InspectorTest.logMessage(message.params.stackTrace));

contextGroup.addScript(`
async function test() {
  setTimeout('console.trace("async"); console.log("no-async");', 0);
}
//# sourceURL=test.js`);

Protocol.Runtime.setAsyncCallStackDepth({maxDepth: 10});
Protocol.Runtime.evaluate({ expression: 'test()//# sourceURL=expr.js'})
  .then(InspectorTest.completeTest);
