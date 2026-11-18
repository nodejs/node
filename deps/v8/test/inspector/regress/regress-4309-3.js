// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-4309-3.');

contextGroup.addScript(`
function f() {
  with({}) {
    debugger;
  }
}

%PrepareFunctionForOptimization(f);

f();
f();

%OptimizeFunctionOnNextCall(f);
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(message => {
    const scopes = message.params.callFrames[0].scopeChain;
    InspectorTest.log(`Scope types: ${scopes.map(s => s.type).join(', ')}`);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
