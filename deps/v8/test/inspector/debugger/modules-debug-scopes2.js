// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests scope chain inspection and scope content for modules.');

const source = `
debugger;

let local_let = 1;
var local_var = 2;
export let exported_let = 3;
export var exported_var = 4;
import {exported_let as imported_let} from "modules-debug-scopes2.mjs";
import {exported_var as imported_var} from "modules-debug-scopes2.mjs";

debugger;

local_let += 10;
local_var += 10;
exported_let += 10;
exported_var += 10;

debugger;
`;

(async function test() {
  await Protocol.Debugger.enable();

  async function checkScopeChain(scopes, callFrame) {
    const scopeChain = callFrame.scopeChain;
    if (scopes.length !== scopeChain.length) {
      throw new Error(
          `Scope count mismatch: expected ${scopes.length}, ` +
          `got ${scopeChain.length}`);
    }
    for (let i = 0; i < scopes.length; i++) {
      const scope = scopeChain[i];
      if (scopes[i] !== scope.type) {
        throw new Error(
            `Scope ${i} type mismatch: expected ${scopes[i]}, ` +
            `got ${scope.type}`);
      }
    }
  }

  async function checkScopeContent(minimum_content, number, callFrame) {
    const scope = callFrame.scopeChain[number];
    const {result: {result: props}} = await Protocol.Runtime.getProperties({
      objectId: scope.object.objectId,
      ownProperties: true,
    });
    const propMap = new Map();
    for (const elem of props) {
      let val = undefined;
      if (elem.value) {
        val = elem.value.type === 'undefined' ? undefined : elem.value.value;
      }
      propMap.set(elem.name, val);
    }

    let minimum_count = 0;
    for (const p in minimum_content) {
      if (!propMap.has(p)) {
        throw new Error(`property ${p} not found in scope`);
      }
      if (propMap.get(p) !== minimum_content[p]) {
        throw new Error(
            `property ${p} has unexpected value: ${propMap.get(p)} vs ` +
            minimum_content[p]);
      }
      minimum_count++;
    }

    let scope_size = props.length;
    if (propMap.has('arguments')) scope_size--;
    if (propMap.has('this')) scope_size--;
    if (propMap.has('')) {
      throw new Error('temporary empty property found');
    }
    if (scope_size < minimum_count) {
      throw new Error(
          `scope_size ${scope_size} < minimum_count ${minimum_count}`);
    }
  }

  let resolveDone;
  let donePromise = new Promise(resolve => resolveDone = resolve);

  let pass = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    try {
      const frame = callFrames[0];
      await checkScopeChain(['module', 'global'], frame);

      if (pass === 0) {
        await checkScopeContent(
            {exported_var: undefined, imported_var: undefined}, 0, frame);
        InspectorTest.log('Pass 0 passed');
      } else if (pass === 1) {
        await checkScopeContent(
            {
              exported_let: 3,
              exported_var: 4,
              imported_let: 3,
              imported_var: 4
            },
            0, frame);
        InspectorTest.log('Pass 1 passed');
      } else if (pass === 2) {
        await checkScopeContent(
            {
              exported_let: 13,
              exported_var: 14,
              imported_let: 13,
              imported_var: 14
            },
            0, frame);
        InspectorTest.log('Pass 2 passed');
        resolveDone();
      }

      pass++;
    } catch (e) {
      InspectorTest.log('Error in onPaused: ' + e + '\n' + e.stack);
    }
    await Protocol.Debugger.resume();
  });

  contextGroup.addModule(source, 'modules-debug-scopes2.mjs');

  await donePromise;
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
