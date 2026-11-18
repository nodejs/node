// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests debug events when we only listen to uncaught exceptions and there is a catch handler for the to-be-rejected Promise.');

const source = `
var caught = false;
var p = new Promise(function(resolve, reject) {
  resolve();
});

var q = p.then(
  function() {
    return Promise.reject(Error("caught reject"));
  });

q.catch(
  function(e) {
    caught = (e.message === "caught reject");
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
