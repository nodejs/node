// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Tests for break on exception.');

Protocol.Debugger.enable();
Protocol.Debugger.onPaused(({params:{data}}) => {
  InspectorTest.log('paused on exception:');
  InspectorTest.logMessage(data);
  Protocol.Debugger.resume();
});

contextGroup.addInlineScript(`
function throws() {
  throw 1;
}
function caught() {
  try {
    throws();
  } catch (e) {
  }
}
function uncaught() {
  throws();
}
function uncaughtFinally() {
  try {
    throws();
  } finally {
  }
}
function caughtFinally() {
  L: try {
    throws();
  } finally {
    break L;
  }
}
`, 'test.js');

InspectorTest.runAsyncTestSuite([
  async function testPauseOnInitialState() {
    await evaluate('caught()');
    await evaluate('uncaught()');
    await evaluate('uncaughtFinally()');
    await evaluate('caughtFinally()');
  },

  async function testPauseOnExceptionOff() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
    await evaluate('caught()');
    await evaluate('uncaught()');
    await evaluate('uncaughtFinally()');
    await evaluate('caughtFinally()');
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testBreakOnCaughtException() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'caught'});
    await evaluate('caught()');
    await evaluate('uncaught()');
    await evaluate('uncaughtFinally()');
    await evaluate('caughtFinally()');
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testBreakOnUncaughtException() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
    await evaluate('caught()');
    await evaluate('uncaught()');
    await evaluate('uncaughtFinally()');
    await evaluate('caughtFinally()');
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testBreakOnAll() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
    await evaluate('caught()');
    await evaluate('uncaught()');
    await evaluate('uncaughtFinally()');
    await evaluate('caughtFinally()');
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testTryFinallyOriginalMessage() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
    await evaluate(`
      try {
        throw 1;
      } finally {
      }
    `);
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testPromiseRejectedByCallback() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
    await evaluate(`
      function fun() { eval("throw 'rejection';") }
      var p = new Promise(function(res, rej) { fun(); res(); });
      var r;
      p.then(() => { r = 'resolved'; }, (e) => { r = 'rejected' + e; });
    `);
    InspectorTest.log('r = ');
    InspectorTest.logMessage((await Protocol.Runtime.evaluate({
      expression: 'r'
    })).result.result);
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testBreakOnExceptionAfterReconnect() {
    contextGroup.addInlineScript('function f() { throw new Error(); }');
    await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
    await evaluate('f()');
    InspectorTest.log('\nreconnect..');
    session.reconnect();
    await evaluate('f()');
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testBreakOnExceptionInSilentMode(next) {
    await Protocol.Debugger.setPauseOnExceptions({ state: "all" });
    InspectorTest.log(`evaluate 'caught()'`);
    await Protocol.Runtime.evaluate({expression: 'caught()', silent: true});
    InspectorTest.log(`evaluate 'uncaught()'`);
    await Protocol.Runtime.evaluate({expression: 'uncaught()', silent: true});
    InspectorTest.log(`evaluate 'uncaughtFinally()'`);
    await Protocol.Runtime.evaluate({
      expression: 'uncaughtFinally()',
      silent: true
    });
    InspectorTest.log(`evaluate 'caughtFinally()'`);
    await Protocol.Runtime.evaluate({
      xpression: 'caughtFinally()',
      silent: true
    });
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  }
]);

async function evaluate(expression) {
  InspectorTest.log(`\nevaluate '${expression}'..`);
  contextGroup.addInlineScript(expression);
  await InspectorTest.waitForPendingTasks();
}
