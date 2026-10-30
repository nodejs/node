// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-decorators

// Issue A: `for \u0061wait` must not be parsed as `for await`.
assertThrows("async function f() { for \\u0061wait (let x of []) {} }", SyntaxError);
assertThrows("async () => { for \\u0061wait (let x of []) {} }", SyntaxError);

// Issue B: Escaped `using` or `await` must not be parsed as `using` / `await using`.
assertThrows("{ \\u0075sing x = null; }", SyntaxError);
assertThrows("function f() { \\u0075sing x = null; }", SyntaxError);
assertThrows("for (\\u0075sing x of []) {}", SyntaxError);
assertThrows("for (\\u0075sing x = null;;) {}", SyntaxError);
assertThrows("async function f() { for await (\\u0075sing x of []) {} }", SyntaxError);

assertThrows("async function f() { \\u0061wait using x = null; }", SyntaxError);
assertThrows("async function f() { await \\u0075sing x = null; }", SyntaxError);
assertThrows("async function f() { for (\\u0061wait using x of []) {} }", SyntaxError);
assertThrows("async function f() { for (await \\u0075sing x of []) {} }", SyntaxError);
assertThrows("async function f() { for (\\u0061wait using x = null;;) {} }", SyntaxError);
assertThrows("async function f() { for (await \\u0075sing x = null;;) {} }", SyntaxError);
assertThrows("async function f() { for await (\\u0061wait using x of []) {} }", SyntaxError);
assertThrows("async function f() { for await (await \\u0075sing x of []) {} }", SyntaxError);

// Escaped `\u006ff` in `for (using \u006ff of ...)` is a valid binding identifier.
{
  let disposed = false;
  for (using \u006ff of [{ [Symbol.dispose]() { disposed = true; } }]) {
    assertEquals("object", typeof of);
  }
  assertTrue(disposed);
}

// Escaped `\u0075sing` in `for (\u0075sing of ...)` is an identifier reference.
{
  let using = 0;
  for (\u0075sing of [42]) {}
  assertEquals(42, using);
}

// Issue C: Escaped `\u0061ccessor` in class body cannot start an auto-accessor.
assertThrows("class C { \\u0061ccessor x = 1; }", SyntaxError);
assertThrows("class C { static \\u0061ccessor x = 1; }", SyntaxError);
assertThrows("class C { \\u0061ccessor [x] = 1; }", SyntaxError);

{
  class C {
    \u0061ccessor = 42;
    \u0061ccessor2() { return 10; }
  }
  assertEquals(42, new C().accessor);
}

{
  class C {
    \u0061ccessor() { return 42; }
  }
  assertEquals(42, new C().accessor());
}

{
  class C {
    \u0061ccessor
    x = 42;
  }
  let c = new C();
  assertTrue(Object.hasOwn(c, "accessor"));
  assertEquals(undefined, c.accessor);
  assertEquals(42, c.x);
}
