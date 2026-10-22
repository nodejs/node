// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests Debugger.Scope.emptyReason.');

contextGroup.addScript(`
function empty() {
  debugger;
}

function allUnavailable() {
  debugger;
  let a = 1;
  const b = 2;
  return a + b;
}

function blockAllUnavailable(x) {
  {
    debugger;
    let a = 1;
    let b = 2;
    return a + b + x;
  }
}

function someAvailable() {
  let a = 1;
  debugger;
  let b = 2;
  return a + b;
}

function closure() {
  let outer = 42;
  return function inner() {
    debugger;
    return outer;
  };
}
`);

async function dumpScopeChain(expression) {
  const pausedPromise = Protocol.Debugger.oncePaused();
  const evaluatePromise = Protocol.Runtime.evaluate({expression});
  const {params: {callFrames: [{functionName, scopeChain}]}} =
      await pausedPromise;
  InspectorTest.log(`Paused in ${functionName}:`);
  for (const {type, emptyReason} of scopeChain) {
    // Script and global scopes depend on the test harness.
    if (type === 'script' || type === 'global') continue;
    InspectorTest.log(`  ${type}: ${emptyReason ?? '<absent>'}`);
  }
  await Protocol.Debugger.resume();
  await evaluatePromise;
}

InspectorTest.runAsyncTestSuite([
  async function testEmptyReason() {
    await Protocol.Debugger.enable();
    await dumpScopeChain('empty()');
    await dumpScopeChain('allUnavailable()');
    await dumpScopeChain('blockAllUnavailable(1)');
    await dumpScopeChain('someAvailable()');
    await dumpScopeChain('closure()()');
    await Protocol.Debugger.disable();
  },
]);
