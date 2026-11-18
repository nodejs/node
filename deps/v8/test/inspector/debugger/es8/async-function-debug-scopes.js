// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests async function debug scopes.');

const source = `
var global_marker = 7;

async function thrower() { throw 'Exception'; }

let currentCase = null;

async function runTest(name, func, args, continuation) {
  currentCase = name;
  var result;
  if (typeof func === "object")
    result = await func.method.apply(func, args);
  else
    result = await func.apply(null, args);

  if (typeof continuation === "function") {
    await continuation(result);
  }
}

async function runTests() {
  // Simple
  await runTest(
      "(AsyncFunctionExpression) Local 1",
      async function() { debugger; }, []);

  await runTest(
      "(AsyncFunctionExpression) Local 1 --- resume normal",
      async function() { let z = await 2; debugger; }, []);

  await runTest(
      "(AsyncFunctionExpression) Local 1 --- resume throw",
      async function() { let q = await 1;
                         try { let z = await thrower(); }
                         catch (e) { debugger; } }, []);

  // Simple With Parameter
  await runTest(
      "(AsyncFunctionExpression) Local 2",
      async function(a) { debugger; }, [1]);

  await runTest(
      "(AsyncFunctionExpression) Local 2 --- resume normal",
      async function(a) { let z = await 2; debugger; }, [1]);

  await runTest(
      "(AsyncFunctionExpression) Local 2 --- resume throw",
      async function(a) { let z = await 2;
                          try { await thrower(); } catch (e) { debugger; } }, [1]);

  // Simple With Parameter and Variable
  await runTest(
      "(AsyncFunctionExpression) Local 3",
      async function(a) { var b = 2; debugger; }, [1]);

  await runTest(
      "(AsyncFunctionExpression) Local 3 --- resume normal",
      async function(a) { let y = await 3; var b = 2; let z = await 4;
                          debugger; }, [1]);

  await runTest(
      "(AsyncFunctionExpression) Local 3 --- resume throw",
      async function(a) { let y = await 3;
                          try { var b = 2; let z = await thrower(); }
                          catch (e) { debugger; } }, [1]);

  // Local scope with parameters and local variables.
  await runTest(
      "(AsyncFunctionExpression) Local 4",
      async function(a, b) { var x = 3; var y = 4; debugger; }, [1, 2]);

  await runTest(
      "(AsyncFunctionExpression) Local 4 --- resume normal",
      async function(a, b) { let q = await 5; var x = 3; var y = 4;
                             let r = await 6; debugger; }, [1, 2]);

  await runTest(
      "(AsyncFunctionExpression) Local 4 --- resume throw",
      async function(a, b) { let q = await 5; var x = 3; var y = 4;
                             try { let r = await thrower(); }
                             catch (e) { debugger; } }, [1, 2]);

  // Empty local scope with use of eval.
  await runTest(
      "(AsyncFunctionExpression) Local 5",
      async function() { eval(""); debugger; }, []);

  await runTest(
      "(AsyncFunctionExpression) Local 5 --- resume normal",
      async function() { let x = await 1; eval(""); let y = await 2;
                         debugger; }, []);

  await runTest(
      "(AsyncFunctionExpression) Local 5 --- resume throw",
      async function() { let x = await 1; eval("");
                         try { let y = await thrower(); }
                         catch (e) { debugger; } }, []);

  // Local introducing local variable using eval.
  await runTest(
      "(AsyncFunctionExpression) Local 6",
      async function() { eval("var i = 5"); debugger; }, []);

  await runTest(
      "(AsyncFunctionExpression) Local 6 --- resume normal",
      async function() { let x = await 1; eval("var i = 5"); let y = await 2;
                         debugger; }, []);

  await runTest(
      "(AsyncFunctionExpression) Local 6 --- resume throw",
      async function() { let x = await 1; eval("var i = 5");
                         try { let y = await thrower(); }
                         catch (e) { debugger; } }, []);

  // Local scope with parameters, local variables and local variable introduced
  // using eval.
  await runTest(
      "(AsyncFunctionExpression) Local 7",
      async function(a, b) { var x = 3; var y = 4;
                             eval("var i = 5;"); eval("var j = 6");
                             debugger; }, [1, 2]);

  await runTest(
      "(AsyncFunctionExpression) Local 7 --- resume normal",
      async function(a, b) { let z = await 7; var x = 3; var y = 4;
                             eval("var i = 5;"); eval("var j = 6");
                             let q = await 8;
                             debugger; }, [1, 2]);

  await runTest(
      "(AsyncFunctionExpression) Local 7 --- resume throw",
      async function(a, b) { let z = await 7; var x = 3; var y = 4;
                             eval("var i = 5;"); eval("var j = 6");
                             try { let q = await thrower(); }
                             catch (e) { debugger; } }, [1, 2]);

  // Nested empty with blocks.
  await runTest(
      "(AsyncFunctionExpression) With",
      async function() { with ({}) { with ({}) { debugger; } } }, []);

  await runTest(
      "(AsyncFunctionExpression) With --- resume normal",
      async function() { let x = await 1; with ({}) { with ({}) {
                         let y = await 2; debugger; } } }, []);

  await runTest(
      "(AsyncFunctionExpression) With --- resume throw",
      async function() { let x = await 1; with ({}) { with ({}) {
                         try { let y = await thrower(); }
                         catch (e) { debugger; } } } }, []);

  // Simple closure formed by returning an inner function referring the outer
  // functions arguments.
  await runTest(
      "(AsyncFunctionExpression) Closure 1",
      async function(a) { return function() { debugger; return a; } }, [1],
      result => result());

  await runTest(
      "(AsyncFunctionExpression) Closure 1 --- resume normal",
      async function(a) { let x = await 2;
                          return function() { debugger; return a; } }, [1],
      result => result());

  await runTest(
      "(AsyncFunctionExpression) Closure 1 --- resume throw",
      async function(a) { let x = await 2;
                          return async function() {
                              try { await thrower(); }
                              catch (e) { debugger; } return a; }; }, [1],
      result => result());

  await runTest(
      "(AsyncFunctionExpression) Catch block 1",
      async function() { try { throw 'Exception'; } catch (e) { debugger; } }, []);

  await runTest(
      "(AsyncFunctionExpression) Catch block 1 --- resume normal",
      async function() {
        let x = await 1;
        try { throw 'Exception'; } catch (e) { let y = await 2; debugger; } }, []);

  await runTest(
      "(AsyncFunctionExpression) Catch block 1 --- resume throw",
      async function() {
        let x = await 1;
        try { throw 'Exception!'; } catch (e) {
          try { let y = await thrower(); } catch (e) { debugger; } } }, []);
}
`;

