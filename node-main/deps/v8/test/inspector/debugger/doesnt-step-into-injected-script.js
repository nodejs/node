// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Check that stepInto at then end of the script go to next user script instead InjectedScriptSource.js.');

(async function test() {
  session.setupScriptMap();
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: '(function boo() { setTimeout(() => 239, 0); debugger; })()\n'});
  await waitPauseAndDumpLocation();
  Protocol.Debugger.stepInto();
  await waitPauseAndDumpLocation();
  Protocol.Debugger.stepInto();
  await waitPauseAndDumpLocation();
  Protocol.Debugger.stepInto();
  await waitPauseAndDumpLocation();
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

async function waitPauseAndDumpLocation() {
  var message = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  session.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
