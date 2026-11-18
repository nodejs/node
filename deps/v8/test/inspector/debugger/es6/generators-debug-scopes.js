// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-analyze-environment-liveness

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests scopes in functions and generators.');

const source = `
var global_marker = 7;

function runTestFun(formals_and_body, args, hasContinuation) {
  var fun = Function.apply(null, formals_and_body);
  var res = fun.apply(null, args);
  if (hasContinuation) res();
}

function runTestGen(formals_and_body, args, hasContinuation) {
  var gen = (function*(){}).constructor.apply(null, formals_and_body);
  var res = gen.apply(null, args).next().value;
  if (hasContinuation) res();
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

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
        InspectorTest.log(`Scope ${i} mismatch: expected ${
            expectedScopes[i]}, got ${actualScopes[i]}`);
      }
    }

    const globalScope = callFrame.scopeChain.find(s => s.type === 'global');
    if (globalScope) {
      const {result: {result: gProps}} = await Protocol.Runtime.getProperties(
          {objectId: globalScope.object.objectId, ownProperties: true});
      const marker = gProps.find(p => p.name === 'global_marker');
      if (!marker || marker.value.value !== 7) {
        InspectorTest.log(`global_marker mismatch: ${
            marker ? marker.value.value : 'missing'}`);
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
      if (p.value) {
        obj[p.name] = p.value.type === 'function' ? 'function' : p.value.value;
      } else {
        obj[p.name] = undefined;
      }
    }
    InspectorTest.log(`Scope ${scopeIndex} content: ${JSON.stringify(obj)}`);
    for (const [p, val] of Object.entries(expectedContent)) {
      if (obj[p] !== val) {
        InspectorTest.log(
            `Property "${p}" mismatch: expected ${val}, got ${obj[p]}`);
      }
    }
  }

  const cases = [
    {
      name: 'Local 1',
      formals: ['debugger;'],
      args: [],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {}}]
    },
    {
      name: 'Local 2',
      formals: ['a', 'debugger;'],
      args: [1],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {a: 1}}]
    },
    {
      name: 'Local 3',
      formals: ['a', 'var x = 3; debugger;'],
      args: [1],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {a: 1, x: 3}}]
    },
    {
      name: 'Local 4',
      formals: ['a', 'b', 'var x = 3; var y = 4; debugger;'],
      args: [1, 2],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {a: 1, b: 2, x: 3, y: 4}}]
    },
    {
      name: 'Local 5',
      formals: ['eval(""); debugger;'],
      args: [],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {}}]
    },
    {
      name: 'Local 6',
      formals: ['eval("var i = 5"); debugger;'],
      args: [],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {i: 5}}]
    },
    {
      name: 'Local 7',
      formals: [
        'a', 'b',
        'var x = 3; var y = 4;\n' +
            'eval(\'var i = 5\'); eval (\'var j = 6\');\n' +
            'debugger;'
      ],
      args: [1, 2],
      continuation: false,
      expectedScopes: ['local', 'global'],
      checks: [{scope: 0, expected: {a: 1, b: 2, i: 5, j: 6, x: 3, y: 4}}]
    },
    {
      name: 'With',
      formals: ['with ({a:1}) { with ({b:2}) { debugger; } }'],
      args: [],
      continuation: false,
      expectedScopes: ['with', 'with', 'local', 'global'],
      checks: [{scope: 0, expected: {b: 2}}, {scope: 1, expected: {a: 1}}]
    },
    {
      name: 'Closure 1',
      formals: ['a', 'return function() { debugger; return a; }'],
      args: [1],
      continuation: true,
      expectedScopes: ['local', 'closure', 'global'],
      checks: [{scope: 1, expected: {a: 1}}]
    },
    {
      name: 'The full monty',
      formals: [
        'a', 'b',
        'var x = 3;\n' +
            'var y = 4;\n' +
            'eval(\'var i = 5\');\n' +
            'eval(\'var j = 6\');\n' +
            'function f(a, b) {\n' +
            '  var x = 9;\n' +
            '  var y = 10;\n' +
            '  eval(\'var i = 11\');\n' +
            '  eval(\'var j = 12\');\n' +
            '  with ({j:13}){\n' +
            '    return function() {\n' +
            '      var x = 14;\n' +
            '      with ({a:15}) {\n' +
            '        with ({b:16}) {\n' +
            '          debugger;\n' +
            '          some_global = a;\n' +
            '          return f;\n' +
            '        }\n' +
            '      }\n' +
            '    };\n' +
            '  }\n' +
            '}\n' +
            'return f(a, b);'
      ],
      args: [1, 2],
      continuation: true,
      expectedScopes:
          ['with', 'with', 'local', 'with', 'closure', 'closure', 'global'],
      checks: [
        {scope: 0, expected: {b: 16}}, {scope: 1, expected: {a: 15}},
        {scope: 2, expected: {x: 14}}, {scope: 3, expected: {j: 13}},
        {scope: 4, expected: {a: 1, b: 2, i: 11, j: 12, x: 9, y: 10}}, {
          scope: 5,
          expected: {a: 1, b: 2, f: 'function', i: 5, j: 6, x: 3, y: 4}
        }
      ]
    },
    {
      name: 'Catch block 1',
      formals: ['try { throw \'Exception\'; } catch (e) { debugger; }'],
      args: [],
      continuation: false,
      expectedScopes: ['catch', 'local', 'global'],
      checks: [{scope: 0, expected: {e: 'Exception'}}]
    }
  ];

  for (const c of cases) {
    InspectorTest.log(`--- ${c.name} ---`);
    // Function test
    listenerDelegate = async (callFrame) => {
      InspectorTest.log('Function:');
      await checkScopeChain(callFrame, c.expectedScopes);
      for (const chk of c.checks) {
        await checkScopeContent(callFrame, chk.scope, chk.expected);
      }
    };
    await Protocol.Runtime.evaluate({
      expression: `runTestFun(${JSON.stringify(c.formals)}, ${
          JSON.stringify(c.args)}, ${c.continuation})`
    });

    // Generator test
    listenerDelegate = async (callFrame) => {
      InspectorTest.log('Generator:');
      await checkScopeChain(callFrame, c.expectedScopes);
      for (const chk of c.checks) {
        await checkScopeContent(callFrame, chk.scope, chk.expected);
      }
    };
    await Protocol.Runtime.evaluate({
      expression: `runTestGen(${JSON.stringify(c.formals)}, ${
          JSON.stringify(c.args)}, ${c.continuation})`
    });
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
