// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function ID(x) {
  return x;
}


(function TestComputedMethodSuper() {
  class Base {
    m() {
      return ' base m';
    }
  }
  class Derived extends Base {
    ['a']() { return 'a' + super.m(); }
    [ID('b')]() { return 'b' + super.m(); }
    [0]() { return '0' + super.m(); }
    [ID(1)]() { return '1' + super.m(); }
  }

  assertSame(Derived.prototype, Derived.prototype.a[%HomeObjectSymbol()]);

  assertEquals('a base m', new Derived().a());
  assertEquals('b base m', new Derived().b());
  assertEquals('0 base m', new Derived()[0]());
  assertEquals('1 base m', new Derived()[1]());
})();


(function TestComputedGetterSuper() {
  class Base {
    m() {
      return ' base m';
    }
  }
  class Derived extends Base {
    get ['a']() { return 'a' + super.m(); }
    get [ID('b')]() { return 'b' + super.m(); }
    get [0]() { return '0' + super.m(); }
    get [ID(1)]() { return '1' + super.m(); }
  }
  assertEquals('a base m', new Derived().a);
  assertEquals('b base m', new Derived().b);
  assertEquals('0 base m', new Derived()[0]);
  assertEquals('1 base m', new Derived()[1]);
})();


(function TestComputedSetterSuper() {
  var value;
  class Base {
    m(name, v) {
      value = name + ' ' + v;
    }
  }
  class Derived extends Base {
    set ['a'](v) { super.m('a', v); }
    set [ID('b')](v) { super.m('b', v); }
    set [0](v) { super.m('0', v); }
    set [ID(1)](v) { super.m('1', v); }
  }
  new Derived().a = 2;
  assertEquals('a 2', value);
  new Derived().b = 3;
  assertEquals('b 3', value);
  new Derived()[0] = 4;
  assertEquals('0 4', value);
  new Derived()[1] = 5;
  assertEquals('1 5', value);
})();
