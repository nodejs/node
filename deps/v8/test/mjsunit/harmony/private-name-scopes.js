// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
    let heritageFn;
    class O {
      #f = "O.#f";
      static C = class C extends (heritageFn = function () {
                   return class D {
                     exfil(obj) { return obj.#f; }
                     exfilEval(obj) { return eval("obj.#f"); }
                   };
                 }) {
                   #f = "C.#f";
                 };
    }

    const o = new O;
    const c = new O.C;
    const D = heritageFn();
    const d = new D;
    assertEquals(d.exfil(o), "O.#f");
    assertEquals(d.exfilEval(o), "O.#f");
    assertThrows(() => d.exfil(c), TypeError);
    assertThrows(() => d.exfilEval(c), TypeError);
}

// Early errors

assertThrows(() => eval("new class extends " +
                        "(class { m() { let x = this.#f; } }) " +
                        "{ #f }"), SyntaxError);

assertThrows(() => eval("new class extends this.#foo { #foo }"), SyntaxError);

// Runtime errors

{
  // Test private name context chain recalc.
  let heritageFn;
  class O {
    #f = "O.#f";
    static C = class C extends (heritageFn = function () {
                 return class D { exfil(obj) { return obj.#f; } }
               }) {
                 #f = "C.#f";
               };
  }

  const o = new O;
  const c = new O.C;
  const D = heritageFn();
  const d = new D;
  assertEquals(d.exfil(o), "O.#f");
  assertThrows(() => d.exfil(c), TypeError);
}

{
  // Test private name context chain recalc with nested closures with context.
  let heritageFn;
  class O {
    #f = "O.#f";
    static C = class C extends (heritageFn = function () {
                 let forceContext = 1;
                 return () => {
                   assertEquals(forceContext, 1);
                   return class D { exfil(obj) { return obj.#f; } }
                 };
               }) {
                 #f = "C.#f";
               };
  }

  const o = new O;
  const c = new O.C;
  const D = heritageFn()();
  const d = new D;
  assertEquals(d.exfil(o), "O.#f");
  assertThrows(() => d.exfil(c), TypeError);
}

{
  // Test private name context chain recalc where skipped class has no context.
  let heritageFn;
  class O {
    #f = "O.#f";
    static C = class C0 extends (class C1 extends (heritageFn = function (obj) {
                 if (obj) { return obj.#f; }
               }) {}) {
                 #f = "C0.#f"
               }
  }

  const o = new O;
  const c = new O.C;
  assertEquals(heritageFn(o), "O.#f");
  assertThrows(() => heritageFn(c), TypeError);
}

{
  // Test private name context chain recalc where skipping function has no
  // context.
  let heritageFn;
  class O {
    #f = "O.#f";
    static C = class C extends (heritageFn = function () {
                 return (obj) => { return obj.#f; }
               }) {
                 #f = "C.#f";
               }
  }

  const o = new O;
  const c = new O.C;
  assertEquals(heritageFn()(o), "O.#f");
  assertThrows(() => heritageFn()(c), TypeError);
}

{
  // Test private name context chain recalc where neither skipped class nor
  // skipping function has contexts.
  let heritageFn;
  class O {
    #f = "O.#f";
    static C = class C0 extends (class C1 extends (heritageFn = function () {
                 return (obj) => { return obj.#f; }
               }) {}) {
                 #f = "C0.#f";
               }
  }

  const o = new O;
  const c = new O.C;
  assertEquals(heritageFn()(o), "O.#f");
  assertThrows(() => heritageFn()(c), TypeError);
}
