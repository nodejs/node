// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that all sessions get console api notifications.');

function connect(contextGroup, num) {
  var session = contextGroup.connect();
  session.Protocol.Runtime.onConsoleAPICalled(message => {
    InspectorTest.log('From session ' + num);
    InspectorTest.logMessage(message);
  });
  return session;
}

(async function test() {
  var contextGroup = new InspectorTest.ContextGroup();
  var session1 = connect(contextGroup, 1);
  var session2 = connect(contextGroup, 2);
  await session1.Protocol.Runtime.enable();
  await session2.Protocol.Runtime.enable();

  InspectorTest.log('Error in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'console.error(1)'});

  InspectorTest.log('Logging in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'console.log(2)'});

  InspectorTest.log('Error in setTimeout 1');
  await session1.Protocol.Runtime.evaluate({expression: 'setTimeout(() => console.error("a"), 0)'});
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log('Logging in setTimeout 2');
  await session2.Protocol.Runtime.evaluate({expression: 'setTimeout(() => console.log("b"), 0)'});
  await InspectorTest.waitForPendingTasks();

  InspectorTest.completeTest();
})();
