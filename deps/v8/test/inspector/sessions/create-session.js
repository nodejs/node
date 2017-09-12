// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that creating multiple sessions works.');

function connect(contextGroup, num) {
  var session = contextGroup.connect();
  var executionContextId;
  session.Protocol.Runtime.onExecutionContextCreated(message => {
    InspectorTest.log('From session ' + num);
    InspectorTest.logMessage(message);
    executionContextId = message.params.context.id;
  });
  session.Protocol.Runtime.onExecutionContextDestroyed(message => {
    InspectorTest.log('From session ' + num);
    InspectorTest.logMessage(message);
    InspectorTest.log('id matching: ' + (message.params.executionContextId === executionContextId));
  });
  return session;
}

(async function test() {
  var contextGroup = new InspectorTest.ContextGroup();
  InspectorTest.log('Connecting session 1');
  var session1 = connect(contextGroup, 1);
  await session1.Protocol.Runtime.enable();

  InspectorTest.log('Connecting session 2');
  var session2 = connect(contextGroup, 2);
  await session2.Protocol.Runtime.enable();

  InspectorTest.log('Reconnecting session 2');
  session2.reconnect();
  await session2.Protocol.Runtime.enable();

  InspectorTest.log('Reconnecting session 1');
  session1.reconnect();
  await session1.Protocol.Runtime.enable();

  InspectorTest.log('Connecting session 3');
  var session3 = connect(contextGroup, 3);
  await session3.Protocol.Runtime.enable();

  InspectorTest.log('Destroying and creating context');
  await session2.Protocol.Runtime.evaluate({expression: 'inspector.fireContextDestroyed(); inspector.fireContextCreated(); '});

  InspectorTest.log('Disconnecting all sessions');
  session1.disconnect();
  session2.disconnect();
  session3.disconnect();

  InspectorTest.log('Connecting session 4');
  var session4 = connect(contextGroup, 4);
  await session4.Protocol.Runtime.enable();

  InspectorTest.completeTest();
})();
