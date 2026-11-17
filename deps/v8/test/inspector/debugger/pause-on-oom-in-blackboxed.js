// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=16 --no-concurrent-recompilation

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that an OOM break triggered at the entry of a blackboxed function ' +
    'with a non-blackboxed caller pauses');

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
  await Protocol.Debugger.setBlackboxPatterns({patterns: ['blackboxed.js']});

  await Protocol.Runtime.evaluate({
    expression: `
      function f() { return 1; }
      //# sourceURL=blackboxed.js`
  });

  // The loop body only allocates before calling the blackboxed function, so
  // the OOM interrupt requested by the near-heap-limit callback is likely to
  // fire at the function-entry stack check of `f`.
  await Protocol.Runtime.evaluate({
    expression: `
      let arr = [];
      let stop = false;
      while (!stop) {
        arr.push(1n << (4n * 1024n * 1024n));
        f();
      }
      //# sourceURL=test.js`
  });

  InspectorTest.log('Test finished normally.');
  InspectorTest.completeTest();
})();
