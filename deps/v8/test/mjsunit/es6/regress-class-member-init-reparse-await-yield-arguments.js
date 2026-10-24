// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test reparsing of class member initializers when the immediately enclosing
// function did not allocate a context (and is therefore absent from the
// ScopeInfo chain).

// 1. Computed property key using 'arguments' inside a contextless function
// nested inside a class static block whose initializer allocates a context.
{
  let x = 1;
  class Outer {
    static {
      () => this;
      function foo() {
        class Inner {
          [arguments] = x();
        }
        new Inner();
      }
      assertThrows(foo, TypeError, "x is not a function");
    }
  }
}

// 2. Computed property key using 'await / 1' inside a contextless async
// function where 'await / 1... /g' is an AwaitExpression of a RegExp literal.
{
  let x = 1;
  async function inner() {
    class C {
      [await / 1]() { [[a] + 1 /g] = { a: x() }
    }
    new C();
  }
  let error;
  inner().catch(e => { error = e; });
  %PerformMicrotaskCheckpoint();
  assertInstanceof(error, TypeError);
  assertMatches(/x is not a function/, error.message);
}

// 3. Computed property key using 'yield' inside a contextless generator
// function.
{
  let x = 1;
  function* gen() {
    class C {
      [yield 1] = x();
    }
    return C;
  }
  let iter = gen();
  assertEquals({ value: 1, done: false }, iter.next());
  let C = iter.next("prop").value;
  assertThrows(() => new C(), TypeError, "x is not a function");
}

// 4. Computed property key using 'await / 1' as division inside a contextless
// synchronous function nested inside an async function that allocates a
// context.
{
  var await = 2;
  async function outer() {
    let x = 1;
    function sync_inner() {
      class C {
        [await / 1] = x();
      }
      new C();
    }
    sync_inner();
  }
  let error;
  outer().catch(e => { error = e; });
  %PerformMicrotaskCheckpoint();
  assertInstanceof(error, TypeError);
  assertMatches(/x is not a function/, error.message);
}
