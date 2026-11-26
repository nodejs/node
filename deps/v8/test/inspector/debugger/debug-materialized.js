// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests inspecting materialized arguments and objects in optimized frames.');

contextGroup.addScript(`
function dbg(x) {
  debugger;
}

function foo() {
  arguments[0];
  dbg();
}

function bar() {
  var t = { a : 1 };
  dbg();
  return t.a;
}
`);

async function getLocalScopeProperties(frame) {
  for (const scope of frame.scopeChain) {
    if (scope.type === 'local') {
      const {result: {result}} = await Protocol.Runtime.getProperties({
        objectId: scope.object.objectId,
        ownProperties: true,
      });
      return result;
    }
  }
  return [];
}

(async function test() {
  await Protocol.Debugger.enable();

  let inspectFrames = false;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (inspectFrames) {
      for (let i = 0; i < callFrames.length; i++) {
        const locals = await getLocalScopeProperties(callFrames[i]);
        for (const local of locals) {
          if (local.name === 'arguments') continue;
          if (local.value === undefined || local.value.type === 'object') {
            InspectorTest.log(
                local.name + ' is unavailable or materialized object');
          } else {
            const val = local.value.type === 'undefined' ? undefined :
                                                           local.value.value;
            InspectorTest.log('\'' + local.name + '\' = ' + val);
          }
        }
      }
    }
    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({
    expression: `
      %PrepareFunctionForOptimization(foo);
      %PrepareFunctionForOptimization(bar);
      foo(1);
      foo(1);
      bar(1);
      bar(1);
      %OptimizeFunctionOnNextCall(foo);
      %OptimizeFunctionOnNextCall(bar);
    `,
  });

  inspectFrames = true;
  await Protocol.Runtime.evaluate({expression: 'foo(1); bar(1);'});
  InspectorTest.completeTest();
})();
