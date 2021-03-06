// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Tests that multiple sessions share the context.');

(async function test() {
  var contextGroup = new InspectorTest.ContextGroup();
  var session1 = contextGroup.connect();
  var session2 = contextGroup.connect();

  InspectorTest.log('Assigning in 1');
  await session1.Protocol.Runtime.evaluate({expression: 'var a = 42;'});
  InspectorTest.log('Evaluating in 2');
  InspectorTest.logMessage(await session2.Protocol.Runtime.evaluate({expression: 'a'}));

  InspectorTest.log('Awaiting in 1');
  var promise = session1.Protocol.Runtime.evaluate({expression: 'var cb; new Promise(f => cb = f)', awaitPromise: true});
  InspectorTest.log('Resolving in 2');
  await session2.Protocol.Runtime.evaluate({expression: 'cb("foo")'});
  InspectorTest.log('Should resolve in 1');
  InspectorTest.logMessage(await promise);

  InspectorTest.completeTest();
})();
