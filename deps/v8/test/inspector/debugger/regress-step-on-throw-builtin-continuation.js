// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests stepping over a throw in a callback from an inlined builtin.');

const source = `
function f(arr, cb) {
  try {
    arr.forEach(cb);
  } catch (e) {
  }
}

const arr = [1, 2, 3];
const cb1 = (x) => x + 1;
const cb2 = (x) => x + 2;

for (let i = 0; i < 5000; i++) {
  f(arr, cb1);
  f(arr, cb2);
}

function testRun() {
  f(arr, () => {
    debugger;
    throw new Error('boom');
  });
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    InspectorTest.log(
        'Paused at: ' + (callFrames[0].functionName || '(anonymous)'));
    await Protocol.Debugger.stepOver();
  });

  await Protocol.Runtime.evaluate({expression: 'testRun();'});
  InspectorTest.log('Finished evaluation.');

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
