// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-1.mjs';

assertEquals(0, globalThis.eval_list.length);

assertEquals('Deferred Module', ns[Symbol.toStringTag]);
assertEquals(0, globalThis.eval_list.length);

const nonExistentSymbol = Symbol('nonExistent');
assertEquals(undefined, ns[nonExistentSymbol]);
assertEquals(0, globalThis.eval_list.length);

assertEquals(undefined, ns.then);
assertEquals(0, globalThis.eval_list.length);

assertThrows(() => ns.foo = 30, TypeError);
assertEquals(0, globalThis.eval_list.length);

assertThrows(() => ns.nonExistent = 30, TypeError);
assertEquals(0, globalThis.eval_list.length);

assertThrows(() => ns[Symbol.toStringTag] = 30, TypeError);
assertEquals(0, globalThis.eval_list.length);

assertThrows(() => ns[Symbol('nonExistent')] = 30, TypeError);
assertEquals(0, globalThis.eval_list.length);

let obj = Object.create(ns);

assertThrows(() => obj.foo = 40, TypeError);
assertEquals(0, globalThis.eval_list.length);

assertThrows(() => obj.nonExistent = 41, TypeError);
assertEquals(0, globalThis.eval_list.length);

// super property set with symbol-like keys should not trigger evaluation.
class A { constructor() { return ns; } }

class B1 extends A {
  constructor() {
    super();
    super[Symbol()] = 14;
  }
}
try { new B1(); } catch (_) {}
assertEquals(0, globalThis.eval_list.length);

class B2 extends A {
  constructor() {
    super();
    super[Symbol.toStringTag] = 14;
  }
}
try { new B2(); } catch (_) {}
assertEquals(0, globalThis.eval_list.length);

class B3 extends A {
  constructor() {
    super();
    super.then = 14;
  }
}
try { new B3(); } catch (_) {}
assertEquals(0, globalThis.eval_list.length);

