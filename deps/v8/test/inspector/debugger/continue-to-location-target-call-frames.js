// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Check that continue-to-location works with different strategies.');

contextGroup.addScript(`
async function asyncFact(n) {
  if (n == 0) return 1;
  let r = n * await asyncFact(n - 1);
  console.log(r);
  return r;
}

function fact(n) {
  if (n == 0) return 1;
  let r = n * fact(n - 1);
  console.log(r);
  return r;
}

function topLevel() {
  eval(` + '`' + `
  var a = 1;
  var b = 2;
  fact(3);
  console.log(a + b);
  ` + '`' + `);
}

//# sourceURL=test.js`, 7, 26);

session.setupScriptMap();
InspectorTest.runAsyncTestSuite([
  async function testAwaitAny() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'asyncFact(4)//# sourceURL=expr.js'});
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    let message = await pausedAndDumpStack();
    let location = message.params.callFrames[0].location;
    location.lineNumber = 11;
    Protocol.Debugger.continueToLocation({location, targetCallFrames: 'any'});
    await pausedAndDumpStack();
    Protocol.Debugger.disable();
  },

  async function testAwaitCurrent() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'asyncFact(4)//# sourceURL=expr.js'});
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    let message = await pausedAndDumpStack();
    let location = message.params.callFrames[0].location;
    location.lineNumber = 11;
    Protocol.Debugger.continueToLocation({location, targetCallFrames: 'current'});
    await pausedAndDumpStack();
    await Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  },

  async function testAny() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'fact(4)//# sourceURL=expr.js'});
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    let message = await pausedAndDumpStack();
    let location = message.params.callFrames[0].location;
    location.lineNumber = 18;
    Protocol.Debugger.continueToLocation({location, targetCallFrames: 'any'});
    await pausedAndDumpStack();
    Protocol.Debugger.disable();
  },

  async function testCurrent() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'fact(4)//# sourceURL=expr.js'});
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    let message = await pausedAndDumpStack();
    let location = message.params.callFrames[0].location;
    location.lineNumber = 18;
    Protocol.Debugger.continueToLocation({location, targetCallFrames: 'current'});
    await pausedAndDumpStack();
    await Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  },

  async function testTopLevelAny() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'topLevel()//# sourceURL=expr.js'});
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    let message = await pausedAndDumpStack();
    let location = message.params.callFrames[0].location;
    location.lineNumber = 4;
    Protocol.Debugger.continueToLocation({location, targetCallFrames: 'any'});
    await pausedAndDumpStack();
    Protocol.Debugger.disable();
  },

  async function testTopLevelCurrent() {
    Protocol.Debugger.enable();
    Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 });
    Protocol.Debugger.pause();
    Protocol.Runtime.evaluate({expression: 'topLevel()//# sourceURL=expr.js'});
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    await pausedAndDumpStack();
    Protocol.Debugger.stepInto();
    let message = await pausedAndDumpStack();
    let location = message.params.callFrames[0].location;
    location.lineNumber = 4;
    Protocol.Debugger.continueToLocation({location, targetCallFrames: 'current'});
    await pausedAndDumpStack();
    await Protocol.Debugger.resume();
    Protocol.Debugger.disable();
  }
]);

async function pausedAndDumpStack() {
  let message = await Protocol.Debugger.oncePaused();
  session.logCallFrames(message.params.callFrames);
  session.logAsyncStackTrace(message.params.asyncStackTrace);
  InspectorTest.log('');
  return message;
}
