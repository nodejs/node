// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
  'Checks that it is possible to disable agent on pause.');

contextGroup.addScript(`
function foo() {
  return 42;
}`, 0, 0, 'test.js');

(async function test() {
  Protocol.Debugger.enable();
  InspectorTest.log('set breakpoint, call foo, wait for pause..');
  let {result:{breakpointId}} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2, url: 'test.js'});
  const finished = Protocol.Runtime.evaluate({
    expression: 'foo()//# sourceURL=expr.js'
  });
  const {params:{callFrames:[topFrame]}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log('disable agent..');
  Protocol.Debugger.disable();
  await finished;
  InspectorTest.log('call finished');
  InspectorTest.completeTest();
})();
