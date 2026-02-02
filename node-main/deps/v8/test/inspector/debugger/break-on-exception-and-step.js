// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Tests for break on exception and stepping.');

session.setupScriptMap();
Protocol.Debugger.enable();
Protocol.Debugger.setBlackboxPatterns({patterns: ['expr\.js']});
Protocol.Debugger.onPaused(async ({params:{callFrames:[topFrame], data}}) => {
  if (data) {
    InspectorTest.log('paused on exception:');
    InspectorTest.logMessage(data);
  }
  await session.logSourceLocation(topFrame.location);
  Protocol.Debugger.stepInto();
});

contextGroup.addScript(`
function a() { n(); };
function b() { c(); };
function c() { n(); };
function d() { x = 1; try { e(); } catch(x) { x = 2; } };
function e() { n(); };
function f() { x = 1; try { g(); } catch(x) { x = 2; } };
function g() { h(); };
function h() { x = 1; throw 1; };
`);

InspectorTest.runAsyncTestSuite([
  async function testStepThrougA() {
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
  },

  async function testStepThrougB() {
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'b()\n//# sourceURL=expr.js'});
  },

  async function testStepThrougD() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'd()\n//# sourceURL=expr.js'});
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testStepThrougDWithBreakOnAllExceptions() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'd()\n//# sourceURL=expr.js'});
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testStepThrougF() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'uncaught'});
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'f()\n//# sourceURL=expr.js'});
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  },

  async function testStepThrougFWithBreakOnAllExceptions() {
    await Protocol.Debugger.setPauseOnExceptions({state: 'all'});
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'f()\n//# sourceURL=expr.js'});
    await Protocol.Debugger.setPauseOnExceptions({state: 'none'});
  }
]);
