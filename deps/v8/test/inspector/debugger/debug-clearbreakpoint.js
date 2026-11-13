// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests clearing a breakpoint from inside the pause handler.');

contextGroup.addScript(`function g() {};`, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 0, columnNumber: 0, url: 'test.js'});

  let pauseCount = 0;
  Protocol.Debugger.onPaused(async () => {
    ++pauseCount;
    InspectorTest.log(`Paused #${pauseCount}, removing breakpoint...`);
    await Protocol.Debugger.removeBreakpoint({breakpointId});
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'g()'});
  await Protocol.Runtime.evaluate({expression: 'g()'});
  InspectorTest.log(`Total pauses: ${pauseCount}`);

  InspectorTest.completeTest();
})();
