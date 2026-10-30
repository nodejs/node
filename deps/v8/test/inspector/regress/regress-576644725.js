// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that session reconnect restores isolateId correctly.');

(async function test() {
  await Protocol.Runtime.enable();
  const consolePromise = Protocol.Runtime.onceConsoleAPICalled();
  await Protocol.Runtime.evaluate({expression: 'console.log({a: 1})'});
  const message = await consolePromise;

  session.reconnect();

  const replayedPromise = Protocol.Runtime.onceConsoleAPICalled();
  await Protocol.Runtime.enable();
  const replayedMessage = await replayedPromise;

  const objectId = replayedMessage.params.args[0].objectId;
  InspectorTest.log('Has objectId: ' + !!objectId);
  InspectorTest.log('Valid isolateId prefix: ' + !objectId.startsWith('0.'));

  const properties = await Protocol.Runtime.getProperties({
    objectId: objectId,
    ownProperties: true,
  });
  InspectorTest.logMessage(properties.result.result);

  InspectorTest.completeTest();
})();
