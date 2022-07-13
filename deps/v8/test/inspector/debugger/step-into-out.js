// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Tests for stepInto out different places.');

session.setupScriptMap();
Protocol.Debugger.enable();
let stepAction;
Protocol.Debugger.setBlackboxPatterns({patterns: ['expr\.js']});
Protocol.Debugger.onPaused(async ({params:{callFrames:[topFrame]}}) => {
  if (stepAction !== 'resume') {
    await session.logSourceLocation(topFrame.location);
  }
  Protocol.Debugger[stepAction]();
});

InspectorTest.runAsyncTestSuite([
  async function testStepInOutBranch() {
    contextGroup.addScript(`
      function a() { b(false); c(); };
      function b(x) { if (x) { c(); }};
      function c() {};
      a(); b(); c();`);
    stepAction = 'stepInto';
    Protocol.Debugger.pause();
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});
  },

  async function testStepInOutTree() {
    contextGroup.addScript(`
      function a() { b(c(d()), d()); c(d()); d(); };
      function b(x,y) { c(); };
      function c(x) {};
      function d() {};
      a(); b(); c(); d();`);
    stepAction = 'stepInto';
    Protocol.Debugger.pause();
    InspectorTest.log('use stepInto..');
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});

    stepAction = 'stepOver';
    Protocol.Debugger.pause();
    InspectorTest.log('use stepOver..');
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});

    stepAction = 'stepOut';
    Protocol.Debugger.pause();
    InspectorTest.log('use stepOut..');
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});
  },

  async function testStepInOutSimple() {
    contextGroup.addScript(`
      function a() { b(); c(); }
      function b() { c(); }
      function c() {}
      a(); b(); c();`);
    stepAction = 'stepInto';
    Protocol.Debugger.pause();
    InspectorTest.log('use stepInto..');
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});

    stepAction = 'stepOver';
    Protocol.Debugger.pause();
    InspectorTest.log('use stepOver..');
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});

    stepAction = 'stepOut';
    Protocol.Debugger.pause();
    InspectorTest.log('use stepOut..');
    await Protocol.Runtime.evaluate({expression: 'a()\n//# sourceURL=expr.js'});
    stepAction = 'resume';
    await Protocol.Runtime.evaluate({expression: ''});
  }
]);
