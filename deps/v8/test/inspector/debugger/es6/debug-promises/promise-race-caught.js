// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests Promise.race with catch handler has no uncaught exception break.');

const source = `
var caught = false;
var p1 = Promise.resolve();
p1.name = "p1";

var p2 = p1.then(function() {
  throw new Error("caught");
});
p2.name = "p2";

var p3 = Promise.race([p2]);
p3.name = "p3";

p3.catch(function(e) {
  caught = true;
});
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  let paused = false;
  Protocol.Debugger.onPaused(async message => {
    paused = true;
    InspectorTest.log('FAIL: paused on unexpected exception');
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result}} =
      await Protocol.Runtime.evaluate({expression: 'caught'});
  InspectorTest.log(`Promise catch handler executed: ${result.value}`);
  InspectorTest.log(`Paused on exception: ${paused}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
