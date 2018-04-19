// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that evaluation works when code generation from strings is not allowed.');

Protocol.Debugger.enable();
Protocol.Runtime.enable();

InspectorTest.runAsyncTestSuite([
  async function testEvaluateNotPaused() {
    contextGroup.addScript(`inspector.setAllowCodeGenerationFromStrings(false);
    var global1 = 'Global1';`);
    await Protocol.Debugger.onceScriptParsed();
    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'global1'}));
  },

  async function testEvaluatePaused() {
    contextGroup.addScript(`inspector.setAllowCodeGenerationFromStrings(false);
    var global2 = 'Global2';
    function foo(x) {
      var local = 'Local';
      debugger;
      return local + x;
    }
    foo();`);
    let {params: {callFrames: [{callFrameId}]}} =
        await Protocol.Debugger.oncePaused();

    InspectorTest.logMessage(
        await Protocol.Runtime.evaluate({expression: 'global2'}));
    InspectorTest.logMessage(await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId, expression: 'local'}));
    await Protocol.Debugger.resume();
  }
]);
