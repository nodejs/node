// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=16 --no-concurrent-recompilation

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that an OOM break pauses even if all frames are blackboxed');

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
  await Protocol.Debugger.setBlackboxPatterns({patterns: ['blackboxed']});

  await Protocol.Runtime.evaluate({
    expression: `
      function f() { return 1; }
      //# sourceURL=blackboxed-f.js`
  });

  // Both the loop and `f` are blackboxed, so the OOM interrupt fires with
  // only blackboxed frames on the stack, either at the function-entry stack
  // check of `f` or at the loop back-edge.
  await Protocol.Runtime.evaluate({
    expression: `
      let arr = [];
      let stop = false;
      while (!stop) {
        arr.push(1n << (4n * 1024n * 1024n));
        f();
      }
      //# sourceURL=blackboxed-loop.js`
  });

  InspectorTest.log('Test finished normally.');
  InspectorTest.completeTest();
})();
