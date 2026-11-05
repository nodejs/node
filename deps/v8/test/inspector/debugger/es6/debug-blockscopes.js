// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noanalyze-environment-liveness --turbofan

let {session, contextGroup, Protocol} =
    InspectorTest.start('Test block scopes in V8 debugger.');

const scriptSource = `
"use strict";

let script_marker = 42;
var global_marker = 7;

function local_block_1() {
  {
    debugger;
  }
}

function local_2(a) {
  {
    debugger;
  }
}

function local_3(a) {
  let x = 3;
  debugger;
}

function local_4(a, b) {
  let x = 3;
  let y = 4;
  debugger;
}

function local_5(a) {
  {
    let x = 5;
    debugger;
  }
}

function local_6(a) {
  {
    let x = 6;
    let y = 7;
    debugger;
  }
}

function local_7(a) {
  {
    {
      let x = 8;
      debugger;
    }
  }
}

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

function for_loop_1() {
  for (let x in {y:undefined}) {
    debugger;
  }
}

function for_loop_2() {
  for (let x in {y:undefined}) {
    let x = 3;
    debugger;
  }
}

function for_loop_3() {
  for (let x = 3; x < 4; ++x) {
    debugger;
  }
}

function for_loop_4() {
  for (let x = 3; x < 4; ++x) {
    let x = 5;
    debugger;
  }
}

function for_loop_5() {
  for (let x = 3, y = 5; x < 4; ++x) {
    debugger;
  }
}

function uninitialized_1() {
  {
    debugger;
    let x = 1;
  }
}

function shadowing_1() {
  let i = 0;
  {
    let i = 5;
    debugger;
  }
  if (i !== 0) throw new Error('Expected 0, got ' + i);
}

function shadowing_2() {
  let i = 0;
  {
    let j = 5;
    debugger;
  }
  if (i !== 0) throw new Error('Expected 0, got ' + i);
}
`;

