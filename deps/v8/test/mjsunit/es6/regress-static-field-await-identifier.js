// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Per ECMA-262, FieldDefinition (`[Yield, ~Await]`) does not have the
// `[+Await]` grammar parameter, unlike ClassStaticBlockBody (`[~Yield, +Await]`).
// In Script mode outside async functions and static blocks, `await` is a
// valid identifier in static class field initializers.

var await = 42;

class C1 {
  static x = await;
  static f = (await) => await;
  static g = ({await = 10} = {}) => await;
}
assertEquals(42, C1.x);
assertEquals(7, C1.f(7));
assertEquals(10, C1.g());

class C2 {
  instanceField = 1;
  static x = await;
  static f = (await) => await;
  static g = ({await = 10} = {}) => await;
}
assertEquals(42, C2.x);
assertEquals(7, C2.f(7));
assertEquals(10, C2.g());

// Inside a `static {}` block, `await` is disallowed as an identifier.
assertThrows(
    () => eval("class C { static { await; } }"),
    SyntaxError,
    "Unexpected reserved word");
assertThrows(
    () => eval("class C { static { (await) => {}; } }"),
    SyntaxError,
    "Unexpected reserved word");
assertThrows(
    () => eval("class C { static { ({await} = {}); } }"),
    SyntaxError,
    "Unexpected reserved word");
assertThrows(
    () => eval("class C { static { ({await = 1} = {}); } }"),
    SyntaxError,
    "Unexpected reserved word");
assertThrows(
    () => eval("class C { static { class D { [await] = 1; } } }"),
    SyntaxError,
    "Unexpected reserved word");

assertThrows(
    () => eval("async ({await = 1}) => {}"),
    SyntaxError,
    "'await' is not a valid identifier name in an async function");
