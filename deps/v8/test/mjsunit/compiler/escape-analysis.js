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

// Flags: --allow-natives-syntax --use-escape-analysis


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
