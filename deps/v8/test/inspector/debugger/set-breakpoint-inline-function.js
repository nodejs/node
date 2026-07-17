// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks if we can set a breakpoint on a one-line inline functions.');

session.setupScriptMap();
const testFunction = ` function test() {
    function func(a) {console.log(a);}
    func("hi");
  }
  //# sourceURL=testFunction.js`;

contextGroup.addScript(testFunction);

(async function testSetBreakpoint() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  InspectorTest.log('Setting breakpoint');
  const {result: {locations}} = await Protocol.Debugger.setBreakpointByUrl(
      {lineNumber: 1, url: 'testFunction.js'});
  await session.logBreakLocations(locations);

  Protocol.Runtime.evaluate({expression: 'test()'});
  const {params: {callFrames}} = await Protocol.Debugger.oncePaused();
  InspectorTest.log('Paused on location:');
  session.logCallFrames(callFrames);
  InspectorTest.completeTest();
})();
