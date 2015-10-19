// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax
// Flags: --harmony-arrow-functions --harmony-rest-parameters
// Flags: --harmony-destructuring --harmony-spread-arrays

'use strict';

(function WeakObjectLiterals() {
  function assertWeakObject(x) {
    assertFalse(%IsStrong(x));
    assertSame(Object.prototype, Object.getPrototypeOf(x));
  }
  assertWeakObject({});
  assertWeakObject({a: 0, b: 0});
  assertWeakObject({a: [], b: {}});
  assertWeakObject({a: [], b: {}}.b);
  assertWeakObject({a: {b: {c: {}}}}.a);
  assertWeakObject({a: {b: {c: {}}}}.a.b);
  assertWeakObject({a: {b: {c: {}}}}.a.b.c);
  assertWeakObject([[1], {}, [[3]]][1]);
  assertWeakObject({f: function(){}});
  assertWeakObject(
    Realm.eval(Realm.current(), "({f: function(){}})"));
})();

(function StrongObjectLiterals() {
  'use strong';
  function assertStrongObject(x) {
    assertTrue(%IsStrong(x));
    assertSame(Object.prototype, Object.getPrototypeOf(x));
  }
  assertStrongObject({});
  assertStrongObject({a: 0, b: 0});
  assertStrongObject({a: [], b: {}});
  assertStrongObject({a: [], b: {}}.b);
  assertStrongObject({a: {b: {c: {}}}}.a);
  assertStrongObject({a: {b: {c: {}}}}.a.b);
  assertStrongObject({a: {b: {c: {}}}}.a.b.c);
  // Maps for literals with too many properties are not cached.
  assertStrongObject({
    x001: 0, x002: 0, x003: 0, x004: 0, x005: 0,
    x006: 0, x007: 0, x008: 0, x009: 0, x010: 0,
    x011: 0, x012: 0, x013: 0, x014: 0, x015: 0,
    x016: 0, x017: 0, x018: 0, x019: 0, x020: 0,
    x021: 0, x022: 0, x023: 0, x024: 0, x025: 0,
    x026: 0, x027: 0, x028: 0, x029: 0, x030: 0,
    x031: 0, x032: 0, x033: 0, x034: 0, x035: 0,
    x036: 0, x037: 0, x038: 0, x039: 0, x040: 0,
    x041: 0, x042: 0, x043: 0, x044: 0, x045: 0,
    x046: 0, x047: 0, x048: 0, x049: 0, x050: 0,
    x051: 0, x052: 0, x053: 0, x054: 0, x055: 0,
    x056: 0, x057: 0, x058: 0, x059: 0, x060: 0,
    x061: 0, x062: 0, x063: 0, x064: 0, x065: 0,
    x066: 0, x067: 0, x068: 0, x069: 0, x070: 0,
    x071: 0, x072: 0, x073: 0, x074: 0, x075: 0,
    x076: 0, x077: 0, x078: 0, x079: 0, x080: 0,
    x081: 0, x082: 0, x083: 0, x084: 0, x085: 0,
    x086: 0, x087: 0, x088: 0, x089: 0, x090: 0,
    x091: 0, x092: 0, x093: 0, x094: 0, x095: 0,
    x096: 0, x097: 0, x098: 0, x099: 0, x100: 0,
    x101: 0, x102: 0, x103: 0, x104: 0, x105: 0,
    x106: 0, x107: 0, x108: 0, x109: 0, x110: 0,
    x111: 0, x112: 0, x113: 0, x114: 0, x115: 0,
    x116: 0, x117: 0, x118: 0, x119: 0, x120: 0,
    x121: 0, x122: 0, x123: 0, x124: 0, x125: 0,
    x126: 0, x127: 0, x128: 0, x129: 0, x130: 0,
    x131: 0, x132: 0, x133: 0, x134: 0, x135: 0,
    x136: 0, x137: 0, x138: 0, x139: 0, x140: 0,
    x141: 0, x142: 0, x143: 0, x144: 0, x145: 0,
    x146: 0, x147: 0, x148: 0, x149: 0, x150: 0,
    x151: 0, x152: 0, x153: 0, x154: 0, x155: 0,
    x156: 0, x157: 0, x158: 0, x159: 0, x160: 0,
    x161: 0, x162: 0, x163: 0, x164: 0, x165: 0,
    x166: 0, x167: 0, x168: 0, x169: 0, x170: 0,
    x171: 0, x172: 0, x173: 0, x174: 0, x175: 0,
    x176: 0, x177: 0, x178: 0, x179: 0, x180: 0,
    x181: 0, x182: 0, x183: 0, x184: 0, x185: 0,
    x186: 0, x187: 0, x188: 0, x189: 0, x190: 0,
    x191: 0, x192: 0, x193: 0, x194: 0, x195: 0,
    x196: 0, x197: 0, x198: 0, x199: 0, x200: 0,
  });
  assertStrongObject([[1], {}, [[3]]][1]);
  assertStrongObject({[Date() + ""]: 0, [Symbol()]: 0});
  assertStrongObject({m() { super.m() }});
  assertTrue(%IsStrong({__proto__: {}, get a() {}, set b(x) {}}));
  // Object literals with constant functions are treated specially,
  // but currently only on the toplevel (using Realm.eval to emulate that).
  assertStrongObject({f: function(){}});
  assertStrongObject(
    Realm.eval(Realm.current(), "'use strong'; ({f: function(){}})"));
})();

