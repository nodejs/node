// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Don\'t crash when evaluating `this` in class instance initializer');

const script = `
(function foo() {
  function identity(v) {
    return v;
  }

  debugger;

  new class tmpName {
    #thisWorks = this.constructor.name;
    #thisFails = identity(this.constructor.name);
  };
})();
`;

(async () => {
  session.setupScriptMap();
  Protocol.Debugger.enable();

  contextGroup.addScript(script, 0, 0, 'test.js');

  await Protocol.Debugger.oncePaused();

  Protocol.Debugger.stepInto({});
  await Protocol.Debugger.oncePaused();

  Protocol.Debugger.stepInto({});
  const { params: { callFrames } } = await Protocol.Debugger.oncePaused();

  InspectorTest.logMessage(
    await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: callFrames[0].callFrameId, expression: "this" }));
  InspectorTest.logMessage(
    await Protocol.Debugger.evaluateOnCallFrame({ callFrameId: callFrames[0].callFrameId, expression: "this.constructor.name" }));

  await Protocol.Debugger.resume();

  contextGroup.addScript(
      `
function f() {
  function id(x) { return x; }
  new class tmp {
    #foo = id(42);
  };
}
`,
      0, 0, 'test-f.js');

  await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 3,
    columnNumber: 0,
    url: 'test-f.js',
  });

  const pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: 'f();'});
  const {params: {callFrames: callFramesF}} = await pausedPromise;
  await session.logSourceLocation(callFramesF[0].location);
  await Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();
