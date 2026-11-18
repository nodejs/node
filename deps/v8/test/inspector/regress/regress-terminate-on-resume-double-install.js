// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Debugger.resume({terminateOnResume: true}) must not install the termination
// callbacks a second time when a Runtime.terminateExecution request is already
// pending.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Runtime.terminateExecution followed by resume(terminateOnResume)');

(async function test() {
  await Protocol.Debugger.enable();
  const paused = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({expression: `debugger; for (;;) {}`});
  await paused;
  // Do not await: the pause loop keeps running, so the termination is not
  // reported while we are paused.
  Protocol.Runtime.terminateExecution();
  await Protocol.Debugger.resume({terminateOnResume: true});
  InspectorTest.log('Resumed without crashing');
  InspectorTest.completeTest();
})();
