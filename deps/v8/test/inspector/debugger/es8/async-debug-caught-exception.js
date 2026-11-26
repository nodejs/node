// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignore-unhandled-promises --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests exception events in async functions with caught and uncaught exceptions.');

const source = `
var exception = null;
var log = [];

async function thrower() {
  throw "a";  // Exception a
}

async function caught_throw() {
  try {
    await thrower();
  } catch (e) {
    if (e !== "a") throw new Error('Expected "a", got ' + e);
  }
}

var reject = Promise.reject("b");

async function caught_reject() {
  try {
    await reject;
  } catch (e) {
    if (e !== "b") throw new Error('Expected "b", got ' + e);
  }
}

// "rethrown" uncaught exceptions in return don't cause another event
async function propagate_inner() { return thrower(); }
async function propagate_outer() { return propagate_inner(); }

// Also don't propagate if an await interceded
async function propagate_await() { await 1; return thrower(); }
async function propagate_await_outer() { return propagate_await(); }

var resolve;
var turnOnListenerPromise = new Promise(r => resolve = r);
async function confused() {
  await turnOnListenerPromise;
  throw foo;  // Exception e
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let listenerActive = false;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (listenerActive) {
      let {result: {scriptSource}} = await Protocol.Debugger.getScriptSource(
          {scriptId: callFrames[0].location.scriptId});
      let line = scriptSource.split('\n')[callFrames[0].location.lineNumber];
      let match = /Exception (\w)/.exec(line);
      if (match) {
        await Protocol.Runtime.evaluate(
            {expression: `log.push("${match[1]}");`});
      } else {
        await Protocol.Runtime.evaluate({
          expression:
              `exception = "No match on line: " + ${JSON.stringify(line)};`,
        });
      }
    }
    await Protocol.Debugger.resume();
  });

  async function checkResults(expectedLog) {
    let {result: {result: logRes}} = await Protocol.Runtime.evaluate({
      expression: 'log',
      returnByValue: true,
    });
    let {result: {result: excRes}} = await Protocol.Runtime.evaluate({
      expression: 'exception',
      returnByValue: true,
    });
    let logMatches =
        JSON.stringify(logRes.value) === JSON.stringify(expectedLog);
    InspectorTest.log(
        `log: ${JSON.stringify(logRes.value)} (matches: ${logMatches})`);
    InspectorTest.log(`exception: ${excRes.value}`);
  }

  // Caught throw, events on any exception.
  InspectorTest.log('Caught throw, events on any exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  listenerActive = true;
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.evaluate({
    expression: 'caught_throw(); %PerformMicrotaskCheckpoint();',
  });
  listenerActive = false;
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await checkResults(['a']);

  // Caught throw, events on uncaught exception.
  InspectorTest.log('Caught throw, events on uncaught exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  listenerActive = true;
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
  await Protocol.Runtime.evaluate({
    expression: 'caught_throw(); %PerformMicrotaskCheckpoint();',
  });
  listenerActive = false;
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await checkResults([]);

  // Caught reject, events on any exception.
  InspectorTest.log('Caught reject, events on any exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  listenerActive = true;
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.evaluate({
    expression: 'caught_reject(); %PerformMicrotaskCheckpoint();',
  });
  listenerActive = false;
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await checkResults([]);

  // Caught reject, events on uncaught exception.
  InspectorTest.log('Caught reject, events on uncaught exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  listenerActive = true;
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
  await Protocol.Runtime.evaluate({
    expression: 'caught_reject(); %PerformMicrotaskCheckpoint();',
  });
  listenerActive = false;
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await checkResults([]);

  // "rethrown" uncaught exceptions in return don't cause another event
  InspectorTest.log('Propagate return, events on any exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  listenerActive = true;
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
  await Protocol.Runtime.evaluate({
    expression: 'propagate_outer(); %PerformMicrotaskCheckpoint();',
  });
  await checkResults(['a']);

  // Also don't propagate if an await interceded
  InspectorTest.log('Propagate await return, events on any exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  await Protocol.Runtime.evaluate({
    expression: 'propagate_await_outer(); %PerformMicrotaskCheckpoint();',
  });
  await checkResults(['a']);

  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});

  // Promise reject caught
  InspectorTest.log('Promise reject caught, events on uncaught exception:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  await Protocol.Runtime.evaluate({
    expression:
        'Promise.resolve().then(() => Promise.reject()).catch(() => log.push("d")); // Exception c\n' +
        '%PerformMicrotaskCheckpoint();',
  });
  await checkResults(['d']);

  listenerActive = false;
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});

  // If devtools is turned on in the middle, then catch prediction
  // could be wrong (here, it mispredicts the exception as caught),
  // but shouldn't crash.
  InspectorTest.log('Confused catch prediction:');
  await Protocol.Runtime.evaluate({expression: 'log = []; exception = null;'});
  await Protocol.Runtime.evaluate({
    expression: 'confused(); %PerformMicrotaskCheckpoint();',
  });
  listenerActive = true;
  await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
  await Protocol.Runtime.evaluate({
    expression: 'resolve(); %PerformMicrotaskCheckpoint();',
  });
  listenerActive = false;
  await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  await checkResults(['e']);

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
