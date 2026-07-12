// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks Debugger.scheduleStepIntoAsync.');

contextGroup.addScript(`
function testNoScheduledTask() {
  debugger;
  return 42;
}

function testSimple() {
  debugger;
  Promise.resolve().then(v => v * 2);
}

function testNotResolvedPromise() {
  var resolveCallback;
  var p = new Promise(resolve => resolveCallback = resolve);
  debugger;
  p.then(v => v * 2);
  resolveCallback();
}

function testTwoAsyncTasks() {
  debugger;
  Promise.resolve().then(v => v * 2);
  Promise.resolve().then(v => v * 4);
}

function testTwoAsyncTasksWithBreak() {
  debugger;
  Promise.resolve().then(v => v * 2);
  debugger;
  Promise.resolve().then(v => v * 4);
}

function testPromiseAll() {
  debugger;
  Promise.all([ Promise.resolve(), Promise.resolve() ]).then(v => v * 2);
}

function testBlackboxedCreatePromise() {
  debugger;
  createPromise().then(v => v * 2);
}

async function testAsyncFunction() {
  debugger;
  foo();
  await foo();
  foo().then(boo);

  async function foo() {
    return 42;
  }

  function boo() {
  }
}
//# sourceURL=test.js`);

contextGroup.addScript(`

function createPromise() {
  return Promise.resolve().then(v => v * 3).then(v => v * 4);
}

//# sourceURL=framework.js`)

session.setupScriptMap();

Protocol.Debugger.enable();
Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});
InspectorTest.runAsyncTestSuite([
  async function testScheduleErrors() {
    Protocol.Runtime.evaluate({ expression: 'testNoScheduledTask()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testSimple() {
    Protocol.Runtime.evaluate({ expression: 'testSimple()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testNotResolvedPromise() {
    Protocol.Runtime.evaluate({ expression: 'testNotResolvedPromise()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testTwoAsyncTasks() {
    Protocol.Runtime.evaluate({ expression: 'testTwoAsyncTasks()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testTwoTasksAndGoToSecond() {
    Protocol.Runtime.evaluate({ expression: 'testTwoAsyncTasks()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testTwoAsyncTasksWithBreak() {
    Protocol.Runtime.evaluate({ expression: 'testTwoAsyncTasksWithBreak()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testPromiseAll() {
    Protocol.Runtime.evaluate({ expression: 'testPromiseAll()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testWithBlackboxedCode() {
    Protocol.Runtime.evaluate({ expression: 'testBlackboxedCreatePromise()' });
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js'] });
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testWithBlackboxedCode() {
    Protocol.Runtime.evaluate({expression: 'testBlackboxedCreatePromise()'});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.setBlackboxPatterns({patterns: ['framework\.js']});
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    await Protocol.Debugger.resume();
  },

  async function testAsyncFunction() {
    Protocol.Runtime.evaluate({expression: 'testAsyncFunction()'});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOver();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOut();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOut();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepOut();
    await waitPauseAndDumpLocation();
    Protocol.Debugger.stepInto({breakOnAsyncCall: true});
    await waitPauseAndDumpLocation();

    await Protocol.Debugger.resume();
  }
]);

async function waitPauseAndDumpLocation() {
  var {params: {callFrames}} =
      await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  await session.logSourceLocation(callFrames[0].location);
}
