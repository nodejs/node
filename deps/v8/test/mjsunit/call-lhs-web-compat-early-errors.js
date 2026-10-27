// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Will be used in the tests
function foo() {}

function wrapInLazyFunction(s) {
  // Use an async function, since some tests use the await keyword.
  return "async function test() { " + s + "}";
}

function wrapInEagerFunction(s) {
  // Use an async function, since some tests use the await keyword. Await the
  // result, so that we get the error right away.
  return "await (async function test() { " + s + "})();";
}

function assertEarlyError(s) {
  assertThrows(wrapInLazyFunction(s), SyntaxError);
}

function assertLateError(s) {
  assertDoesNotThrow(wrapInLazyFunction(s));
  assertThrows(wrapInEagerFunction(s), ReferenceError);
}

// Web compatibility:
assertLateError("foo() = 0;");
assertLateError("foo()++;");
assertLateError("foo()--;");
assertLateError("++foo();");
assertLateError("--foo();");
assertLateError("foo() = 1;");
assertLateError("foo() += 1;");
assertLateError("foo() -= 1;");
assertLateError("foo() *= 1;");
assertLateError("foo() /= 1;");
assertLateError("for (foo() = 0; ; ) {}");
assertLateError("for ( ; ; foo() = 0) {}");
assertLateError("for (foo() of []) {}");
assertLateError("for (foo() in {}) {}");
assertLateError("for await (foo() of []) {}");

// The web compatibility behavior does not apply in strict mode.
assertEarlyError("\"use strict\"; foo() = 0;");
assertEarlyError("\"use strict\"; foo()++;");
assertEarlyError("\"use strict\"; ++foo();");
assertEarlyError("\"use strict\"; foo() += 1;");
assertEarlyError("\"use strict\"; for (foo() of []) {}");
assertEarlyError("\"use strict\"; for (foo() in {}) {}");
assertEarlyError("\"use strict\"; (foo()) = 0;");
assertEarlyError("\"use strict\"; (foo()) += 1;");

// These are early errors though:
assertEarlyError("for (let foo() = 0; ;) {}");
assertEarlyError("for (const foo() = 0; ;) {}");
assertEarlyError("for (var foo() = 0; ;) {}");

// Modern language features:
// Tagged templates
assertEarlyError("foo() `foo` ++;");
assertEarlyError("foo() `foo` --;");
assertEarlyError("++ foo() `foo`;");
assertEarlyError("-- foo() `foo`;");
assertEarlyError("foo() `foo` = 1;");
assertEarlyError("foo() `foo` += 1;");
assertEarlyError("foo() `foo` -= 1;");
assertEarlyError("foo() `foo` *= 1;");
assertEarlyError("foo() `foo` /= 1;");
assertEarlyError("for (foo() `foo` = 0; ; ) {}");
assertEarlyError("for (; ; foo() `foo` = 0) {}");

// Logical assignment
assertEarlyError("foo() &&= 1;");
assertEarlyError("foo() ||= 1;");
assertEarlyError("foo() ??= 1;");
assertEarlyError("for (foo() &&= 0; ;) {}");
assertEarlyError("for ( ; ; foo() &&= 0) {}");
