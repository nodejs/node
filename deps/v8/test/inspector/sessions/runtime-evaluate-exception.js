// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that all sessions get exception notifications.');

function connect(contextGroup, num) {
  var session = contextGroup.connect();
  var exceptionId;
  session.Protocol.Runtime.onExceptionThrown(message => {
    InspectorTest.log('From session ' + num);
    InspectorTest.logMessage(message);
    exceptionId = message.params.exceptionDetails.exceptionId;
  });
  session.Protocol.Runtime.onExceptionRevoked(message => {
    InspectorTest.log('From session ' + num);
    InspectorTest.logMessage(message);
    InspectorTest.log('id matching: ' + (message.params.exceptionId === exceptionId));
  });
  return session;
}

(async function test() {
  var contextGroup = new InspectorTest.ContextGroup();
  var session1 = connect(contextGroup, 1);
  var session2 = connect(contextGroup, 2);
  await session1.Protocol.Runtime.enable();
  await session2.Protocol.Runtime.enable();

  InspectorTest.log('Throwing in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'throw "error1";'});

  InspectorTest.log('Throwing in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'throw "error2";'});

  InspectorTest.log('Throwing in setTimeout 1');
  await session1.Protocol.Runtime.evaluate({expression: 'setTimeout(() => { throw "error3"; }, 0)'});
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log('Throwing in setTimeout 2');
  await session2.Protocol.Runtime.evaluate({expression: 'setTimeout(() => { throw "error4"; }, 0)'});
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log('Rejecting in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'var p2; setTimeout(() => { p2 = Promise.reject("error5") }, 0)'});
  await InspectorTest.waitForPendingTasks();
  InspectorTest.log('Revoking in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'setTimeout(() => { p2.catch() }, 0);'});
  await InspectorTest.waitForPendingTasks();

  InspectorTest.log('Rejecting in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'var p1; setTimeout(() => { p1 = Promise.reject("error6")} , 0)'});
  await InspectorTest.waitForPendingTasks();
  InspectorTest.log('Revoking in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'setTimeout(() => { p1.catch() }, 0);'});
  await InspectorTest.waitForPendingTasks();

  InspectorTest.completeTest();
})();
