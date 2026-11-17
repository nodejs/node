// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} = InspectorTest.start(
    'Checks that console.assert() does not break into the debugger after ' +
    'formatting its arguments terminated execution.');

const expression = `
  const arr = [];
  Object.defineProperty(arr, '0', {
    enumerable: true,
    configurable: true,
    get() { debugger; return 1; },
  });
  arr.length = 1;
  console.assert(false, arr);
`;

(async () => {
  await Protocol.Runtime.enable();
  await Protocol.Debugger.enable();
  await Protocol.Debugger.setPauseOnExceptions({state: 'all'});

  let pauses = 0;
  Protocol.Debugger.onPaused(({params: {reason}}) => {
    InspectorTest.log(`paused: ${reason}`);
    if (++pauses === 1) {
      Protocol.Runtime.terminateExecution();
    }
    Protocol.Debugger.resume();
    if (pauses > 1) InspectorTest.quitImmediately();
  });

  await Protocol.Runtime.evaluate({expression});
  InspectorTest.log('evaluate returned');
  InspectorTest.completeTest();
})();
