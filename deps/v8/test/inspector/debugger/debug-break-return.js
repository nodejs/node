// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests setting and clearing a breakpoint on an empty function return.');

contextGroup.addScript(`function foo(){}`, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();

  let breakpoint_count = 0;
  let last_source_line = 0;
  let last_source_column = 0;

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    ++breakpoint_count;
    last_source_line = callFrames[0].location.lineNumber + 1;
    last_source_column = callFrames[0].location.columnNumber;
    await Protocol.Debugger.resume();
  });

  // Run without breakpoints.
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  InspectorTest.log(`Without breakpoint: count=${breakpoint_count}`);

  // Run with breakpoint.
  const {result: {breakpointId}} = await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 0, columnNumber: 0, url: 'test.js'});
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  InspectorTest.log(`First call: count=${breakpoint_count}, line=${
      last_source_line}, col=${last_source_column}`);
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  InspectorTest.log(`Second call: count=${breakpoint_count}, line=${
      last_source_line}, col=${last_source_column}`);

  // Run without breakpoints.
  await Protocol.Debugger.removeBreakpoint({breakpointId});
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  InspectorTest.log(`After removeBreakpoint: count=${breakpoint_count}`);

  InspectorTest.completeTest();
})();
