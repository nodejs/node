// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests debug events when we only listen to uncaught exceptions and there is a catch handler for the to-be-rejected Promise.');

const source = `
var log = [];
var done = false;
var testError = null;

var reject_closure;

var p = new Promise(function(resolve, reject) {
  log.push("postpone p");
  reject_closure = reject;
});

var q = new Promise(function(resolve, reject) {
  log.push("resolve q");
  resolve();
});

q.then(function() {
  log.push("reject p");
  reject_closure(new Error("uncaught reject p"));  // event
});

log.push("end main");

function testDone(iteration) {
  function checkResult() {
    try {
      if (iteration >= 10) throw new Error('iteration >= 10');
      if (log.length === 4) {
        if (log[0] !== "postpone p" || log[1] !== "resolve q" ||
            log[2] !== "end main" || log[3] !== "reject p") {
          throw new Error('Unexpected log: ' + JSON.stringify(log));
        }
        done = true;
      } else {
        testDone(iteration + 1);
      }
    } catch (e) {
      testError = e.message;
    }
  }

  %EnqueueMicrotask(checkResult);
}

testDone(0);
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
    let isEventLine = line.indexOf('// event') > 0;
    let message = data.description.split('\n')[0].replace(/^Error:\s*/, '');
    InspectorTest.log(`Paused on exception: uncaught=${data.uncaught}`);
    InspectorTest.log(`Exception message: ${message}`);
    InspectorTest.log(`At event line: ${isEventLine}`);
    await Protocol.Debugger.resume();
  });

  contextGroup.addScript(source, 0, 0, 'test.js');
  await InspectorTest.waitForPendingTasks();

  let {result: {result: doneResult}} =
      await Protocol.Runtime.evaluate({expression: 'done'});
  let {result: {result: errResult}} =
      await Protocol.Runtime.evaluate({expression: 'testError'});
  let {result: {result: logResult}} =
      await Protocol.Runtime.evaluate({expression: 'log', returnByValue: true});

  InspectorTest.log(`Total exception events: ${events}`);
  InspectorTest.log(`Test done: ${doneResult.value}`);
  InspectorTest.log(`Test error: ${errResult.value}`);
  InspectorTest.log(`Log: ${JSON.stringify(logResult.value)}`);

  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
