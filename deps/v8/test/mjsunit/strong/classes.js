// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode
// Flags: --harmony-classes --harmony-arrow-functions

'use strong';

class C {}

let indirect_eval = eval;

function assertTypeError(script) { assertThrows(script, TypeError) }
function assertSyntaxError(script) { assertThrows(script, SyntaxError) }
function assertReferenceError(script) { assertThrows(script, ReferenceError) }

(function ImmutableClassBindings() {
  class D {}
  assertTypeError(function(){ indirect_eval("C = 0") });
  assertEquals('function', typeof C);
  assertEquals('function', typeof D);
  assertTypeError("'use strong'; (function f() {class E {}; E = 0})()");
})();

function constructor(body) {
  return "'use strong'; " +
      "(class extends Object { constructor() { " + body + " } })";
}

(function NoSuperExceptCall() {
  assertSyntaxError(constructor("super.a;"));
  assertSyntaxError(constructor("super['a'];"));
  assertSyntaxError(constructor("super.f();"));
  assertSyntaxError(constructor("super.a;"));
  assertSyntaxError(constructor("{ super.a }"));
  assertSyntaxError(constructor("if (0) super.a;"));
  // TODO(rossberg): arrow functions do not handle 'super' yet.
  // assertSyntaxError(constructor("() => super.a;"));
  // assertSyntaxError(constructor("() => () => super.a;"));
  // assertSyntaxError(constructor("() => { () => if (0) { super.a; } }"));
})();

(function NoMissingSuper() {
  assertReferenceError(constructor(""));
  assertReferenceError(constructor("1"));
})();

(function NoNestedSuper() {
  assertSyntaxError(constructor("super(), 0;"));
  assertSyntaxError(constructor("(super());"));
  assertSyntaxError(constructor("super().a;"));
  assertSyntaxError(constructor("(() => super())();"));
  assertSyntaxError(constructor("{ super(); }"));
  assertSyntaxError(constructor("if (1) super();"));
  assertSyntaxError(constructor("label: super();"));
})();

(function NoDuplicateSuper() {
  assertSyntaxError(constructor("super(), super();"));
  assertSyntaxError(constructor("super(); super();"));
  assertSyntaxError(constructor("super(); (super());"));
  assertSyntaxError(constructor("super(); { super() }"));
  assertSyntaxError(constructor("super(); (() => super())();"));
})();

(function NoSuperAfterThis() {
  assertSyntaxError(constructor("this.a = 0, super();"));
  assertSyntaxError(constructor("this.a = 0; super();"));
  assertSyntaxError(constructor("this.a = 0; super(); this.b = 0;"));
  assertSyntaxError(constructor("this.a = 0; (super());"));
  assertSyntaxError(constructor("super(this.a = 0);"));
})();

(function NoReturnValue() {
  assertSyntaxError(constructor("return {};"));
  assertSyntaxError(constructor("return undefined;"));
  assertSyntaxError(constructor("return this;"));
  assertSyntaxError(constructor("return this.a = 0;"));
  assertSyntaxError(constructor("{ return {}; }"));
  assertSyntaxError(constructor("if (1) return {};"));
})();

(function NoReturnBeforeSuper() {
  assertSyntaxError(constructor("return; super();"));
  assertSyntaxError(constructor("if (0) return; super();"));
  assertSyntaxError(constructor("{ return; } super();"));
})();

(function NoReturnBeforeThis() {
  assertSyntaxError(constructor("return; this.a = 0;"));
  assertSyntaxError(constructor("if (0) return; this.a = 0;"));
  assertSyntaxError(constructor("{ return; } this.a = 0;"));
})();

(function NoThisExceptInitialization() {
  assertSyntaxError(constructor("this;"));
  assertSyntaxError(constructor("this.a;"));
  assertSyntaxError(constructor("this['a'];"));
  assertSyntaxError(constructor("this();"));
  assertSyntaxError(constructor("this.a();"));
  assertSyntaxError(constructor("this.a.b = 0;"));
  assertSyntaxError(constructor("{ this }"));
  assertSyntaxError(constructor("if (0) this;"));
  // TODO(rossberg): this does not handle arrow functions yet.
  // assertSyntaxError(constructor("() => this;"));
  // assertSyntaxError(constructor("() => () => this;"));
  // assertSyntaxError(constructor("() => { () => if (0) { this; } }"));
})();

(function NoNestedThis() {
  assertSyntaxError(constructor("(this.a = 0);"));
  assertSyntaxError(constructor("{ this.a = 0; }"));
  assertSyntaxError(constructor("if (0) this.a = 0;"));
  // TODO(rossberg): this does not handle arrow functions yet.
  // assertSyntaxError(constructor("() => this.a = 0;"));
  // assertSyntaxError(constructor("() => { this.a = 0; }"));
  assertSyntaxError(constructor("label: this.a = 0;"));
})();
