// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks stepping with more then one context group.');

(async function test() {
  InspectorTest.setupScriptMap();
  await Protocol.Debugger.enable();
  let contextGroupId = utils.createContextGroup();
  await Protocol.Debugger.enable({}, contextGroupId);
  Protocol.Runtime.evaluate({expression: 'debugger'});
  Protocol.Runtime.evaluate({expression: 'setTimeout(() => { debugger }, 0)'}, contextGroupId);
  Protocol.Runtime.evaluate({expression: 'setTimeout(() => 42, 0)'});
  await waitPauseAndDumpLocation();
  Protocol.Debugger.stepOver();
  await Protocol.Debugger.oncePaused();
  Protocol.Debugger.stepOver();
  await waitPauseAndDumpLocation();
  await Protocol.Debugger.disable({}, contextGroupId);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

async function waitPauseAndDumpLocation() {
  var message = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  await InspectorTest.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