(function WeakArrayLiterals(...args) {
  function assertWeakArray(x) {
    assertFalse(%IsStrong(x));
    assertSame(Array.prototype, Object.getPrototypeOf(x));
  }
  let [...r] = [];
  assertWeakArray(args);
  assertWeakArray(r);
  assertWeakArray([]);
  assertWeakArray([1, 2, 3]);
  assertWeakArray([1, 2, ...[3, 4], 5]);
  assertWeakArray([[[]]]);
  assertWeakArray([[1], {}, [[3]]]);
  assertWeakArray([[1], {}, [[3]]][0]);
  assertWeakArray([[1], {}, [[3]]][2]);
  assertWeakArray([[1], {}, [[3]]][2][0]);
  assertWeakArray({a: [], b: {}}.a);
})();

(function StrongArrayLiterals(...args) {
  'use strong';
  function assertStrongArray(x) {
    assertTrue(%IsStrong(x));
    assertSame(Array.prototype, Object.getPrototypeOf(x));
  }
  let [...r] = [];
  assertStrongArray(args);
  assertStrongArray(r);
  assertStrongArray([]);
  assertStrongArray([1, 2, 3]);
  assertStrongArray([1, 2, ...[3, 4], 5]);
  assertStrongArray([[[]]]);
  assertStrongArray([[1], {}, [[3]]]);
  assertStrongArray([[1], {}, [[3]]][0]);
  assertStrongArray([[1], {}, [[3]]][2]);
  assertStrongArray([[1], {}, [[3]]][2][0]);
  assertStrongArray({a: [], b: {}}.a);
})();

(function WeakFunctionLiterals() {
  function assertWeakFunction(x) {
    assertFalse(%IsStrong(x));
    assertFalse(%IsStrong(x.prototype));
    assertSame(Function.prototype, Object.getPrototypeOf(x));
  }
  function f() {}
  assertWeakFunction(f);
  assertWeakFunction(function(){});
  assertWeakFunction(function f(){});
  assertWeakFunction(() => {});
  assertWeakFunction(x => x);
  assertWeakFunction({m(){}}.m);
  assertWeakFunction(Object.getOwnPropertyDescriptor(
      {get a(){}}, 'a').get);
  assertWeakFunction(Object.getOwnPropertyDescriptor(
      {set a(x){}}, 'a').set);
  assertWeakFunction((class {static m(){}}).m);
  assertWeakFunction(Object.getOwnPropertyDescriptor(
      class {static get a(){}}, 'a').get);
  assertWeakFunction(Object.getOwnPropertyDescriptor(
      class {static set a(x){}}, 'a').set);
  assertWeakFunction((new class {m(){}}).m);
  assertWeakFunction(Object.getOwnPropertyDescriptor(
      (class {get a(){}}).prototype, 'a').get);
  assertWeakFunction(Object.getOwnPropertyDescriptor(
      (class {set a(x){}}).prototype, 'a').set);
})();

(function StrongFunctionLiterals() {
  'use strong';
  function assertStrongFunction(x) {
    assertTrue(%IsStrong(x));
    assertFalse('prototype' in x);
    assertSame(Function.prototype, Object.getPrototypeOf(x));
  }
  function f() {}
  assertStrongFunction(f);
  assertStrongFunction(function(){});
  assertStrongFunction(function f(){});
  assertStrongFunction(() => {});
  assertStrongFunction(x => x);
  assertStrongFunction({m(){}}.m);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      {get a(){}}, 'a').get);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      {set a(x){}}, 'a').set);
  assertStrongFunction((class {static m(){}}).m);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      class {static get a(){}}, 'a').get);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      class {static set a(x){}}, 'a').set);
  assertStrongFunction((new class {m(){}}).m);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      (class {get a(){}}).prototype, 'a').get);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      (class {set a(x){}}).prototype, 'a').set);
})();

(function SelfStrongFunctionLiterals() {
  function assertStrongFunction(x) {
    assertTrue(%IsStrong(x));
    assertFalse('prototype' in x);
    assertSame(Function.prototype, Object.getPrototypeOf(x));
  }
  function f() {'use strong'}
  assertStrongFunction(f);
  assertStrongFunction(function(){'use strong'});
  assertStrongFunction(function f(){'use strong'});
  assertStrongFunction(() => {'use strong'});
  assertStrongFunction(x => {'use strong'});
  assertStrongFunction({m(){'use strong'}}.m);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      {get a(){'use strong'}}, 'a').get);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      {set a(x){'use strong'}}, 'a').set);
  assertStrongFunction((class {static m(){'use strong'}}).m);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      class {static get a(){'use strong'}}, 'a').get);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      class {static set a(x){'use strong'}}, 'a').set);
  assertStrongFunction((new class {m(){'use strong'}}).m);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      (class {get a(){'use strong'}}).prototype, 'a').get);
  assertStrongFunction(Object.getOwnPropertyDescriptor(
      (class {set a(x){'use strong'}}).prototype, 'a').set);
})();

