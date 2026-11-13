// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests function scope chains and scope objects while paused.');

contextGroup.addScript('let topLevelLet = null;');

async function getScopeObject(objectId) {
  const {result: {result}} = await Protocol.Runtime.getProperties({
    objectId,
    ownProperties: true,
  });
  const obj = {};
  for (const elem of result) {
    if (elem.value) {
      obj[elem.name] =
          elem.value.type === 'undefined' ? undefined : elem.value.value;
    }
  }
  return obj;
}

async function dumpScopes(frame, expectedKeysPerScope) {
  InspectorTest.log('scopeCount: ' + frame.scopeChain.length);
  for (let i = 0; i < frame.scopeChain.length; i++) {
    const scope = frame.scopeChain[i];
    const obj = await getScopeObject(scope.object.objectId);
    const keys = expectedKeysPerScope[i] || [];
    const subset = {};
    for (const k of keys) {
      subset[k] = k in obj ? obj[k] : '<missing>';
    }
    InspectorTest.log(
        '  scope ' + i + ' (' + scope.type + '): ' + JSON.stringify(subset));
  }
}

(async function test() {
  await Protocol.Debugger.enable();

  let currentExpectedKeys = [];
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    await dumpScopes(callFrames[0], currentExpectedKeys);
    await Protocol.Debugger.resume();
  });

  InspectorTest.log('Case 1');
  currentExpectedKeys = [[], ['a', 'b'], ['w', 'v'], ['z'], ['x'], [], []];
  await Protocol.Runtime.evaluate({
    expression: `
      (function F1(x) {
        function F2(y) {
          var z = x + y;
          with ({w: 5, v: "Capybara"}) {
            var F3 = function(a, b) {
              function F4(p) {
                debugger;
                return p + a + b + z + w + v.length;
              }
              return F4;
            }
            return F3(4, 5);
          }
        }
        return F2(17);
      })(5)();
    `,
  });

  InspectorTest.log('Case 2');
  currentExpectedKeys = [[], [], []];
  await Protocol.Runtime.evaluate({
    expression: '(function() { debugger; return 5; })();',
  });

  InspectorTest.log('Case 3');
  currentExpectedKeys =
      [[], ['visible2'], ['invisible2'], ['visible1', 'invisible1'], [], []];
  await Protocol.Runtime.evaluate({
    expression: `
      (function F1(invisible_parameter) {
        var invisible1 = 1;
        var visible1 = 10;
        return (function F2() {
          var invisible2 = 2;
          return (function F3() {
            var visible2 = 20;
            return (function () { debugger; return visible1 + visible2; });
          })();
        })();
      })(5)();
    `,
  });

  InspectorTest.log('Case 4');
  currentExpectedKeys = [[], ['e2'], ['e1'], [], [], []];
  await Protocol.Runtime.evaluate({
    expression: `
      (function One() {
        try {
          throw "I'm error 1";
        } catch (e1) {
          try {
            throw "I'm error 2";
          } catch (e2) {
            return function GetError() {
              debugger;
              return e1 + e2;
            };
          }
        }
      })()();
    `,
  });

  InspectorTest.log('Case 5');
  currentExpectedKeys = [[], ['p4', 'p6'], ['p1'], [], []];
  await Protocol.Runtime.evaluate({
    expression: `
      (function Raz(p1, p2) {
        var p3 = p1 + p2;
        return (function() {
          var p4 = 20;
          var p5 = 21;
          var p6 = 22;
          return eval("(function(p7){ debugger; return p1 + p4 + p6 + p7})");
        })();
      })(1,2)();
    `,
  });

  InspectorTest.completeTest();
})();
