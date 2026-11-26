// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test debug events when we listen to all exceptions and ' +
    'there is a catch handler for the to-be-rejected Promise.');

const source = `
var log = [];
var caughtError = null;

var p = new Promise(function(resolve, reject) {
  log.push("resolve");
  resolve();
});

var q = p.then(
  function(value) {
    log.push("reject");
    return Promise.reject(new Error("reject"));
  });

q.catch(
  function(e) {
    caughtError = e.message;
  });

log.push("end main");
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let events = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames, data}}) => {
    events++;
    let message = data.description.split('\n')[0].replace(/^Error:\s*/, '');
    InspectorTest.log(`Paused on exception: uncaught=${data.uncaught}`);
    InspectorTest.log(`Exception message: ${message}`);
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: logResult}} =
      await Protocol.Runtime.evaluate({expression: 'log', returnByValue: true});
  let {result: {result: caughtResult}} =
      await Protocol.Runtime.evaluate({expression: 'caughtError'});

  InspectorTest.log(`Total exception events: ${events}`);
  InspectorTest.log(`Caught error message: ${caughtResult.value}`);
  InspectorTest.log(`Log: ${JSON.stringify(logResult.value)}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
