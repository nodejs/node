// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

InspectorTest.log('Checks stepping with more then one context group.');

var contextGroup1 = new InspectorTest.ContextGroup();
var session1 = contextGroup1.connect();
session1.setupScriptMap();

let contextGroup2 = new InspectorTest.ContextGroup();
let session2 = contextGroup2.connect();
session2.setupScriptMap();

(async function test() {
  await session1.Protocol.Debugger.enable();
  await session2.Protocol.Debugger.enable({});
  session1.Protocol.Runtime.evaluate({expression: 'debugger'});
  session2.Protocol.Runtime.evaluate({expression: 'setTimeout(() => { debugger }, 0)'});
  session1.Protocol.Runtime.evaluate({expression: 'setTimeout(() => 42, 0)'});
  await waitPauseAndDumpLocation(session1);
  session1.Protocol.Debugger.stepOver();
  await session1.Protocol.Debugger.oncePaused();
  session1.Protocol.Debugger.stepOver();
  await waitPauseAndDumpLocation(session1);
  await session2.Protocol.Debugger.disable({});
  await session1.Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();

async function waitPauseAndDumpLocation(session) {
  var message = await session.Protocol.Debugger.oncePaused();
  InspectorTest.log('paused at:');
  await session.logSourceLocation(message.params.callFrames[0].location);
  return message;
}