let GeneratorPrototype = (function*(){}).__proto__;

(function WeakGeneratorLiterals() {
  function assertWeakGenerator(x) {
    assertFalse(%IsStrong(x));
    assertFalse(%IsStrong(x.prototype));
    assertSame(GeneratorPrototype, Object.getPrototypeOf(x));
    assertFalse(%IsStrong(x()));
  }
  function* g() {}
  assertWeakGenerator(g);
  assertWeakGenerator(function*(){});
  assertWeakGenerator(function* g(){});
  assertWeakGenerator({*m(){}}.m);
  assertWeakGenerator((class {static *m(){}}).m);
  assertWeakGenerator((new class {*m(){}}).m);
})();

(function StrongGeneratorLiterals() {
  'use strong';
  function assertStrongGenerator(x) {
    assertTrue(%IsStrong(x));
    // TODO(rossberg): strongify generator prototypes
    // assertTrue(%IsStrong(x.prototype));
    assertSame(GeneratorPrototype, Object.getPrototypeOf(x));
    // TODO(rossberg): strongify generator instances
    // assertTrue(%IsStrong(x()));
  }
  function* g() {}
  assertStrongGenerator(g);
  assertStrongGenerator(function*(){});
  assertStrongGenerator(function* g(){});
  assertStrongGenerator({*m(){}}.m);
  assertStrongGenerator((class {static *m(){}}).m);
  assertStrongGenerator((new class {*m(){}}).m);
})();

(function SelfStrongGeneratorLiterals() {
  function assertStrongGenerator(x) {
    assertTrue(%IsStrong(x));
    // TODO(rossberg): strongify generator prototypes
    // assertTrue(%IsStrong(x.prototype));
    assertSame(GeneratorPrototype, Object.getPrototypeOf(x));
    // TODO(rossberg): strongify generator instances
    // assertTrue(%IsStrong(x()));
  }
  function* g() {'use strong'}
  assertStrongGenerator(g);
  assertStrongGenerator(function*(){'use strong'});
  assertStrongGenerator(function* g(){'use strong'});
  assertStrongGenerator({*m(){'use strong'}}.m);
  assertStrongGenerator((class {static *m(){'use strong'}}).m);
  assertStrongGenerator((new class {*m(){'use strong'}}).m);
})();

(function WeakClassLiterals() {
  function assertWeakClass(x) {
    assertFalse(%IsStrong(x));
    assertFalse(%IsStrong(x.prototype));
    assertFalse(%IsStrong(new x));
  }
  class C {};
  class D extends C {};
  class E extends Object {};
  // class F extends null {};
  const S = (() => {'use strong'; return class {}})();
  class G extends S {};
  assertWeakClass(C);
  assertWeakClass(D);
  assertWeakClass(E);
  // assertWeakClass(F);
  assertWeakClass(G);
  assertWeakClass(class {});
  assertWeakClass(class extends Object {});
  // assertWeakClass(class extends null {});
  assertWeakClass(class extends C {});
  assertWeakClass(class extends S {});
  assertWeakClass(class extends class {} {});
  assertWeakClass(class C {});
  assertWeakClass(class D extends Object {});
  // assertWeakClass(class D extends null {});
  assertWeakClass(class D extends C {});
  assertWeakClass(class D extends S {});
  assertWeakClass(class D extends class {} {});
})();

(function StrongClassLiterals() {
  'use strong';
  function assertStrongClass(x) {
    assertTrue(%IsStrong(x));
    assertTrue(%IsStrong(x.prototype));
    // TODO(rossberg): strongify class instance
    // assertTrue(%IsStrong(new x));
  }
  class C {};
  class D extends C {};
  class E extends Object {};
  const W = (1, eval)(() => {'use strict'; return class {}})();
  class G extends W {};
  assertStrongClass(C);
  assertStrongClass(D);
  assertStrongClass(E);
  assertStrongClass(G);
  assertStrongClass(class {});
  assertStrongClass(class extends Object {});
  assertStrongClass(class extends C {});
  assertStrongClass(class extends W {});
  assertStrongClass(class extends class {} {});
  assertStrongClass(class C {});
  assertStrongClass(class D extends Object {});
  assertStrongClass(class D extends C {});
  assertStrongClass(class D extends W {});
  assertStrongClass(class D extends class {} {});
})();

(function WeakRegExpLiterals() {
  function assertWeakRegExp(x) {
    assertFalse(%IsStrong(x));
  }
  assertWeakRegExp(/abc/);
})();

(function StrongRegExpLiterals() {
  'use strong';
  function assertStrongRegExp(x) {
    // TODO(rossberg): strongify regexps
    // assertTrue(%IsStrong(x));
  }
  assertStrongRegExp(/abc/);
})();
