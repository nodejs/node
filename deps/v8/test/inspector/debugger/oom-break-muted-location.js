// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --max-old-space-size=16 --no-concurrent-recompilation

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests that an OOM break is not dropped at a muted conditional breakpoint location.');

Protocol.Debugger.onPaused(async message => {
  InspectorTest.log(`reason: ${message.params.reason}`);
  await Protocol.Debugger.evaluateOnCallFrame({
    callFrameId: message.params.callFrames[0].callFrameId,
    expression: 'arr = []; stop = true;',
  });
  Protocol.Debugger.resume();
});

(async function test() {
  await Protocol.Debugger.enable();

  const source = [
    'var arr = [];',
    'var stop = false;',
    'function run() {',
    '  while (!stop) {',
    '    arr.push(new Array(10000).fill(0));',
    '  }',
    '}',
    '//# sourceURL=test.js',
  ].join('\n');

  await Protocol.Runtime.evaluate({expression: source});

  await Protocol.Debugger.setBreakpointByUrl({
    url: 'test.js',
    lineNumber: 4,
    columnNumber: 0,
    condition: 'false',
  });

  await Protocol.Runtime.evaluate({expression: 'run()'});

  InspectorTest.log('Test finished normally.');
  InspectorTest.completeTest();
})();
