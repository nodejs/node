// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks that it is possbible to remove breakpoint when paused at it.');

contextGroup.addScript(`
function foo() {
  return 42;
}`, 0, 0, 'test.js');

(async function test() {
  session.setupScriptMap();
  Protocol.Debugger.enable();
  InspectorTest.log('set breakpoint, call foo, wait for pause..');
  let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2, url: 'test.js'});
  Protocol.Runtime.evaluate({expression: 'foo()//# sourceURL=expr.js'});
  const {params:{callFrames:[topFrame]}} = await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(topFrame.location);
  InspectorTest.log('remove breakpoint, call foo again..');
  Protocol.Debugger.removeBreakpoint({breakpointId});
  await Protocol.Debugger.resume();
  await Protocol.Runtime.evaluate({expression: 'foo()'});
  InspectorTest.log('call finished without pause');
  InspectorTest.completeTest();
})();
