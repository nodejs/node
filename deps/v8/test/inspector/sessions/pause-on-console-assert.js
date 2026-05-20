// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that multiple sessions pause once on console.assert.');

(async function test() {
  var contextGroup1 = new InspectorTest.ContextGroup();
  var session1 = await connect(contextGroup1, 1);
  var session2 = await connect(contextGroup1, 2);
  var contextGroup2 = new InspectorTest.ContextGroup();
  var session3 = await connect(contextGroup2, 3);

  InspectorTest.log('Pausing on exceptions in 1');
  await session1.Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  InspectorTest.log('Asserting in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});
  InspectorTest.log('Asserting in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});

  InspectorTest.log('Pausing on exceptions in both');
  await session2.Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  InspectorTest.log('Asserting in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});
  InspectorTest.log('Asserting in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});

  InspectorTest.log('Not pausing on exceptions');
  await session1.Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await session2.Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  InspectorTest.log('Asserting in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});
  InspectorTest.log('Asserting in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});

  InspectorTest.log('Pausing on exceptions in 3 (different context group)');
  await session3.Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  InspectorTest.log('Asserting in 3');
  await session3.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});
  InspectorTest.log('Asserting in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'console.assert(false)'});

  InspectorTest.completeTest();
})();

async function connect(contextGroup, num) {
  var session = contextGroup.connect();
  await session.Protocol.Debugger.enable();
  session.Protocol.Debugger.onPaused(message => {
    InspectorTest.log(`Paused in ${num} with reason ${message.params.reason}`);
    session.Protocol.Debugger.resume();
  });
  return session;
}
