// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {session, contextGroup, Protocol} =
    InspectorTest.start('Tests block scopes in closures.');

contextGroup.addScript(
    `
var global_marker = 7;

function closure_1(a) {
  var x = 2;
  let y = 3;
  if (true) {
    let z = 4;
    function f() {
      debugger;
      return a + x + y + z;
    };
    return f;
  }
}
`,
    0, 0, 'test.js');

(async function() {
  await Protocol.Debugger.enable();

  Protocol.Debugger.onPaused(async message => {
    const callFrame = message.params.callFrames[0];
    const expectedScopeTypes = ['local', 'block', 'closure', 'global'];
    const actualScopeTypes = callFrame.scopeChain.map(s => s.type);
    InspectorTest.log(`Scope chain types: ${JSON.stringify(actualScopeTypes)}`);

    for (let i = 0; i < expectedScopeTypes.length; ++i) {
      if (actualScopeTypes[i] !== expectedScopeTypes[i]) {
        InspectorTest.log(
            `Scope ${i} type mismatch: expected ${expectedScopeTypes[i]}, ` +
            `got ${actualScopeTypes[i]}`);
      }
    }

    async function getPropertiesMap(objectId) {
      const {result: {result: props}} = await Protocol.Runtime.getProperties({
        objectId: objectId,
        ownProperties: true,
      });
      const propMap = {};
      for (const elem of props) {
        if (elem.value) {
          propMap[elem.name] =
              elem.value.type === 'undefined' ? undefined : elem.value.value;
        }
      }
      return propMap;
    }

    const localProps =
        await getPropertiesMap(callFrame.scopeChain[0].object.objectId);
    delete localProps['this'];
    delete localProps['arguments'];
    InspectorTest.log(`Scope 0 (local): ${JSON.stringify(localProps)}`);

    const blockProps =
        await getPropertiesMap(callFrame.scopeChain[1].object.objectId);
    delete blockProps['this'];
    delete blockProps['arguments'];
    InspectorTest.log(`Scope 1 (block): ${JSON.stringify(blockProps)}`);

    const closureProps =
        await getPropertiesMap(callFrame.scopeChain[2].object.objectId);
    delete closureProps['this'];
    delete closureProps['arguments'];
    InspectorTest.log(`Scope 2 (closure): ${JSON.stringify(closureProps)}`);

    const globalProps =
        await getPropertiesMap(callFrame.scopeChain[3].object.objectId);
    InspectorTest.log(
        `Scope 3 (global) marker matches: ${globalProps.global_marker === 7}`);

    await Protocol.Debugger.resume();
  });

  await Protocol.Runtime.evaluate({
    expression: 'closure_1(1)();',
  });

  InspectorTest.completeTest();
})();
