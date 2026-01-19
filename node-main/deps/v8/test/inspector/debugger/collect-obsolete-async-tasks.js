// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we collect obsolete async tasks with async stacks.');

contextGroup.addScript(`
function test() {
  inspector.setMaxAsyncTaskStacks(128);
  var p = Promise.resolve().then(() => 42);

  inspector.dumpAsyncTaskStacksStateForTest();
  inspector.setMaxAsyncTaskStacks(128);
  inspector.dumpAsyncTaskStacksStateForTest();

  p.then(() => 42).then(() => 239);

  inspector.dumpAsyncTaskStacksStateForTest();
  inspector.setMaxAsyncTaskStacks(128);
  inspector.dumpAsyncTaskStacksStateForTest();

  setTimeout(() => 42, 0);

  inspector.dumpAsyncTaskStacksStateForTest();
  inspector.setMaxAsyncTaskStacks(128);
  inspector.dumpAsyncTaskStacksStateForTest();
}
`);

(async function test() {
  Protocol.Debugger.enable();
  Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});
  await Protocol.Runtime.evaluate({expression: 'test()'});
  InspectorTest.completeTest();
})()
