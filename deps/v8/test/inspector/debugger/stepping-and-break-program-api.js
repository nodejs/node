// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks that stepping is cleared after breakProgram.');

InspectorTest.addScript(`
function callBreakProgram() {
  debugger;
  breakProgram('reason', '');
}`);

InspectorTest.setupScriptMap();
(async function test() {
  Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'callBreakProgram();'});
  // Should break at this debugger statement, not at end of callBreakProgram.
  Protocol.Runtime.evaluate({expression: 'setTimeout(\'debugger;\', 0);'});
  await waitPauseAndDumpLocation();
  Protocol.Debugger.stepOver();
  await waitPauseAndDumpLocation();
  Protocol.Debugger.stepOver();
  await waitPauseAndDumpLocation();
  Protocol.Debugger.resume();
  await waitPauseAndDumpLocation();
  InspectorTest.completeTest();
})();

async function waitPauseAndDumpLocation() {
  var message = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  InspectorTest.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
