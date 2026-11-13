// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const contextGroup = new InspectorTest.ContextGroup();
const session = contextGroup.connect();
const {Protocol} = session;

const source = `
function f(cb) {
  try {
    cb();
  } catch (e) {
    // caught
  }
  return 42;
}

function outer() {
  return f(function executor() {
    debugger;
    throw new Error('boom');
  });
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

(async () => {
  InspectorTest.log('Enabling Debugger:');
  await Protocol.Debugger.enable();

  InspectorTest.log('Calling outer():');
  const evalPromise = Protocol.Runtime.evaluate({expression: 'outer()'});

  const p1 = await Protocol.Debugger.oncePaused();
  InspectorTest.log(`Paused in: ${p1.params.callFrames[0].functionName}`);
  InspectorTest.log('Stepping over:');
  Protocol.Debugger.stepOver();

  const p2 = await Protocol.Debugger.oncePaused();
  InspectorTest.log(`Paused in: ${p2.params.callFrames[0].functionName}`);
  InspectorTest.log('Stepping over throw:');
  Protocol.Debugger.stepOver();

  const p3 = await Protocol.Debugger.oncePaused();
  InspectorTest.log(`Paused in: ${p3.params.callFrames[0].functionName}`);
  await Protocol.Debugger.resume();

  await evalPromise;
  InspectorTest.log('Done');
  InspectorTest.completeTest();
})();
