// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Tests that Debugger.setSkipAllPauses skips breaks and does not block resumed notifications');
session.setupScriptMap();

(async function test() {
  await Protocol.Debugger.enable();
  Protocol.Runtime.evaluate({expression: 'debugger;'});
  await waitForPause();
  await Protocol.Debugger.resume();

  await Protocol.Debugger.setSkipAllPauses({skip: true});
  await Protocol.Runtime.evaluate({expression: 'debugger'});

  await Protocol.Debugger.setSkipAllPauses({skip: false});
  Protocol.Runtime.evaluate({expression: 'debugger'});
  await waitForPause();
  Protocol.Debugger.setSkipAllPauses({skip: true});
  Protocol.Debugger.resume();
  await Protocol.Debugger.onceResumed();

  InspectorTest.completeTest();
})();

async function waitForPause() {
  var message = await Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  session.logSourceLocation(message.params.callFrames[0].location);
}
