// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=16

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that an OOM break triggered at the entry of a top-level strict ' +
    'eval pauses inside the eval');

Protocol.Debugger.onPaused(async message => {
  InspectorTest.log(`reason: ${message.params.reason}`);
  await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: message.params.callFrames[0].callFrameId,
    expression: 'arr = []; stop = true;'
  });
  Protocol.Debugger.resume();
});

(async function test() {
  await Protocol.Debugger.enable();
  // The loop body only allocates before entering the eval, so the OOM
  // interrupt requested by the near-heap-limit callback fires at the
  // function-entry stack check of the eval, i.e. before the eval context is
  // pushed. The eval is called from the top level of the script so that the
  // current context is the script context, whose ScopeInfo shares its unique
  // ID with the eval scope.
  await Protocol.Runtime.evaluate({
    expression: `
      'use strict';
      let arr = [];
      let stop = false;
      while (!stop) {
        arr.push(new Array(10000).fill(0));
        eval('let x = 1; function inner() { return x; }');
      }
      //# sourceURL=test.js`
  });
  InspectorTest.completeTest();
})();
