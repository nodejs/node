// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests regress-4309-2.');

contextGroup.addScript(`
function f() {
  eval('var i = 5');
  debugger;
}

%PrepareFunctionForOptimization(f);

f();
f();

%OptimizeFunctionOnNextCall(f);
`);

(async function test() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const objectId = message.params.callFrames[0].scopeChain[0].object.objectId;
    const {result: {result}} = await Protocol.Runtime.getProperties({
      objectId,
      ownProperties: true,
    });
    const prop = result.find(p => p.name === 'i');
    InspectorTest.log(`i: ${prop.value.value}`);
    Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({expression: 'f()'});
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
