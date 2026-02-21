// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} =
    InspectorTest.start('Checks that Debugger.restartFrame returns an error');

session.setupScriptMap();

(async function test() {
  Protocol.Debugger.enable();
  const evalPromise = Protocol.Runtime.evaluate({
    expression: 'function foo() { debugger; }; foo();'
  });
  InspectorTest.log('Paused at debugger:');
  const { params: { callFrames: before } } =
      await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(before[0].location);
  const result = await Protocol.Debugger.restartFrame({ callFrameId: before[0].callFrameId });
  InspectorTest.log('restartFrame result:');
  InspectorTest.logMessage(result);
  await Promise.all([Protocol.Debugger.resume(), evalPromise]);
  InspectorTest.completeTest();
})()
