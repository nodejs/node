// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test debug events when we only listen to uncaught exceptions and ' +
    'there is only a default reject handler for the to-be-rejected Promise.');

const source = `
var log = [];

var resolve, reject;
var p0 = new Promise(function(res, rej) { resolve = res; reject = rej; });
var p1 = p0.then(function() {
  log.push("p0.then");
  return Promise.reject(new Error("123"));
});
var p2 = p1.then(function() {
  log.push("p1.then");
});

var q = new Promise(function(res, rej) {
  log.push("resolve q");
  res();
});

q.then(function() {
  log.push("resolve p");
  resolve();
});

log.push("end main");
`;

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  let events = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames, data}}) => {
    events++;
    let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
        {scriptId: callFrames[0].location.scriptId});
    let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
    let isEventLine = line.indexOf('Promise.reject') > 0;
    let message = data.description.split('\n')[0].replace(/^Error:\s*/, '');
    InspectorTest.log(`Paused on exception: uncaught=${data.uncaught}`);
    InspectorTest.log(`Exception message: ${message}`);
    InspectorTest.log(`At event line: ${isEventLine}`);
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: logResult}} =
      await Protocol.Runtime.evaluate({expression: 'log', returnByValue: true});

  InspectorTest.log(`Total exception events: ${events}`);
  InspectorTest.log(`Log: ${JSON.stringify(logResult.value)}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
