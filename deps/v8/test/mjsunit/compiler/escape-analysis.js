// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --use-escape-analysis --expose-gc


// Test stores on a join path.
(function testJoin() {
  function constructor() {
    this.a = 0;
  }
  function join(mode, expected) {
    var object = new constructor();
    if (mode) {
      object.a = 1;
    } else {
      object.a = 2;
    }
    assertEquals(expected, object.a);
  }
  join(true, 1); join(true, 1);
  join(false, 2); join(false, 2);
  %OptimizeFunctionOnNextCall(join);
  join(true, 1); join(false, 2);
})();


// Test loads and stores inside a loop.
(function testLoop() {
  function constructor() {
    this.a = 0;
    this.b = 23;
  }
  function loop() {
    var object = new constructor();
    for (var i = 1; i < 10; i++) {
      object.a = object.a + i;
      assertEquals(i*(i+1)/2, object.a);
      assertEquals(23, object.b);
    }
    assertEquals(45, object.a);
    assertEquals(23, object.b);
  }
  loop(); loop();
  %OptimizeFunctionOnNextCall(loop);
  loop(); loop();
})();


// Test loads and stores inside nested loop.
(function testNested() {
  function constructor() {
    this.a = 0;
    this.b = 0;
    this.c = 23;
  }
  function nested() {
    var object = new constructor();
    for (var i = 1; i < 10; i++) {
      object.a = object.a + i;
      assertEquals(i*(i+1)/2, object.a);
      assertEquals((i-1)*6, object.b);
      assertEquals(23, object.c);
      for (var j = 1; j < 4; j++) {
        object.b = object.b + j;
        assertEquals(i*(i+1)/2, object.a);
        assertEquals((i-1)*6+j*(j+1)/2, object.b);
        assertEquals(23, object.c);
      }
      assertEquals(i*(i+1)/2, object.a);
      assertEquals(i*6, object.b);
      assertEquals(23, object.c);
    }
    assertEquals(45, object.a);
    assertEquals(54, object.b);
    assertEquals(23, object.c);
  }
  nested(); nested();
  %OptimizeFunctionOnNextCall(nested);
  nested(); nested();
})();


// Test deoptimization with captured objects in local variables.
(function testDeoptLocal() {
  var deopt = { deopt:false };
  function constructor1() {
    this.a = 1.0;
    this.b = 2.3;
    this.c = 3.0;
  }
  function constructor2(o) {
    this.d = o;
    this.e = 4.5;
  }
  function func() {
    var o1 = new constructor1();
    var o2 = new constructor2(o1);
    deopt.deopt;
    assertEquals(1.0, o1.a);
    assertEquals(2.3, o2.d.b);
    assertEquals(3.0, o2.d.c);
    assertEquals(4.5, o2.e);
  }
  func(); func();
  %OptimizeFunctionOnNextCall(func);
  func(); func();
  delete deopt.deopt;
  func(); func();
})();


// Test deoptimization with captured objects on operand stack.
(function testDeoptOperand() {
  var deopt = { deopt:false };
  function constructor1() {
    this.a = 1.0;
    this.b = 2.3;
    deopt.deopt;
    assertEquals(1.0, this.a);
    assertEquals(2.3, this.b);
    this.b = 2.7;
    this.c = 3.0;
    this.d = 4.5;
  }
  function constructor2() {
    this.e = 5.0;
    this.f = new constructor1();
    assertEquals(1.0, this.f.a);
    assertEquals(2.7, this.f.b);
    assertEquals(3.0, this.f.c);
    assertEquals(4.5, this.f.d);
    assertEquals(5.0, this.e);
    this.e = 5.9;
    this.g = 6.7;
  }
  function func() {
    var o = new constructor2();
    assertEquals(1.0, o.f.a);
    assertEquals(2.7, o.f.b);
    assertEquals(3.0, o.f.c);
    assertEquals(4.5, o.f.d);
    assertEquals(5.9, o.e);
    assertEquals(6.7, o.g);
  }
  func(); func();
  %OptimizeFunctionOnNextCall(func);
  func(); func();
  delete deopt.deopt;
  func(); func();
})();


// Test map checks on captured objects.
(function testMapCheck() {
  var sum = 0;
  function getter() { return 27; }
  function setter(v) { sum += v; }
  function constructor() {
    this.x = 23;
    this.y = 42;
  }
  function check(x, y) {
    var o = new constructor();
    assertEquals(x, o.x);
    assertEquals(y, o.y);
  }
  var monkey = Object.create(null, {
    x: { get:getter, set:setter },
    y: { get:getter, set:setter }
  });
  check(23, 42); check(23, 42);
  %OptimizeFunctionOnNextCall(check);
  check(23, 42); check(23, 42);
  constructor.prototype = monkey;
  check(27, 27); check(27, 27);
  assertEquals(130, sum);
})();


