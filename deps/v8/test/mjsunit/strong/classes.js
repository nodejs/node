// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode
// Flags: --harmony-classes --harmony-arrow-functions

'use strong';

class C {}

function assertTypeError(script) { assertThrows(script, TypeError) }
function assertSyntaxError(script) { assertThrows(script, SyntaxError) }
function assertReferenceError(script) { assertThrows(script, ReferenceError) }

(function ImmutableClassBindings() {
  class D {}
  assertTypeError(function(){ eval("C = 0") });
  assertTypeError(function(){ eval("D = 0") });
  assertEquals('function', typeof C);
  assertEquals('function', typeof D);
})();

function constructor(body) {
  return "'use strong'; " +
      "(class extends Object { constructor() { " + body + " } })";
}

(function NoMissingSuper() {
  assertReferenceError(constructor(""));
  assertReferenceError(constructor("1"));
})();

(function NoNestedSuper() {
  assertSyntaxError(constructor("(super());"));
  assertSyntaxError(constructor("(() => super())();"));
  assertSyntaxError(constructor("{ super(); }"));
  assertSyntaxError(constructor("if (1) super();"));
})();

(function NoDuplicateSuper() {
  assertSyntaxError(constructor("super(), super();"));
  assertSyntaxError(constructor("super(); super();"));
  assertSyntaxError(constructor("super(); (super());"));
  assertSyntaxError(constructor("super(); { super() }"));
  assertSyntaxError(constructor("super(); (() => super())();"));
})();

(function NoReturnValue() {
  assertSyntaxError(constructor("return {};"));
  assertSyntaxError(constructor("return undefined;"));
  assertSyntaxError(constructor("{ return {}; }"));
  assertSyntaxError(constructor("if (1) return {};"));
})();

(function NoReturnBeforeSuper() {
  assertSyntaxError(constructor("return; super();"));
  assertSyntaxError(constructor("if (0) return; super();"));
  assertSyntaxError(constructor("{ return; } super();"));
})();
