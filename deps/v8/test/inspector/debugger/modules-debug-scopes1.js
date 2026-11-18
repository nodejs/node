// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noanalyze-environment-liveness

let {session, contextGroup, Protocol} = InspectorTest.start(
    'Tests scope chain inspection and scope objects in modules.');

const source = `
function local_1() {
  debugger;
}
local_1();

function local_2(a) {
  debugger;
}
local_2(1);

function local_3(a) {
  var x = 3;
  debugger;
}
local_3(1);

function local_4(a, b) {
  var x = 3;
  var y = 4;
  debugger;
}
local_4(1, 2);

function local_5() {
  eval('');
  debugger;
}
local_5();

function local_6() {
  eval('var i = 5');
  debugger;
}
local_6();

function local_7(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  debugger;
}
local_7(1, 2);

function closure_1(a) {
  function f() {
    debugger;
    return a;
  }
  return f;
}
closure_1(1)();

function closure_2(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    return a + x;
  }
  return f;
}
closure_2(1, 2)();

function closure_3(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    return a + b + x + y;
  }
  return f;
}
closure_3(1, 2)();

function closure_4(a, b) {
  var x = a + 2;
  var y = b + 2;
  function f() {
    debugger;
    if (f) {
      return a + b + x + y;
    }
  }
  return f;
}
closure_4(1, 2)();

function closure_5(a, b) {
  var x = 3;
  var y = 4;
  function f() {
    eval('');
    debugger;
    return 1;
  }
  return f;
}
closure_5(1, 2)();

let some_global;
function closure_6(a, b) {
  function f(a, b) {
    var x = 3;
    var y = 4;
    return function() {
      var x = 3;
      var y = 4;
      debugger;
      some_global = a;
      return f;
    };
  }
  return f(a, b);
}
closure_6(1, 2)();

function closure_7(a, b) {
  var x = 3;
  var y = 4;
  eval('var i = 5');
  eval('var j = 6');
  function f(a, b) {
    var x = 3;
    var y = 4;
    eval('var i = 5');
    eval('var j = 6');
    return function() {
      debugger;
      some_global = a;
      return f;
    };
  }
  return f(a, b);
}
closure_7(1, 2)();

function closure_8() {
  (function inner(x) {
    debugger;
  })(2);
}
closure_8();

let closure_9 = Function(' \\
  eval("var y = 1;"); \\
  eval("var z = 1;"); \\
  (function inner(x) { \\
    y++; \\
    z++; \\
    debugger; \\
  })(2); \\
');
closure_9();

debugger;

function catch_block_1() {
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
}
catch_block_1();

function catch_block_3() {
  eval("var y = 78;");
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
}
catch_block_3();

try {
  throw 'Exception';
} catch (e) {
  debugger;
}

try {
  throw 'Exception';
} catch (e) {
  (function(x) {
    debugger;
  })(2);
}

function catch_block_7() {
  %OptimizeFunctionOnNextCall(catch_block_7);
  try {
    throw 'Exception';
  } catch (e) {
    debugger;
  }
}
%PrepareFunctionForOptimization(catch_block_7);
catch_block_7();

(function() {
  "use strict";
  class C1 {
    m() {
      debugger;
    }
  }
  new C1().m();
})();

var code1 = "function f() {        \\n" +
            "  var a = 1;          \\n" +
            "  function b() {      \\n" +
            "    debugger;         \\n" +
            "    return a + 1;     \\n" +
            "  }                   \\n" +
            "  b();                \\n" +
            "}                     \\n" +
            "f();                  \\n";
eval(code1);

var code3 = "function for_statement() {         \\n" +
            "  for (let i = 0; i < 1; i++) {    \\n" +
            "    debugger;                      \\n" +
            "  }                                \\n" +
            "}                                  \\n" +
            "for_statement();                   \\n";
eval(code3);

var code4 = "function for_statement() {         \\n" +
            "  for (let i = 0; i < 1; i++) {    \\n" +
            "    let j;                         \\n" +
            "    debugger;                      \\n" +
            "  }                                \\n" +
            "}                                  \\n" +
            "for_statement();                   \\n";
eval(code4);

var code5 = "function for_each_statement() {    \\n" +
            "  for (let i of [0]) {             \\n" +
            "    debugger;                      \\n" +
            "  }                                \\n" +
            "}                                  \\n" +
            "for_each_statement();              \\n";
eval(code5);

var code6 = "function for_each_statement() {    \\n" +
            "  for (let i of [0]) {             \\n" +
            "    let j;                         \\n" +
            "    debugger;                      \\n" +
            "  }                                \\n" +
            "}                                  \\n" +
            "for_each_statement();              \\n";
eval(code6);

var code7 = "function for_each_statement() {    \\n" +
            "  var i;                           \\n" +
            "  for (i of [0]) {                 \\n" +
            "    debugger;                      \\n" +
            "  }                                \\n" +
            "}                                  \\n" +
            "for_each_statement();              \\n";
eval(code7);

var code8 = "function for_each_statement() {    \\n" +
            "  var i;                           \\n" +
            "  for (i of [0]) {                 \\n" +
            "    let j;                         \\n" +
            "    debugger;                      \\n" +
            "  }                                \\n" +
            "}                                  \\n" +
            "for_each_statement();              \\n";
eval(code8);
`;

