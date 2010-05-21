// Copyright 2010 the V8 project authors. All rights reserved.
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

// Tests for compound assignments at the top level

z = 2;
z += 4;

assertEquals(z, 6);

a = new Array(10);

a[2] += 7;
a[2] = 15;
a[2] += 2;

assertEquals(17, a[2]);

b = new Object();
b.foo = 5;
b.foo += 12;

assertEquals(17, b.foo);

// Test compound assignments in an anonymous function with local variables.
(function () {
  var z = 2;
  z += 4;

  assertEquals(z, 6);

  var a = new Array(10);

  a[2] += 7;
  a[2] = 15;
  a[2] += 2;

  assertEquals(17, a[2]);

  var b = new Object();
  b.foo = 5;
  b.foo += 12;

  assertEquals(17, b.foo);
})();

// Test compound assignments in an anonymous function with global variables.
(function () {
  z = 2;
  z += 4;

  assertEquals(z, 6);

  a = new Array(10);

  a[2] += 7;
  a[2] = 15;
  a[2] += 2;

  assertEquals(17, a[2]);

  b = new Object();
  b.foo = 5;
  b.foo += 12;

  assertEquals(17, b.foo);
})();

// Test compound assignments in a named function with local variables.
function foo() {
  var z = 3;
  z += 4;

  assertEquals(z, 7);

  var a = new Array(10);

  a[2] += 7;
  a[2] = 15;
  a[2] += 2;

  assertEquals(17, a[2]);

  var b = new Object();
  b.foo = 5;
  b.foo += 12;

  assertEquals(17, b.foo);
}

foo();

// Test compound assignments in a named function with global variables.
function bar() {
  z = 2;
  z += 5;

  assertEquals(z, 7);

  a = new Array(10);

  a[2] += 7;
  a[2] = 15;
  a[2] += 2;

  assertEquals(17, a[2]);

  b = new Object();
  b.foo = 5;
  b.foo += 12;

  assertEquals(17, b.foo);
}

bar();

// Entire series of tests repeated, in loops.
// -------------------------------------------
// Tests for compound assignments in a loop at the top level

for (i = 0; i < 5; ++i) {
  z = 2;
  z += 4;

  assertEquals(z, 6);

  a = new Array(10);

  a[2] += 7;
  a[2] = 15;
  a[2] += 2;

  assertEquals(17, a[2]);

  b = new Object();
  b.foo = 5;
  b.foo += 12;

  assertEquals(17, b.foo);
}

// Test compound assignments in an anonymous function with local variables.
(function () {
  for (var i = 0; i < 5; ++i) {
    var z = 2;
    z += 4;

    assertEquals(z, 6);

    var a = new Array(10);

    a[2] += 7;
    a[2] = 15;
    a[2] += 2;

    assertEquals(17, a[2]);

    var b = new Object();
    b.foo = 5;
    b.foo += 12;

    assertEquals(17, b.foo);
  }
})();

// Test compound assignments in an anonymous function with global variables.
(function () {
  for (i = 0; i < 5; ++i) {
    z = 2;
    z += 4;

    assertEquals(z, 6);

    a = new Array(10);

    a[2] += 7;
    a[2] = 15;
    a[2] += 2;

    assertEquals(17, a[2]);

    b = new Object();
    b.foo = 5;
    b.foo += 12;

    assertEquals(17, b.foo);
  }
})();

// Test compound assignments in a named function with local variables.
function foo_loop() {
  for (i = 0; i < 5; ++i) {
    var z = 3;
    z += 4;

    assertEquals(z, 7);

    var a = new Array(10);

    a[2] += 7;
    a[2] = 15;
    a[2] += 2;

    assertEquals(17, a[2]);

    var b = new Object();
    b.foo = 5;
    b.foo += 12;

    assertEquals(17, b.foo);
  }
}

foo_loop();

// Test compound assignments in a named function with global variables.
function bar_loop() {
  for (i = 0; i < 5; ++i) {
    z = 2;
    z += 5;

    assertEquals(z, 7);

    a = new Array(10);

    a[2] += 7;
    a[2] = 15;
    a[2] += 2;

    assertEquals(17, a[2]);

    b = new Object();
    b.foo = 5;
    b.foo += 12;

    assertEquals(17, b.foo);
  }
}

bar_loop();


// Test for assignment using a keyed store ic:
function store_i_in_element_i_of_object_i() {
  var i = new Object();
  i[i] = i;
}

// Run three times to exercise caches.
store_i_in_element_i_of_object_i();
store_i_in_element_i_of_object_i();
store_i_in_element_i_of_object_i();
