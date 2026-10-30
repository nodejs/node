// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --stress-flush-code --flush-bytecode

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-9067.');

session.setupScriptMap();

contextGroup.addScript(
    `function f() {
  (function () {})();
}`,
    0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();
  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 0,
    columnNumber: 0,
    url: 'test.js',
  });

  Protocol.Debugger.onPaused(async message => {
    await session.logSourceLocation(message.params.callFrames[0].location);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Protocol.Runtime.evaluate({expression: 'gc()'});
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
