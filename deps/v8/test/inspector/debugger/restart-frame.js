// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, Protocol} =
    InspectorTest.start('Checks that Debugger.restartFrame works');

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
  InspectorTest.log('Call restart and dump location of restart:');
  const { result: { callFrames: restart }} =
      await Protocol.Debugger.restartFrame({
          callFrameId: before[0].callFrameId
      });
  await session.logSourceLocation(restart[0].location);
  InspectorTest.log('Location after restart:');
  Protocol.Debugger.resume();
  const { params: { callFrames: after } } =
      await Protocol.Debugger.oncePaused();
  await session.logSourceLocation(after[0].location);
  Protocol.Debugger.resume();
  await evalPromise;
  InspectorTest.completeTest();
})()
