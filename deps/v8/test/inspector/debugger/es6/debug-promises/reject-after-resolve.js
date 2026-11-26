// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test debug events when we listen to all exceptions and ' +
    'the Promise is rejected in a chained closure after it has been resolved.');

const source = `
var log = [];
var thenRan = false;

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve(reject);
});

var q = p.then(
  function(value) {
    thenRan = true;
    if (log.length !== 2 || log[0] !== "resolve" || log[1] !== "end main") {
      log.push("unexpected: " + JSON.stringify(log));
    }
    value(new Error("reject"));
  });

log.push("end main");
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let paused = false;
  Protocol.Debugger.onPaused(async message => {
    paused = true;
    InspectorTest.log('FAIL: paused on unexpected exception');
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: logResult}} =
      await Protocol.Runtime.evaluate({expression: 'log', returnByValue: true});
  let {result: {result: thenResult}} =
      await Protocol.Runtime.evaluate({expression: 'thenRan'});

  InspectorTest.log(`Log: ${JSON.stringify(logResult.value)}`);
  InspectorTest.log(`Then handler executed: ${thenResult.value}`);
  InspectorTest.log(`Paused on exception: ${paused}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
