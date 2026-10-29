// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function ID(x) {
  return x;
}


(function TestComputedMethodSuper() {
  var proto = {
    m() {
      return ' proto m';
    }
  };
  var object = {
    __proto__: proto,
    ['a']() { return 'a' + super.m(); },
    [ID('b')]() { return 'b' + super.m(); },
    [0]() { return '0' + super.m(); },
    [ID(1)]() { return '1' + super.m(); },
  };

  assertEquals('a proto m', object.a());
  assertEquals('b proto m', object.b());
  assertEquals('0 proto m', object[0]());
  assertEquals('1 proto m', object[1]());
})();


(function TestComputedGetterSuper() {
  var proto = {
    m() {
      return ' proto m';
    }
  };
  var object = {
    __proto__: proto,
    get ['a']() { return 'a' + super.m(); },
    get [ID('b')]() { return 'b' + super.m(); },
    get [0]() { return '0' + super.m(); },
    get [ID(1)]() { return '1' + super.m(); },
  };
  assertEquals('a proto m', object.a);
  assertEquals('b proto m', object.b);
  assertEquals('0 proto m', object[0]);
  assertEquals('1 proto m', object[1]);
})();


(function TestComputedSetterSuper() {
  var value;
  var proto = {
    m(name, v) {
      value = name + ' ' + v;
    }
  };
  var object = {
    __proto__: proto,
    set ['a'](v) { super.m('a', v); },
    set [ID('b')](v) { super.m('b', v); },
    set [0](v) { super.m('0', v); },
    set [ID(1)](v) { super.m('1', v); },
  };
  object.a = 2;
  assertEquals('a 2', value);
  object.b = 3;
  assertEquals('b 3', value);
  object[0] = 4;
  assertEquals('0 4', value);
  object[1] = 5;
  assertEquals('1 5', value);
})();
