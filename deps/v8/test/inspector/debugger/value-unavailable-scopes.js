// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --experimental-value-unavailable

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test scopes with unavailable values');

contextGroup.addScript(`
function tdz() {
  debugger;
  const x = 2 ?? y + x;
  let y = x + 1;
  return y;
}

function opt() {
  function optimizedOut(x, stop) {
    let y = x + 1;
    let z = y + 1;
    if (stop) {
      debugger;
    }
    return z;
  }

  %PrepareFunctionForOptimization(optimizedOut);
  optimizedOut(1, false);
  optimizedOut(2, false);
  %OptimizeFunctionOnNextCall(optimizedOut);
  optimizedOut(3, false);

  return optimizedOut(1, true);
}
`);

Protocol.Debugger.onPaused(async ({params: {callFrames: [{scopeChain}]}}) => {
  for (const scope of scopeChain) {
    if (scope.type !== 'local') continue;
    const {result: {result: variables}} =
        await Protocol.Runtime.getProperties({objectId: scope.object.objectId});
    for (const variable of variables) {
      if (variable.name !== 'y') continue;
      if ('value' in variable || 'get' in variable || 'set' in variable) {
        InspectorTest.log(
            'FAIL: variable y was expected to be reported as <value_unavailable>');
      } else {
        InspectorTest.log(
            'variable y correctly reported as <value_unavailable>');
      }
    }
  }
  await Protocol.Debugger.resume();
});

InspectorTest.runAsyncTestSuite([
  async function testTemporalDeadZone() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    await Protocol.Runtime.evaluate({expression: 'tdz()'});
    await Promise.all([
      Protocol.Runtime.disable(),
      Protocol.Debugger.disable(),
    ]);
  },

  async function testOptimizedOut() {
    await Promise.all([
      Protocol.Runtime.enable(),
      Protocol.Debugger.enable(),
    ]);
    await Protocol.Runtime.evaluate({expression: 'opt()'});
    await Promise.all([
      Protocol.Runtime.disable(),
      Protocol.Debugger.disable(),
    ]);
  },
]);
