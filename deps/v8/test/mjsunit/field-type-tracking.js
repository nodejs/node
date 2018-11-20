// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --nostress-opt --track-field-types

(function() {
  var o = { text: "Hello World!" };
  function A() {
    this.a = o;
  }
  function readA(x) {
    return x.a;
  }
  var a = new A();
  assertUnoptimized(readA);
  readA(a); readA(a); readA(a);
  %OptimizeFunctionOnNextCall(readA);
  assertEquals(readA(a), o);
  assertOptimized(readA);

  var b = new A();
  b.b = o;
  assertEquals(readA(b), o);
  assertUnoptimized(readA);
  %OptimizeFunctionOnNextCall(readA);
  assertEquals(readA(a), o);
  assertOptimized(readA);
  assertEquals(readA(a), o);
  assertEquals(readA(b), o);
  assertOptimized(readA);

  function readAFromB(x) {
    return x.a;
  }
  assertUnoptimized(readAFromB);
  readAFromB(b); readAFromB(b); readAFromB(b);
  %OptimizeFunctionOnNextCall(readAFromB);
  assertEquals(readAFromB(b), o);
  assertOptimized(readAFromB);

  var c = new A();
  c.c = o;
  assertOptimized(readA);
  assertOptimized(readAFromB);
  c.a = [1];
  assertUnoptimized(readA);
  assertUnoptimized(readAFromB);
  assertEquals(readA(a), o);
  assertEquals(readA(b), o);
  assertEquals(readA(c), [1]);
  assertEquals(readAFromB(b), o);

  %OptimizeFunctionOnNextCall(readA);
  assertEquals(readA(a), o);
  %OptimizeFunctionOnNextCall(readAFromB);
  assertEquals(readAFromB(b), o);
  assertOptimized(readA);
  a.a = [1];
  assertEquals(readA(a), [1]);
  assertEquals(readA(b), o);
  assertEquals(readA(c), [1]);
  assertOptimized(readA);
  b.a = [1];
  assertEquals(readA(a), [1]);
  assertEquals(readA(b), [1]);
  assertEquals(readA(c), [1]);
  assertOptimized(readA);
  assertOptimized(readAFromB);
})();

(function() {
  function A() { this.x = 0; }
  A.prototype = {y: 20};
  function B(o) { return o.a.y; }
  function C() { this.a = new A(); }

  B(new C());
  B(new C());
  %OptimizeFunctionOnNextCall(B);
  var c = new C();
  assertEquals(20, B(c));
  assertOptimized(B);
  c.a.y = 10;
  assertEquals(10, B(c));
  assertUnoptimized(B);

  var c = new C();
  %OptimizeFunctionOnNextCall(B);
  assertEquals(20, B(c));
  assertOptimized(B);
  c.a.y = 30;
  assertEquals(30, B(c));
  assertOptimized(B);
})();

(function() {
  var x = new Object();
  x.a = 1 + "Long string that results in a cons string";
  x = JSON.parse('{"a":"Short"}');
})();

(function() {
  var x = {y: {z: 1}};
  x.y.z = 1.1;
})();

(function() {
  function Foo(x) { this.x = x; }
  var f1 = new Foo({x: 1});
  var f2 = new Foo({x: 2});
  var f3 = new Foo({x: 3});
  function readX(f) { return f.x.x; }
  assertEquals(readX(f1), 1);
  assertEquals(readX(f2), 2);
  assertUnoptimized(readX);
  %OptimizeFunctionOnNextCall(readX);
  assertEquals(readX(f3), 3);
  assertOptimized(readX);
  function writeX(f, x) { f.x = x; }
  writeX(f1, {x: 11});
  writeX(f2, {x: 22});
  assertUnoptimized(writeX);
  assertEquals(readX(f1), 11);
  assertEquals(readX(f2), 22);
  assertOptimized(readX);
  %OptimizeFunctionOnNextCall(writeX);
  writeX(f3, {x: 33});
  assertEquals(readX(f3), 33);
  assertOptimized(readX);
  assertOptimized(writeX);
  function addY(f, y) { f.y = y; }
  writeX(f1, {a: "a"});
  assertUnoptimized(readX);
  assertUnoptimized(writeX);
})();

(function() {
  function Narf(x) { this.x = x; }
  var f1 = new Narf(1);
  var f2 = new Narf(2);
  var f3 = new Narf(3);
  function baz(f, y) { f.y = y; }
  baz(f1, {y: 9});
  baz(f2, {y: 9});
  %OptimizeFunctionOnNextCall(baz);
  baz(f3, {a: -1});
  assertUnoptimized(baz);
})();

(function() {
  function Foo(x) { this.x = x; this.a = x; }
  function Bar(x) { this.x = x; this.b = x; }
  function readA(o) { return o.x.a; }
  var f = new Foo({a:1});
  var b = new Bar({a:2});
  assertEquals(readA(f), 1);
  assertEquals(readA(b), 2);
  assertEquals(readA(f), 1);
  assertEquals(readA(b), 2);
  %OptimizeFunctionOnNextCall(readA);
  assertEquals(readA(f), 1);
  assertEquals(readA(b), 2);
  assertOptimized(readA);
  f.a.y = 0;
  assertUnoptimized(readA);
})();
