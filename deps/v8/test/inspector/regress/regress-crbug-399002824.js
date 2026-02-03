// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, Protocol, contextGroup } =
  InspectorTest.start('Don\'t crash when pausing on the iterator ".next" call');

session.setupScriptMap();
contextGroup.addScript(`
  function *iter() {
    yield 1;
    debugger;
    yield 2;
  }

  function crashMe() {
    for (const e of iter()) {
      () => e;   // Context allocate e.
    }
  }
`);

Protocol.Debugger.enable();

(async () => {
  let pausedPromise = Protocol.Debugger.oncePaused();
  Protocol.Runtime.evaluate({ expression: 'crashMe()' });

  let { params: { callFrames } } = await pausedPromise;
  await session.logSourceLocation(callFrames[1].location);

  Protocol.Debugger.resume();

  InspectorTest.completeTest();
})();
