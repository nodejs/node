// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that multiple sessions do not interfere with each other\'s remote objects.');

(async function test() {
  var contextGroup = new InspectorTest.ContextGroup();
  var session1 = contextGroup.connect();
  var session2 = contextGroup.connect();

  InspectorTest.log('Evaluating in 1');
  var result1 = await session1.Protocol.Runtime.evaluate({expression: '({a: 42})'});
  InspectorTest.log('Evaluating in 2');
  var result2 = await session2.Protocol.Runtime.evaluate({expression: '({a: 17})'});

  await print(2, session2, result2);
  await print(1, session1, result1);
  InspectorTest.log('Disconnecting 2');
  session2.disconnect();
  await print(1, session1, result1);

  InspectorTest.completeTest();
})();

async function print(num, session, message) {
  InspectorTest.log('Retrieving properties in ' + num);
  var objectId = message.result.result.objectId;
  InspectorTest.logMessage(await session.Protocol.Runtime.getProperties({objectId, ownProperties: true}));
}
