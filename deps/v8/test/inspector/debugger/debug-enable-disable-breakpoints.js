// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests enabling and disabling breakpoints with ' +
    'Debugger.setBreakpointsActive.');

contextGroup.addScript(`function f() {a=1;b=2;};`, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let break_point_hit_count = 0;
  Protocol.Debugger.onPaused(async () => {
    break_point_hit_count++;
    await Protocol.Debugger.resume();
  });

  await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 0, columnNumber: 0, url: 'test.js'});

  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(`After initial call: ${break_point_hit_count}`);

  await Protocol.Debugger.setBreakpointsActive({active: false});
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(
      `After setBreakpointsActive(false): ${break_point_hit_count}`);

  await Protocol.Debugger.setBreakpointsActive({active: true});
  await Protocol.Runtime.evaluate({expression: 'f()'});
  InspectorTest.log(
      `After setBreakpointsActive(true): ${break_point_hit_count}`);

  InspectorTest.completeTest();
})();
