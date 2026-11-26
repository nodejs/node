// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests function and block scopes.');

contextGroup.addScript('let top_level_let = 255;', 0, 0, 'test.js');

(async function test() {
  await Protocol.Debugger.enable();

  async function checkScope(scope, expectations, expectedType) {
    if (scope.type !== expectedType) {
      InspectorTest.log(
          `FAIL: scope type mismatch: expected ${expectedType}, ` +
          `got ${scope.type}`);
    }
    let {result: {result}} = await Protocol.Runtime.getProperties(
        {objectId: scope.object.objectId, ownProperties: true});
    let scopeObject = {};
    for (let prop of result) {
      if (prop.value) {
        scopeObject[prop.name] = prop.value.value;
      }
    }
    for (let [name, expected] of Object.entries(expectations)) {
      let actual = scopeObject[name];
      if (actual !== expected) {
        InspectorTest.log(`FAIL: ${name}: expected ${expected}, got ${actual}`);
      }
    }
    InspectorTest.log(`Scope ${expectedType}: ${JSON.stringify(expectations)}`);
  }

  let listenerDelegate = null;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    let frame = callFrames[0];
    await listenerDelegate(frame);
    await Protocol.Debugger.resume();
  });

  // Case 1
  InspectorTest.log('Case 1:');
  listenerDelegate = async (frame) => {
    InspectorTest.log(`Scope count: ${frame.scopeChain.length}`);
    await checkScope(frame.scopeChain[0], {}, 'local');
    await checkScope(frame.scopeChain[1], {a: 4, b: 5}, 'closure');
    await checkScope(
        frame.scopeChain[2], {z: 22, w: 5, v: 'Capybara'}, 'closure');
    await checkScope(frame.scopeChain[3], {x: 5}, 'closure');
    await checkScope(frame.scopeChain[4], {top_level_let: 255}, 'script');
    await checkScope(frame.scopeChain[5], {}, 'global');
  };

  const case1 = `
  (function F1(x) {
    function F2(y) {
      var z = x + y;
      {
        var w = 5;
        var v = "Capybara";
        var F3 = function(a, b) {
          function F4(p) {
            debugger;
            return p + a + b + z + w + v.length;
          }
          return F4;
        };
        return F3(4, 5);
      }
    }
    return F2(17);
  })(5)();
  `;
  contextGroup.addScript(case1, 0, 0, 'case1.js');
  await InspectorTest.waitForPendingTasks();

  // Case 2
  InspectorTest.log('Case 2:');
  listenerDelegate = async (frame) => {
    InspectorTest.log(`Scope count: ${frame.scopeChain.length}`);
    await checkScope(frame.scopeChain[0], {}, 'local');
    await checkScope(frame.scopeChain[1], {}, 'block');
    await checkScope(frame.scopeChain[2], {l3: 9}, 'block');
    await checkScope(frame.scopeChain[3], {l2: 7}, 'block');
    await checkScope(
        frame.scopeChain[4], {v1: 3, l0: 0, v3: 5, v6: 11}, 'closure');
    await checkScope(frame.scopeChain[5], {top_level_let: 255}, 'script');
    await checkScope(frame.scopeChain[6], {}, 'global');
  };

  const case2 = `
  (function() {
    var v1 = 3;
    var v2 = 4;
    let l0 = 0;
    {
      var v3 = 5;
      let l1 = 6;
      let l2 = 7;
      {
        var v4 = 8;
        let l3 = 9;
        {
          var v5 = "Cat";
          let l4 = 11;
          var v6 = l4;
          return function() {
            debugger;
            return l0 + v1 + v3 + l2 + l3 + v6;
          };
        }
      }
    }
  })()();
  `;
  contextGroup.addScript(case2, 0, 0, 'case2.js');
  await InspectorTest.waitForPendingTasks();

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