contextGroup.addScript(source, 0, 0, 'test.js');

const expectations = [
  {
    name: '(AsyncFunctionExpression) Local 1',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {}},
  },
  {
    name: '(AsyncFunctionExpression) Local 1 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {z: 2}},
  },
  {
    name: '(AsyncFunctionExpression) Local 1 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {q: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Local 2',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Local 2 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, z: 2}},
  },
  {
    name: '(AsyncFunctionExpression) Local 2 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {a: 1, z: 2}},
  },
  {
    name: '(AsyncFunctionExpression) Local 3',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, b: 2}},
  },
  {
    name: '(AsyncFunctionExpression) Local 3 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, b: 2, y: 3, z: 4}},
  },
  {
    name: '(AsyncFunctionExpression) Local 3 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {a: 1, b: 2, y: 3}},
  },
  {
    name: '(AsyncFunctionExpression) Local 4',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, b: 2, x: 3, y: 4}},
  },
  {
    name: '(AsyncFunctionExpression) Local 4 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, b: 2, x: 3, y: 4, q: 5, r: 6}},
  },
  {
    name: '(AsyncFunctionExpression) Local 4 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {a: 1, b: 2, x: 3, y: 4, q: 5}},
  },
  {
    name: '(AsyncFunctionExpression) Local 5',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {}},
  },
  {
    name: '(AsyncFunctionExpression) Local 5 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {x: 1, y: 2}},
  },
  {
    name: '(AsyncFunctionExpression) Local 5 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {x: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Local 6',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {i: 5}},
  },
  {
    name: '(AsyncFunctionExpression) Local 6 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {i: 5, x: 1, y: 2}},
  },
  {
    name: '(AsyncFunctionExpression) Local 6 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {i: 5, x: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Local 7',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, b: 2, x: 3, y: 4, i: 5, j: 6}},
  },
  {
    name: '(AsyncFunctionExpression) Local 7 --- resume normal',
    scopes: ['local', 'closure', 'script', 'global'],
    contents: {0: {a: 1, b: 2, x: 3, y: 4, i: 5, j: 6, z: 7, q: 8}},
  },
  {
    name: '(AsyncFunctionExpression) Local 7 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}},
  },
  {
    name: '(AsyncFunctionExpression) With',
    scopes: ['with', 'with', 'local', 'closure', 'script', 'global'],
    contents: {0: {}, 1: {}},
  },
  {
    name: '(AsyncFunctionExpression) With --- resume normal',
    scopes: ['block', 'with', 'with', 'local', 'closure', 'script', 'global'],
    contents: {0: {y: 2}, 1: {}, 2: {}, 3: {x: 1}},
  },
  {
    name: '(AsyncFunctionExpression) With --- resume throw',
    scopes: ['catch', 'with', 'with', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {}, 2: {}, 3: {x: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Closure 1',
    scopes: ['local', 'closure', 'closure', 'script', 'global'],
    contents: {1: {a: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Closure 1 --- resume normal',
    scopes: ['local', 'closure', 'closure', 'script', 'global'],
    contents: {1: {a: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Closure 1 --- resume throw',
    scopes: ['catch', 'local', 'closure', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 2: {a: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Catch block 1',
    scopes: ['catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}},
  },
  {
    name: '(AsyncFunctionExpression) Catch block 1 --- resume normal',
    scopes: ['block', 'catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {y: 2}, 1: {e: 'Exception'}, 2: {x: 1}},
  },
  {
    name: '(AsyncFunctionExpression) Catch block 1 --- resume throw',
    scopes: ['catch', 'catch', 'local', 'closure', 'script', 'global'],
    contents: {0: {e: 'Exception'}, 1: {e: 'Exception!'}, 2: {x: 1}},
  },
];

async function checkScopeChain(callFrame, expectedScopes) {
  const actualScopes = callFrame.scopeChain.map(s => s.type);
  if (actualScopes.length !== expectedScopes.length) {
    InspectorTest.log(
        `Scope count mismatch: expected ${expectedScopes.length}, ` +
        `got ${actualScopes.length} (${actualScopes.join(', ')})`);
    return;
  }
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
          `global_marker mismatch: ${marker ? marker.value.value : 'missing'}`);
    }
  }
}

async function checkScopeContent(callFrame, scopeIndex, expectedContent) {
  const scope = callFrame.scopeChain[scopeIndex];
  const {result: {result: props}} = await Protocol.Runtime.getProperties(
      {objectId: scope.object.objectId, ownProperties: true});
  const propMap = new Map();
  for (const elem of props) {
    let val = undefined;
    if (elem.value) {
      val = elem.value.type === 'undefined' ? undefined : elem.value.value;
    }
    propMap.set(elem.name, val);
  }

  let count = 0;
  for (const p in expectedContent) {
    if (!propMap.has(p)) {
      InspectorTest.log(`property ${p} not found in scope ${scopeIndex}`);
    } else if (propMap.get(p) !== expectedContent[p]) {
      InspectorTest.log(
          `property ${p} has unexpected value: expected ${
              expectedContent[p]}, ` +
          `got ${propMap.get(p)}`);
    }
    count++;
  }

  let scopeSize = props.length;
  if (propMap.has('arguments')) scopeSize--;
  if (propMap.has('')) scopeSize--;
  if (scopeSize < count) {
    InspectorTest.log(
        `scopeSize ${scopeSize} < count ${count} for scope ${scopeIndex}`);
  }
}

(async function test() {
  await Protocol.Debugger.enable();

  let caseIndex = 0;
  Protocol.Debugger.onPaused(async ({params: {callFrames}}) => {
    const expected = expectations[caseIndex];
    if (!expected) {
      InspectorTest.log(`Unexpected pause at index ${caseIndex}`);
      await Protocol.Debugger.resume();
      return;
    }
    await checkScopeChain(callFrames[0], expected.scopes);
    for (const [scopeIdx, content] of Object.entries(expected.contents)) {
      await checkScopeContent(callFrames[0], Number(scopeIdx), content);
    }
    InspectorTest.log(`${expected.name} passed`);
    caseIndex++;
    await Protocol.Debugger.resume();
  });

  const {result} = await Protocol.Runtime.evaluate({
    expression: 'runTests()',
    awaitPromise: true,
  });
  if (result.exceptionDetails) {
    InspectorTest.log(
        'runTests threw: ' + JSON.stringify(result.exceptionDetails));
  }
  if (caseIndex !== expectations.length) {
    InspectorTest.log(
        `Expected ${expectations.length} tests, ran ${caseIndex}`);
  }

  await Protocol.Debugger.disable();
  InspectorTest.completeTest();
})();
