// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Scopes with only sync `using` declarations keep the resources in registers.
// Every test runs in the interpreter, in Maglev and in Turbofan.

function check(test) {
  %PrepareFunctionForOptimization(test);
  test();
  test();
  %OptimizeMaglevOnNextCall(test);
  test();
  %OptimizeFunctionOnNextCall(test);
  test();
}

function resource(log, name, error) {
  return {
    [Symbol.dispose]() {
      log.push(name);
      if (error) throw error;
    }
  };
}

// Resources are disposed in the reverse order of their declarations, null
// and undefined are skipped.
check(function testOrder() {
  const log = [];
  {
    using a = resource(log, 'a'), b = null;
    using c = undefined;
    using d = resource(log, 'd');
    log.push('body');
  }
  assertEquals(['body', 'd', 'a'], log);
});

// The receiver of the dispose method is the resource.
check(function testReceiver() {
  let receiver;
  const r = { [Symbol.dispose]() { receiver = this; } };
  {
    using x = r;
  }
  assertSame(r, receiver);
});

// The dispose method is read once, at the declaration.
check(function testMethodReadOnce() {
  const log = [];
  let reads = 0;
  const r = {
    get [Symbol.dispose]() {
      reads++;
      return () => log.push('first');
    }
  };
  {
    using x = r;
    Object.defineProperty(r, Symbol.dispose, { value: () => log.push('later') });
  }
  assertEquals(1, reads);
  assertEquals(['first'], log);
});

// A value that is not an object throws at the declaration, and so does a
// method that is not callable. Declarations that did not run are not disposed.
check(function testInvalidValues() {
  for (const value of [1, 'str', true, Symbol()]) {
    const log = [];
    assertThrows(() => {
      using a = resource(log, 'a');
      using b = value;
      using c = resource(log, 'c');
    }, TypeError);
    assertEquals(['a'], log);
  }
  for (const method of [undefined, null, 42, {}]) {
    const log = [];
    assertThrows(() => {
      using a = resource(log, 'a');
      using b = { [Symbol.dispose]: method };
    }, TypeError);
    assertEquals(['a'], log);
  }
});

// A getter of the dispose method that throws makes the declaration throw.
check(function testGetterThrows() {
  const log = [];
  const error = new Error('getter');
  try {
    using a = resource(log, 'a');
    using b = { get [Symbol.dispose]() { throw error; } };
    log.push('unreachable');
  } catch (e) {
    assertSame(error, e);
  }
  assertEquals(['a'], log);
});

// An undetectable callable object is a valid dispose method.
check(function testUndetectableMethod() {
  {
    using x = { [Symbol.dispose]: %GetUndetectable() };
  }
});

// An error from a dispose method is thrown after the remaining resources are
// disposed.
check(function testDisposeThrows() {
  const log = [];
  const error = new Error('dispose');
  try {
    using a = resource(log, 'a');
    using b = resource(log, 'b', error);
    log.push('body');
  } catch (e) {
    assertSame(error, e);
    log.push('caught');
  }
  assertEquals(['body', 'b', 'a', 'caught'], log);
});

// An error from the body and errors from dispose methods are chained in
// SuppressedErrors.
check(function testSuppressedErrors() {
  const body = new Error('body');
  const first = new Error('first');
  const second = new Error('second');
  const log = [];
  try {
    using a = resource(log, 'a', second);
    using b = resource(log, 'b', first);
    throw body;
  } catch (e) {
    assertInstanceof(e, SuppressedError);
    assertSame(second, e.error);
    assertInstanceof(e.suppressed, SuppressedError);
    assertSame(first, e.suppressed.error);
    assertSame(body, e.suppressed.suppressed);
  }
  assertEquals(['b', 'a'], log);

  // Two errors from dispose methods without an error from the body.
  try {
    using a = resource(log, 'a', second);
    using b = resource(log, 'b', first);
  } catch (e) {
    assertInstanceof(e, SuppressedError);
    assertSame(second, e.error);
    assertSame(first, e.suppressed);
  }

  // An error from the body, and no errors from dispose methods.
  try {
    using a = resource(log, 'a');
    throw body;
  } catch (e) {
    assertSame(body, e);
  }
});

// Return, break and continue dispose the resources; an error from a dispose
// method replaces the return value.
check(function testControlFlow() {
  const log = [];
  function ret() {
    using a = resource(log, 'ret');
    return 'value';
  }
  assertEquals('value', ret());

  for (let i = 0; i < 3; i++) {
    using a = resource(log, i);
    if (i == 0) continue;
    if (i == 1) break;
  }

  outer: {
    using a = resource(log, 'label');
    break outer;
  }
  assertEquals(['ret', 0, 1, 'label'], log);

  const error = new Error();
  function retThrows() {
    using a = resource(log, 'x', error);
    return 'value';
  }
  assertThrows(retThrows, Error);
});

// A loop body is a new scope in every iteration.
check(function testLoop() {
  const log = [];
  for (let i = 0; i < 3; i++) {
    if (i == 1) {
      using a = resource(log, 'skipped-' + i);
      continue;
    }
    using b = resource(log, i);
  }
  for (using x of [resource(log, 'of-0'), resource(log, 'of-1')]) {
    log.push('iteration');
  }
  for (using x = resource(log, 'head');;) {
    log.push('body');
    break;
  }
  assertEquals(
      [0, 'skipped-1', 2, 'iteration', 'of-0', 'iteration', 'of-1', 'body',
       'head'],
      log);
});

// A `using` variable captured by a closure lives in the context.
check(function testContextAllocated() {
  const log = [];
  let read;
  {
    using a = resource(log, 'a');
    read = () => a;
    assertSame(read(), a);
  }
  assertEquals(['a'], log);
});

// A sloppy direct eval sees the `using` variables of the block.
check(function testDirectEval() {
  const log = [];
  {
    using a = resource(log, 'a');
    assertSame(a, eval('a'));
  }
  assertEquals(['a'], log);
});

// Many declarations in one scope.
check(function testManyDeclarations() {
  const count = 300;
  const log = [];
  const declarations = [];
  for (let i = 0; i < count; i++) {
    declarations.push(`using r${i} = resource(log, ${i});`);
  }
  new Function('resource', 'log', `{ ${declarations.join('\n')} }`)(
      resource, log);
  assertEquals(count, log.length);
  for (let i = 0; i < count; i++) assertEquals(count - 1 - i, log[i]);
});

// Generators dispose on return() and keep the resources across yields.
check(function testGenerator() {
  const log = [];
  function* gen() {
    using a = resource(log, 'a');
    yield 1;
    using b = resource(log, 'b');
    yield 2;
  }
  const done = [...gen()];
  assertEquals([1, 2], done);
  assertEquals(['b', 'a'], log);

  log.length = 0;
  const it = gen();
  it.next();
  it.return();
  assertEquals(['a'], log);
});

// Sync scopes nested in a scope with `await using` and the other way round.
async function testNestedAsync() {
  const log = [];
  {
    await using a = {
      async [Symbol.asyncDispose]() { log.push('async'); }
    };
    using b = resource(log, 'b');
    {
      using c = resource(log, 'c');
    }
  }
  {
    using d = resource(log, 'd');
    {
      await using e = null;
      using f = resource(log, 'f');
    }
  }
  assertEquals(['c', 'b', 'async', 'f', 'd'], log);
}
assertPromiseResult(testNestedAsync());
