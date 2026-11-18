// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const { session, contextGroup, Protocol } = InspectorTest.start(
    'Checks that setting a breakpoint on a function with no breakable positions does not crash.');

(async () => {
  await Protocol.Debugger.enable();

  contextGroup.addScript(`
    function loop() {
      while (true) {}
    }
  `, 0, 0, 'loop.js');

  InspectorTest.log('Setting breakpoint on loop function...');
  const {result, error} = await Protocol.Debugger.setBreakpointByUrl({
    lineNumber: 2,
    url: 'loop.js'
  });
  if (error) {
    InspectorTest.log(`Error: ${error.message}`);
  } else {
    InspectorTest.log(`Breakpoint result: ${JSON.stringify(result)}`);
  }

  InspectorTest.completeTest();
})();