// Test OSR into a loop with captured objects.
(function testOSR() {
  function constructor() {
    this.a = 23;
  }
  function osr1(length) {
    assertEquals(23, (new constructor()).a);
    var result = 0;
    for (var i = 0; i < length; i++) {
      result = (result + i) % 99;
    }
    return result;
  }
  function osr2(length) {
    var result = 0;
    for (var i = 0; i < length; i++) {
      result = (result + i) % 99;
    }
    assertEquals(23, (new constructor()).a);
    return result;
  }
  function osr3(length) {
    var result = 0;
    var o = new constructor();
    for (var i = 0; i < length; i++) {
      result = (result + i) % 99;
    }
    assertEquals(23, o.a);
    return result;
  }
  function test(closure) {
    assertEquals(45, closure(10));
    assertEquals(45, closure(10));
    assertEquals(10, closure(50000));
  }
  test(osr1);
  test(osr2);
  test(osr3);
})();


// Test out-of-bounds access on captured objects.
(function testOOB() {
  function cons1() {
    this.x = 1;
    this.y = 2;
    this.z = 3;
  }
  function cons2() {
    this.a = 7;
  }
  function oob(constructor, branch) {
    var o = new constructor();
    if (branch) {
      return o.a;
    } else {
      return o.z;
    }
  }
  assertEquals(3, oob(cons1, false));
  assertEquals(3, oob(cons1, false));
  assertEquals(7, oob(cons2, true));
  assertEquals(7, oob(cons2, true));
  gc();  // Clears type feedback of constructor call.
  assertEquals(7, oob(cons2, true));
  assertEquals(7, oob(cons2, true));
  %OptimizeFunctionOnNextCall(oob);
  assertEquals(7, oob(cons2, true));
})();


// Test non-shallow nested graph of captured objects.
(function testDeep() {
  var deopt = { deopt:false };
  function constructor1() {
    this.x = 23;
  }
  function constructor2(nested) {
    this.a = 17;
    this.b = nested;
    this.c = 42;
  }
  function deep() {
    var o1 = new constructor1();
    var o2 = new constructor2(o1);
    assertEquals(17, o2.a);
    assertEquals(23, o2.b.x);
    assertEquals(42, o2.c);
    o1.x = 99;
    deopt.deopt;
    assertEquals(99, o1.x);
    assertEquals(99, o2.b.x);
  }
  deep(); deep();
  %OptimizeFunctionOnNextCall(deep);
  deep(); deep();
  delete deopt.deopt;
  deep(); deep();
})();


// Test non-shallow nested graph of captured objects with duplicates
(function testDeepDuplicate() {
  function constructor1() {
    this.x = 23;
  }
  function constructor2(nested) {
    this.a = 17;
    this.b = nested;
    this.c = 42;
  }
  function deep(shouldDeopt) {
    var o1 = new constructor1();
    var o2 = new constructor2(o1);
    var o3 = new constructor2(o1);
    assertEquals(17, o2.a);
    assertEquals(23, o2.b.x);
    assertEquals(42, o2.c);
    o3.c = 54;
    o1.x = 99;
    if (shouldDeopt) %DeoptimizeFunction(deep);
    assertEquals(99, o1.x);
    assertEquals(99, o2.b.x);
    assertEquals(99, o3.b.x);
    assertEquals(54, o3.c);
    assertEquals(17, o3.a);
    assertEquals(42, o2.c);
    assertEquals(17, o2.a);
    o3.b.x = 1;
    assertEquals(1, o1.x);
  }
  deep(false); deep(false);
  %OptimizeFunctionOnNextCall(deep);
  deep(false); deep(false);
  deep(true); deep(true);
})();


// Test non-shallow nested graph of captured objects with inline
(function testDeepInline() {
  function h() {
    return { y : 3 };
  }

  function g(x) {
    var u = { x : h() };
    %DeoptimizeFunction(f);
    return u;
  }

  function f() {
    var l = { dummy : { } };
    var r = g(l);
    assertEquals(3, r.x.y);
  }

  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();


// Test two nested objects
(function testTwoNestedObjects() {
  function f() {
    var l = { x : { y : 111 } };
    var l2 = { x : { y : 111 } };
    %DeoptimizeFunction(f);
    assertEquals(111, l.x.y);
    assertEquals(111, l2.x.y);
  }

  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();


// Test a nested object and a duplicate
(function testTwoObjectsWithDuplicate() {
  function f() {
    var l = { x : { y : 111 } };
    var dummy = { d : 0 };
    var l2 = l.x;
    %DeoptimizeFunction(f);
    assertEquals(111, l.x.y);
    assertEquals(111, l2.y);
    assertEquals(0, dummy.d);
  }

  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();


// Test materialization of a field that requires a Smi value.
(function testSmiField() {
  var deopt = { deopt:false };
  function constructor() {
    this.x = 1;
  }
  function field(x) {
    var o = new constructor();
    o.x = x;
    deopt.deopt
    assertEquals(x, o.x);
  }
  field(1); field(2);
  %OptimizeFunctionOnNextCall(field);
  field(3); field(4);
  delete deopt.deopt;
  field(5.5); field(6.5);
})();


// Test materialization of a field that requires a heap object value.
(function testHeapObjectField() {
  var deopt = { deopt:false };
  function constructor() {
    this.x = {};
  }
  function field(x) {
    var o = new constructor();
    o.x = x;
    deopt.deopt
    assertEquals(x, o.x);
  }
  field({}); field({});
  %OptimizeFunctionOnNextCall(field);
  field({}); field({});
  delete deopt.deopt;
  field(1); field(2);
})();