contextGroup.addScript(scriptSource, 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  let listenerDelegate = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    if (listenerDelegate) {
      await listenerDelegate(callFrames[0]);
    }
    await Protocol.Debugger.resume();
  });

  async function checkScopeChain(callFrame, expectedScopes) {
    const actualScopes = callFrame.scopeChain.map(s => s.type);
    InspectorTest.log(`Scope chain: ${actualScopes.join(', ')}`);
    for (let i = 0; i < expectedScopes.length; i++) {
      if (actualScopes[i] !== expectedScopes[i]) {
        InspectorTest.log(
            `Scope ${i} mismatch: expected ${expectedScopes[i]}, ` +
            `got ${actualScopes[i]}`);
      }
    }

    const globalScope = callFrame.scopeChain.find(s => s.type === 'global');
    if (globalScope) {
      const {result: {result: gProps}} = await Protocol.Runtime.getProperties(
          {objectId: globalScope.object.objectId, ownProperties: true});
      const marker = gProps.find(p => p.name === 'global_marker');
      if (!marker || marker.value.value !== 7) {
        InspectorTest.log(
            `global_marker mismatch: ` +
            `${marker ? marker.value.value : 'missing'}`);
      }
    }
  }

  async function checkScopeContent(callFrame, scopeIndex, expectedContent) {
    const scope = callFrame.scopeChain[scopeIndex];
    const {result: {result: props}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    const filtered = props.filter(p => p.name !== 'arguments' && p.name !== '');
    const obj = {};
    for (const p of filtered.sort((a, b) => a.name.localeCompare(b.name))) {
      obj[p.name] = p.value ? p.value.value : undefined;
    }
    InspectorTest.log(`Scope ${scopeIndex} content: ${JSON.stringify(obj)}`);
    for (const [p, val] of Object.entries(expectedContent)) {
      if (obj[p] !== val) {
        InspectorTest.log(
            `Property "${p}" mismatch: expected ${val}, got ${obj[p]}`);
      }
    }
    const expectedCount = Object.keys(expectedContent).length;
    if (filtered.length !== expectedCount) {
      InspectorTest.log(
          `Scope ${scopeIndex} count mismatch: expected ${expectedCount}, ` +
          `got ${filtered.length}`);
    }
  }

  async function runCase(name, expr, delegate) {
    InspectorTest.log(`--- ${name} ---`);
    listenerDelegate = delegate;
    await Protocol.Runtime.evaluate({expression: expr});
  }

  await runCase('Local block 1', 'local_block_1()', async (frame) => {
    await checkScopeChain(frame, ['local', 'script', 'global']);
    await checkScopeContent(frame, 0, {});
  });

  await runCase('Local 2', 'local_2(1)', async (frame) => {
    await checkScopeChain(frame, ['local', 'script', 'global']);
    await checkScopeContent(frame, 0, {a: 1});
  });

  await runCase('Local 3', 'local_3(1)', async (frame) => {
    await checkScopeChain(frame, ['local', 'script', 'global']);
    await checkScopeContent(frame, 0, {a: 1, x: 3});
  });

  await runCase('Local 4', 'local_4(1, 2)', async (frame) => {
    await checkScopeChain(frame, ['local', 'script', 'global']);
    await checkScopeContent(frame, 0, {a: 1, b: 2, x: 3, y: 4});
  });

  await runCase('Local 5', 'local_5(1)', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 5});
    await checkScopeContent(frame, 1, {a: 1});
  });

  await runCase('Local 6', 'local_6(1)', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 6, y: 7});
    await checkScopeContent(frame, 1, {a: 1});
  });

  await runCase('Local 7', 'local_7(1)', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 8});
    await checkScopeContent(frame, 1, {a: 1});
  });

  await runCase('Closure 1', 'closure_1(1)()', async (frame) => {
    await checkScopeChain(
        frame, ['local', 'block', 'closure', 'script', 'global']);
    await checkScopeContent(frame, 0, {});
    await checkScopeContent(frame, 2, {a: 1, x: 2, y: 3});
  });

  await runCase('For loop 1', 'for_loop_1()', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 'y'});
  });

  await runCase('For loop 2', 'for_loop_2()', async (frame) => {
    await checkScopeChain(
        frame, ['block', 'block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 3});
    await checkScopeContent(frame, 1, {x: 'y'});
  });

  await runCase('For loop 3', 'for_loop_3()', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 3});
    await checkScopeContent(frame, 1, {});
  });

  await runCase('For loop 4', 'for_loop_4()', async (frame) => {
    await checkScopeChain(
        frame, ['block', 'block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 5});
    await checkScopeContent(frame, 1, {x: 3});
    await checkScopeContent(frame, 2, {});
  });

  await runCase('For loop 5', 'for_loop_5()', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    await checkScopeContent(frame, 0, {x: 3, y: 5});
    await checkScopeContent(frame, 1, {});
  });

  await runCase('Uninitialized 1', 'uninitialized_1()', async (frame) => {
    await checkScopeChain(frame, ['block', 'local', 'script', 'global']);
    const scope = frame.scopeChain[0];
    const {result: {result: props}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    const propX = props.find(p => p.name === 'x');
    InspectorTest.log(`x is present in scope: ${!!propX}`);
    InspectorTest.log(`x value is unavailable: ${propX.value === undefined}`);
  });

  await runCase('Block scopes shadowing 1', 'shadowing_1()', async (frame) => {
    const {result: {result: val}} = await Protocol.Debugger.evaluateOnCallFrame(
        {callFrameId: frame.callFrameId, expression: 'i'});
    InspectorTest.log(`Evaluated i: ${val.value}`);
  });

  await runCase('Block scopes shadowing 2', 'shadowing_2()', async (frame) => {
    const {result: {result: valI}} =
        await Protocol.Debugger.evaluateOnCallFrame(
            {callFrameId: frame.callFrameId, expression: 'i'});
    InspectorTest.log(`Evaluated i: ${valI.value}`);
    const {result: {result: valJ}} =
        await Protocol.Debugger.evaluateOnCallFrame(
            {callFrameId: frame.callFrameId, expression: 'j'});
    InspectorTest.log(`Evaluated j: ${valJ.value}`);
  });

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