(async function test() {
  await Protocol.Debugger.enable();

  async function checkScopeChain(scopes, callFrame) {
    const scopeChain = callFrame.scopeChain;
    if (scopes.length !== scopeChain.length) {
      throw new Error(
          `Scope count mismatch: expected ${scopes.length}, ` +
          `got ${scopeChain.length} (${
              scopeChain.map(s => s.type).join(', ')})`);
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

  function checkScopeChainPositions(positions, callFrame) {
    if (positions.length > callFrame.scopeChain.length) {
      throw new Error(
          `Positions length ${positions.length} > scopeChain length ${
              callFrame.scopeChain.length}`);
    }
    for (let i = 0; i < positions.length; i++) {
      const pos = positions[i];
      if (pos && (pos.start !== undefined || pos.end !== undefined)) {
        InspectorTest.log(
            `Checking position.start = ${pos.start}, .end = ${pos.end}`);
      }
    }
  }

  const tests = [
    {
      name: 'Local 1',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({}, 0, frame);
      }
    },
    {
      name: 'Local 2',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({a: 1}, 0, frame);
      }
    },
    {
      name: 'Local 3',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({a: 1, x: 3}, 0, frame);
      }
    },
    {
      name: 'Local 4',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({a: 1, b: 2, x: 3, y: 4}, 0, frame);
      }
    },
    {
      name: 'Local 5',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({}, 0, frame);
      }
    },
    {
      name: 'Local 6',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({}, 0, frame);
      }
    },
    {
      name: 'Local 7',
      check: async (frame) => {
        await checkScopeChain(['local', 'module', 'global'], frame);
        await checkScopeContent({a: 1, b: 2, x: 3, y: 4}, 0, frame);
      }
    },
    {
      name: 'Closure 1',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        await checkScopeContent({a: 1}, 1, frame);
      }
    },
    {
      name: 'Closure 2',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        await checkScopeContent({a: 1, x: 3}, 1, frame);
      }
    },
    {
      name: 'Closure 3',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        await checkScopeContent({a: 1, b: 2, x: 3, y: 4}, 1, frame);
      }
    },
    {
      name: 'Closure 4',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        await checkScopeContent(
            {a: 1, b: 2, x: 3, y: 4, f: undefined}, 1, frame);
      }
    },
    {
      name: 'Closure 5',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        await checkScopeContent(
            {a: 1, b: 2, x: 3, y: 4, f: undefined}, 1, frame);
      }
    },
    {
      name: 'Closure 6',
      check: async (frame) => {
        await checkScopeChain(
            ['local', 'closure', 'closure', 'module', 'global'], frame);
        await checkScopeContent({a: 1}, 1, frame);
        await checkScopeContent({f: undefined}, 2, frame);
      }
    },
    {
      name: 'Closure 7',
      check: async (frame) => {
        await checkScopeChain(
            ['local', 'closure', 'closure', 'module', 'global'], frame);
        await checkScopeContent({}, 0, frame);
        await checkScopeContent({a: 1, b: 2, x: 3, y: 4}, 1, frame);
        await checkScopeContent(
            {a: 1, b: 2, x: 3, y: 4, f: undefined}, 2, frame);
      }
    },
    {
      name: 'Closure 8',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        await checkScopeContent({x: 2}, 0, frame);
        await checkScopeContent({}, 1, frame);
      }
    },
    {
      name: 'Closure 9',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'global'], frame);
      }
    },
    {
      name: 'Global',
      check: async (frame) => {
        await checkScopeChain(['module', 'global'], frame);
      }
    },
    {
      name: 'Catch block 1',
      check: async (frame) => {
        await checkScopeChain(['catch', 'local', 'module', 'global'], frame);
        await checkScopeContent({e: 'Exception'}, 0, frame);
      }
    },
    {
      name: 'Catch block 3',
      check: async (frame) => {
        await checkScopeChain(['catch', 'local', 'module', 'global'], frame);
        await checkScopeContent({e: 'Exception'}, 0, frame);
        await checkScopeContent({}, 1, frame);
      }
    },
    {
      name: 'Catch block 5',
      check: async (frame) => {
        await checkScopeChain(['catch', 'module', 'global'], frame);
        await checkScopeContent({e: 'Exception'}, 0, frame);
      }
    },
    {
      name: 'Catch block 6',
      check: async (frame) => {
        await checkScopeChain(['local', 'catch', 'module', 'global'], frame);
        await checkScopeContent({x: 2}, 0, frame);
        await checkScopeContent({e: 'Exception'}, 1, frame);
      }
    },
    {
      name: 'Catch block 7',
      check: async (frame) => {
        await checkScopeChain(['catch', 'local', 'module', 'global'], frame);
        await checkScopeContent({e: 'Exception'}, 0, frame);
      }
    },
    {
      name: 'Classes and methods 1',
      check: async (frame) => {
        await checkScopeChain(
            ['local', 'block', 'closure', 'module', 'global'], frame);
        await checkScopeContent({}, 3, frame);
      }
    },
    {
      name: 'Scope positions',
      check: async (frame) => {
        checkScopeChainPositions(
            [{start: 58, end: 118}, {start: 10, end: 162}], frame);
      }
    },
    {
      name: 'Scope positions in for statement',
      check: async (frame) => {
        await checkScopeChain(
            ['block', 'local', 'closure', 'module', 'global'], frame);
        checkScopeChainPositions(
            [{start: 42, end: 111}, {start: 22, end: 145}], frame);
      }
    },
    {
      name: 'Scope positions in for statement with lexical block',
      check: async (frame) => {
        await checkScopeChain(
            ['block', 'block', 'local', 'closure', 'module', 'global'], frame);
        checkScopeChainPositions(
            [
              {start: 66, end: 147}, {start: 42, end: 147},
              {start: 22, end: 181}
            ],
            frame);
      }
    },
    {
      name: 'Scope positions in lexical for each statement',
      check: async (frame) => {
        await checkScopeChain(
            ['block', 'local', 'closure', 'module', 'global'], frame);
        checkScopeChainPositions(
            [{start: 55, end: 111}, {start: 27, end: 145}], frame);
      }
    },
    {
      name: 'Scope positions in lexical for each statement with lexical block',
      check: async (frame) => {
        await checkScopeChain(
            ['block', 'block', 'local', 'closure', 'module', 'global'], frame);
        checkScopeChainPositions(
            [
              {start: 57, end: 147}, {start: 55, end: 147},
              {start: 27, end: 181}
            ],
            frame);
      }
    },
    {
      name: 'Scope positions in non-lexical for each statement',
      check: async (frame) => {
        await checkScopeChain(['local', 'closure', 'module', 'global'], frame);
        checkScopeChainPositions([{start: 27, end: 181}], frame);
      }
    },
    {
      name:
          'Scope positions in non-lexical for each statement with lexical block',
      check: async (frame) => {
        await checkScopeChain(
            ['block', 'local', 'closure', 'module', 'global'], frame);
        checkScopeChainPositions(
            [{start: 89, end: 183}, {start: 27, end: 217}], frame);
      }
    },
  ];

  let resolveDone;
  let donePromise = new Promise(resolve => resolveDone = resolve);

  let break_count = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const frame = callFrames[0];
    const currentTest = tests[break_count++];
    try {
      if (currentTest) {
        await currentTest.check(frame);
        InspectorTest.log(currentTest.name + ' passed');
      } else {
        InspectorTest.log('Unexpected break: ' + break_count);
      }
    } catch (e) {
      InspectorTest.log(
          (currentTest ? currentTest.name : 'Break ' + break_count) +
          ' failed: ' + e + '\n' + e.stack);
    }
    if (break_count === tests.length) {
      resolveDone();
    }
    await Protocol.Debugger.resume();
  });

  contextGroup.addModule(source, 'modules-debug-scopes1.mjs');

  await donePromise;
  InspectorTest.log(`All ${tests.length} tests completed successfully (${
      break_count} breaks).`);
  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
