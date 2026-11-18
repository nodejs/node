// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests REPL mode evaluation in global scope.');

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  async function evaluate(expression) {
    const {result} = await Protocol.Runtime.evaluate({
      expression,
      replMode: true,
    });
    if (result.exceptionDetails) {
      throw new Error(result.result.description);
    }
    return result.result.value;
  }

  async function evaluateNonREPL(expression) {
    const {result} = await Protocol.Runtime.evaluate({
      expression,
      replMode: false,
    });
    if (result.exceptionDetails) {
      throw new Error(result.result.description);
    }
    return result.result.value;
  }

  async function assertThrowsREPL(
      expression, expectedClassName, expectedMessage) {
    const {result} = await Protocol.Runtime.evaluate({
      expression,
      replMode: true,
    });
    if (!result.exceptionDetails) {
      throw new Error('Expected exception for: ' + expression);
    }
    if (expectedClassName && result.result.className !== expectedClassName) {
      throw new Error(
          'Expected ' + expectedClassName + ', got ' + result.result.className +
          ' for: ' + expression);
    }
    if (expectedMessage &&
        !result.result.description.includes(expectedMessage)) {
      throw new Error(
          'Expected message "' + expectedMessage + '", got "' +
          result.result.description + '" for: ' + expression);
    }
  }

  function assertEquals(expected, actual) {
    if (expected !== actual) {
      throw new Error('Expected ' + expected + ', got ' + actual);
    }
  }

  // Declare let and get value
  let result;
  result = await evaluate('let x = 7;');
  result = await evaluate('x;');
  assertEquals(7, result);

  // Redeclare in the same script after declaration in another script.
  await assertThrowsREPL('let x = 8; let x = 9;');
  result = await evaluate('x;');
  assertEquals(7, result);

  // Redeclare let as let
  result = await evaluate('let x = 8;');
  result = await evaluate('x;');
  assertEquals(8, result);

  await evaluate('let x = 8;');

  // Close over let. Inner function is only pre-parsed.
  result = await evaluate('function getter() { return x; }');
  assertEquals(undefined, result);
  result = await evaluate('getter();');
  assertEquals(8, result);
  result = await evaluate('x = 9;');
  assertEquals(9, result);
  result = await evaluate('x;');
  assertEquals(9, result);
  result = await evaluate('getter();');
  assertEquals(9, result);
  // Modifies the original x; does not create a new one/shadow.
  result = await evaluate('let x = 10;');
  assertEquals(undefined, result);
  result = await evaluate('x;');
  assertEquals(10, result);
  result = await evaluate('getter();');
  assertEquals(10, result);

  await evaluate('let x = 10');

  // Check store from an inner scope.
  result = await evaluate('{ let z; x = 11; } x;');
  assertEquals(11, result);

  // Check redeclare from an inner scope does nothing.
  result = await evaluate('{ let z; let x = 12; } x;');
  assertEquals(11, result);

  await assertThrowsREPL(
      '{ let qq = 10; } qq;', 'ReferenceError', 'qq is not defined');

  // Redeclare in the same script (no previous declaration).
  await assertThrowsREPL(
      'let y = 7; let y = 8;', 'SyntaxError',
      'Identifier \'y\' has already been declared');

  // Check TDZ; use before initialization.
  // Do not check exact error message, it depends on the path taken through
  // the IC machinery and changes sometimes, causing the test to be flaky.
  await assertThrowsREPL('a; let a = 7;', 'ReferenceError');
  await assertThrowsREPL('a;', 'ReferenceError');
  // This is different to non-REPL mode, which throws the kUndefined error here.
  await assertThrowsREPL(
      'a = 7;', 'ReferenceError', 'Cannot access \'a\' before initialization');

  result = await evaluate('let a = 8;');
  assertEquals(undefined, result);
  result = await evaluate('a;');
  assertEquals(8, result);

  // Check TDZ; store before initialization.
  await assertThrowsREPL(
      'b = 10; let b;', 'ReferenceError',
      'Cannot access \'b\' before initialization');
  // Check that b is still broken.
  await assertThrowsREPL(
      'b = 10; let b;', 'ReferenceError',
      'Cannot access \'b\' before initialization');
  // Check that b is still broken when the let defines a value.
  await assertThrowsREPL(
      'b = 10; let b = 7;', 'ReferenceError',
      'Cannot access \'b\' before initialization');
  result = await evaluate('let b = 11;');
  assertEquals(undefined, result);
  // We fixed 'b'!
  result = await evaluate('b;');
  assertEquals(11, result);

  // Check that class works the same. Internally there is no difference between
  // class and let so we don't test as extensively as for let.
  result = await evaluate('class K {};');
  result = await evaluate('class K {};');

  // Declare const and get value
  result = await evaluate('const c = 7;');
  result = await evaluate('c;');
  assertEquals(7, result);

  // Redeclare in the same script after declaration in another script.
  await assertThrowsREPL('let c = 8; let c = 9;');
  result = await evaluate('c;');
  assertEquals(7, result);

  // Redeclare const as const
  result = await evaluate('const c = 8;');
  result = await evaluate('c;');
  assertEquals(8, result);

  // Assign to const
  await assertThrowsREPL(
      'c = 11;', 'TypeError', 'Assignment to constant variable.');
  result = await evaluate('c;');
  assertEquals(8, result);

  await evaluate('const c = 8;');

  // Close over const. Inner function is only pre-parsed.
  result = await evaluate('function getter() { return c; }');
  assertEquals(undefined, result);
  result = await evaluate('getter();');
  assertEquals(8, result);
  // Modifies the original c; does not create a new one/shadow.
  result = await evaluate('const c = 10;');
  assertEquals(undefined, result);
  result = await evaluate('c;');
  assertEquals(10, result);
  result = await evaluate('getter();');
  assertEquals(10, result);

  await evaluate('const c = 10');

  // Check store from an inner scope throws error.
  await assertThrowsREPL(
      '{ let z; c = 11; };', 'TypeError', 'Assignment to constant variable.');
  result = await evaluate('c;');
  assertEquals(10, result);

  // Check redeclare from an inner scope does nothing.
  result = await evaluate('{ let z; const c = 12; } c;');
  assertEquals(10, result);

  await assertThrowsREPL(
      '{ const qq = 10; } qq;', 'ReferenceError', 'qq is not defined');

  // Const vs. const in same script.
  await assertThrowsREPL(
      'const d = 9; const d = 10;', 'SyntaxError',
      'Identifier \'d\' has already been declared');

  // Check TDZ; const use before initialization.
  await assertThrowsREPL('e; const e = 7;', 'ReferenceError');
  await assertThrowsREPL('e;', 'ReferenceError');

  result = await evaluate('const e = 8;');
  assertEquals(undefined, result);
  result = await evaluate('e;');
  assertEquals(8, result);

  // f is marked as constant in TDZ
  await assertThrowsREPL(
      'f = 10; const f = 7;', 'TypeError', 'Assignment to constant variable.');
  result = await evaluate('const f = 11;');
  assertEquals(undefined, result);
  // We fixed 'f'!
  result = await evaluate('f;');
  assertEquals(11, result);

  // Redeclare let as const
  await evaluate('let z = 10;');
  await assertThrowsREPL(
      'const z = 9;', 'SyntaxError',
      'Identifier \'z\' has already been declared');
  result = await evaluate('z;');
  assertEquals(10, result);

  // Redeclare const as let
  result = await evaluate('const g = 12;');
  await assertThrowsREPL(
      'let g = 13;', 'SyntaxError',
      'Identifier \'g\' has already been declared');
  result = await evaluate('g;');
  assertEquals(12, result);

  // Let vs. const in the same script
  await assertThrowsREPL(
      'let h = 13; const h = 14;', 'SyntaxError',
      'Identifier \'h\' has already been declared');
  await assertThrowsREPL(
      'const i = 13; let i = 14;', 'SyntaxError',
      'Identifier \'i\' has already been declared');

  // Configurable properties of the global object can be redeclared as let.
  result = await evaluate(`Object.defineProperty(globalThis, 'j', {
    value: 1,
    configurable: true
  });`);
  result = await evaluate('j;');
  assertEquals(1, result);
  result = await evaluate('let j = 2;');
  result = await evaluate('j;');
  assertEquals(2, result);

  // Non-configurable properties of the global object (also created by plain old
  // top-level var declarations) cannot be redeclared as let.
  result = await evaluate(`Object.defineProperty(globalThis, 'k', {
    value: 1,
    configurable: false
  });`);
  result = await evaluate('k;');
  assertEquals(1, result);
  await assertThrowsREPL(
      'let k = 2;', 'SyntaxError',
      'Identifier \'k\' has already been declared');
  result = await evaluate('k;');
  assertEquals(1, result);

  // Except if you do it in the same script.
  result = await evaluate(`Object.defineProperty(globalThis, 'k2', {
    value: 1,
    configurable: false
  });
  let k2 = 2;`);
  result = await evaluate('k2;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k2;');
  assertEquals(1, result);

  // But if the property is configurable then both versions are allowed.
  result = await evaluate(`Object.defineProperty(globalThis, 'k3', {
    value: 1,
    configurable: true
  });`);
  result = await evaluate('k3;');
  assertEquals(1, result);
  result = await evaluate('let k3 = 2;');
  result = await evaluate('k3;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k3;');
  assertEquals(1, result);

  result = await evaluate(`Object.defineProperty(globalThis, 'k4', {
    value: 1,
    configurable: true
  });
  let k4 = 2;`);
  result = await evaluate('k4;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k4;');
  assertEquals(1, result);

  // Check var followed by let in the same script.
  await assertThrowsREPL(
      'var k5 = 1; let k5 = 2;', 'SyntaxError',
      'Identifier \'k5\' has already been declared');

  // In different scripts.
  result = await evaluate('var k6 = 1;');
  await assertThrowsREPL(
      'let k6 = 2;', 'SyntaxError',
      'Identifier \'k6\' has already been declared');

  // Check let followed by var in the same script.
  await assertThrowsREPL(
      'let k7 = 1; var k7 = 2;', 'SyntaxError',
      'Identifier \'k7\' has already been declared');

  // In different scripts.
  result = await evaluate('let k8 = 1;');
  await assertThrowsREPL(
      'var k8 = 2;', 'SyntaxError',
      'Identifier \'k8\' has already been declared');

  // Check var followed by var in the same script.
  result = await evaluate('var k9 = 1; var k9 = 2;');
  result = await evaluate('k9;');
  assertEquals(2, result);

  // In different scripts.
  result = await evaluate('var k10 = 1;');
  result = await evaluate('var k10 = 2;');
  result = await evaluate('k10;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k10;');
  assertEquals(2, result);

  // typeof should not throw for undeclared variables
  result = await evaluate('typeof k11');
  assertEquals('undefined', result);

  // Non-configurable properties of the global object (also created by plain old
  // top-level var declarations) cannot be redeclared as const.
  result = await evaluate(`Object.defineProperty(globalThis, 'k12', {
    value: 1,
    configurable: false
  });`);
  result = await evaluate('k12;');
  assertEquals(1, result);
  await assertThrowsREPL(
      'const k12 = 2;', 'SyntaxError',
      'Identifier \'k12\' has already been declared');
  result = await evaluate('k12;');
  assertEquals(1, result);

  // Except if you do it in the same script.
  result = await evaluate(`Object.defineProperty(globalThis, 'k13', {
    value: 1,
    configurable: false
  });
  const k13 = 2;`);
  result = await evaluate('k13;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k13;');
  assertEquals(1, result);

  // But if the property is configurable then both versions are allowed.
  result = await evaluate(`Object.defineProperty(globalThis, 'k14', {
    value: 1,
    configurable: true
  });`);
  result = await evaluate('k14;');
  assertEquals(1, result);
  result = await evaluate('const k14 = 2;');
  result = await evaluate('k14;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k14;');
  assertEquals(1, result);

  result = await evaluate(`Object.defineProperty(globalThis, 'k15', {
    value: 1,
    configurable: true
  });
  const k15 = 2;`);
  result = await evaluate('k15;');
  assertEquals(2, result);
  result = await evaluate('globalThis.k15;');
  assertEquals(1, result);

  // REPL vs. non-REPL scripts

  // We can still read let values cross-mode.
  result = await evaluateNonREPL('let l1 = 1; let l2 = 2; let l3 = 3;');
  result = await evaluate('l1;');
  assertEquals(1, result);

  // But we can't redeclare page script lets in a REPL script. We might want to
  // later.
  await assertThrowsREPL(
      'let l1 = 2;', 'SyntaxError',
      'Identifier \'l1\' has already been declared');

  await assertThrowsREPL(
      'var l2 = 3;', 'SyntaxError',
      'Identifier \'l2\' has already been declared');

  await assertThrowsREPL(
      'const l3 = 4;', 'SyntaxError',
      'Identifier \'l3\' has already been declared');

  // Redeclaring var across modes works.
  result = await evaluateNonREPL('var l4 = 1; const l5 = 1;');
  result = await evaluate('var l4 = 2;');
  result = await evaluate('l4;');
  assertEquals(2, result);

  // Const doesn't.
  await assertThrowsREPL(
      'const l5 = 2;', 'SyntaxError',
      'Identifier \'l5\' has already been declared');
  result = await evaluate('l5;');
  assertEquals(1, result);

  // Now REPL followed by non-REPL
  result = await evaluate('let l6 = 1; const l7 = 2; let l8 = 3;');
  result = await evaluateNonREPL('l7;');
  assertEquals(2, result);
  result = await evaluateNonREPL('l6;');
  assertEquals(1, result);

  // Check that the pattern of `l9; let l9;` does not throw for REPL scripts.
  result = await evaluate('let l9;');
  result = await evaluate('l9; let l9;');
  assertEquals(undefined, await evaluate('l9;'));

  // Check that binding and redeclaring a function via let works.
  await evaluate('let fn1 = function() { return 21; }');
  assertEquals(21, await evaluateNonREPL('fn1()'));
  await evaluate('let fn1 = function() { return 42; }');
  assertEquals(42, await evaluateNonREPL('fn1()'));

  // Check that lazily parsed functions that bind a REPL-let variable work.
  await evaluate('let l10 = 21;');
  await evaluate('let l10 = 42; function fn2() { return l10; }');
  await evaluate('let l10 = \'foo\';');
  assertEquals('foo', await evaluateNonREPL('fn2()'));

  // Check that binding and redeclaring a function via const works.
  await evaluate('const fn3 = function() { return 21; }');
  assertEquals(21, await evaluateNonREPL('fn3()'));
  await evaluate('const fn3 = function() { return 42; }');
  assertEquals(42, await evaluateNonREPL('fn3()'));

  // Check that lazily parsed functions that bind a REPL-const variable work.
  await evaluate('const l11 = 21;');
  await evaluate('const l11 = 42; function fn4() { return l11; }');
  await evaluate('const l11 = \'foo1\';');
  assertEquals('foo1', await evaluateNonREPL('fn4()'));

  InspectorTest.log('REPL mode test completed successfully.');
  InspectorTest.completeTest();
})();
