// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --cache=code --no-debug-code

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Test that code caching functions works with inspector active.');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    let callFrames = message.params.callFrames;
    let {result} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: callFrames[0].callFrameId, expression: 'f'});
    let properties = await Protocol.Runtime.getProperties({
      objectId: result.objectId,
      accessorPropertiesOnly: false,
      generatePreview: true
    });
    let locationProp = properties.result.internalProperties.find(
        prop => prop.name === '[[FunctionLocation]]');
    await Protocol.Debugger.setBreakpoint({location: locationProp.value.value});
    await Protocol.Debugger.resume();
  });

  const script = `
    try { } catch(e) { }
    try { try { } catch (e) { } } catch(e) { }
    function setBreakPoint(f) {
      debugger;
    }
    (function() {
      setBreakPoint(function() {});
    })();

    var a = 1;
    a += a;
  `;

  contextGroup.addScript(script, 0, 0, 'test.js');

  await Protocol.Debugger.disable();

  let {result: {result}} = await Protocol.Runtime.evaluate({expression: 'a'});
  InspectorTest.log(`a = ${result.value}`);
  InspectorTest.completeTest();
})();
