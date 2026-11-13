// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests REPL mode evaluation with optimized functions.');

contextGroup.addScript(`
function checkOptimized(fn) {
  const status = %GetOptimizationStatus(fn);
  if ((status & (1 << 2)) !== 0 || (status & (1 << 20)) !== 0) return;
  if ((status & (1 << 3)) === 0) {
    throw new Error('Expected ' + fn.name + ' to be optimized');
  }
}
`);

(async function test() {
  await Protocol.Debugger.enable();
  await Protocol.Runtime.enable();

  async function evaluate(expression) {
    const {result} = await Protocol.Runtime.evaluate({
      expression,
      replMode: true,
    });
    if (result.exceptionDetails) {
      InspectorTest.log(
          'Unexpected exception in REPL: ' + result.result.description);
    }
    return result.result.value;
  }

  async function evaluateNonREPL(expression) {
    const {result} = await Protocol.Runtime.evaluate({
      expression,
      replMode: false,
    });
    if (result.exceptionDetails) {
      InspectorTest.log('Unexpected exception: ' + result.result.description);
    }
    return result.result.value;
  }

  // Test that a let declared variable 'x' bound by an optimized function is
  // updated properly.
  await evaluate('let x = 42;');
  await evaluate('function foo() { return x; }');

  await evaluateNonREPL(`
    %PrepareFunctionForOptimization(foo);
    foo();
    foo();
    %OptimizeFunctionOnNextCall(foo);
  `);
  InspectorTest.log('foo() optimized: ' + await evaluateNonREPL('foo()'));
  await evaluateNonREPL('checkOptimized(foo)');

  await evaluate('let x = 21');
  InspectorTest.log(
      'foo() after let x = 21: ' + await evaluateNonREPL('foo()'));

  // Test that we do not throw a 'use before declaration error' and
  // that the optimized code does not load the hole from the wrong
  // script context.
  await evaluate('x; let x;');
  InspectorTest.log('foo() after x; let x;: ' + await evaluateNonREPL('foo()'));

  // Test that a const declared variable 'y' bound by an optimized function is
  // updated properly.
  await evaluate('const y = 42;');
  await evaluate('function foo1() { return y; }');

  await evaluateNonREPL(`
    %PrepareFunctionForOptimization(foo1);
    foo1();
    foo1();
    %OptimizeFunctionOnNextCall(foo1);
  `);
  InspectorTest.log('foo1() optimized: ' + await evaluateNonREPL('foo1()'));
  await evaluateNonREPL('checkOptimized(foo1)');

  await evaluate('const y = 21');
  InspectorTest.log(
      'foo1() after const y = 21: ' + await evaluateNonREPL('foo1()'));

  // Test that a const declared variable 'z' bound by an optimized function is
  // updated properly.
  await evaluate('const z = {a:0};');
  await evaluate('function foo2() { z.a = 42; }');

  await evaluateNonREPL(`
    %PrepareFunctionForOptimization(foo2);
    foo2();
    foo2();
    %OptimizeFunctionOnNextCall(foo2);
    foo2();
    checkOptimized(foo2);
  `);

  await evaluate('const z = {a:21}');
  await evaluateNonREPL('foo2()');
  InspectorTest.log(
      'z.a after const z = {a:21} and foo2(): ' + await evaluateNonREPL('z.a'));

  InspectorTest.completeTest();
})();
