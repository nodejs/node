// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.stepOver with enterRanges starting exactly at the function start position.');

contextGroup.addScript(`function outlinedPart() { return 1; }
function main() { debugger; outlinedPart(); return 2; }`);

(async function() {
  Protocol.Debugger.enable();
  const scriptResult = await Protocol.Debugger.onceScriptParsed();
  const scriptId = scriptResult.params.scriptId;

  let evalPromise = Protocol.Runtime.evaluate({expression: 'main()'});

  // Pause on "debugger;"
  let pause = await Protocol.Debugger.oncePaused();

  // Step over "debugger;" to reach "outlinedPart();"
  Protocol.Debugger.stepOver();
  pause = await Protocol.Debugger.oncePaused();

  // The functionLocation of outlinedPart is line 0, column 21.
  // When enterRanges starts exactly at this position, it should enter.
  const enterRanges = [{
    scriptId: scriptId,
    start: {lineNumber: 0, columnNumber: 21},
    end: {lineNumber: 0, columnNumber: 37}
  }];

  Protocol.Debugger.stepOver({enterRanges});
  pause = await Protocol.Debugger.oncePaused();

  let frame = pause.params.callFrames[0];
  InspectorTest.log(`Paused in ${frame.functionName}`);

  await Protocol.Debugger.setSkipAllPauses({skip: true});
  await Protocol.Debugger.resume();
  await evalPromise;

  InspectorTest.completeTest();
})();
