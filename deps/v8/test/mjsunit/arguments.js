// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax

function argc0() {
  return arguments.length;
}

function argc1(i) {
  return arguments.length;
}

function argc2(i, j) {
  return arguments.length;
}

assertEquals(0, argc0());
assertEquals(1, argc0(1));
assertEquals(2, argc0(1, 2));
assertEquals(3, argc0(1, 2, 3));
assertEquals(0, argc1());
assertEquals(1, argc1(1));
assertEquals(2, argc1(1, 2));
assertEquals(3, argc1(1, 2, 3));
assertEquals(0, argc2());
assertEquals(1, argc2(1));
assertEquals(2, argc2(1, 2));
assertEquals(3, argc2(1, 2, 3));

var index;

function argv0() {
  return arguments[index];
}

function argv1(i) {
  return arguments[index];
}

function argv2(i, j) {
  return arguments[index];
}

index = 0;
assertEquals(7, argv0(7));
assertEquals(7, argv0(7, 8));
assertEquals(7, argv0(7, 8, 9));
assertEquals(7, argv1(7));
assertEquals(7, argv1(7, 8));
assertEquals(7, argv1(7, 8, 9));
assertEquals(7, argv2(7));
assertEquals(7, argv2(7, 8));
assertEquals(7, argv2(7, 8, 9));

index = 1;
assertEquals(8, argv0(7, 8));
assertEquals(8, argv0(7, 8));
assertEquals(8, argv1(7, 8, 9));
assertEquals(8, argv1(7, 8, 9));
assertEquals(8, argv2(7, 8, 9));
assertEquals(8, argv2(7, 8, 9));

index = 2;
assertEquals(9, argv0(7, 8, 9));
assertEquals(9, argv1(7, 8, 9));
assertEquals(9, argv2(7, 8, 9));


// Test that calling a lazily compiled function with
// an unexpected number of arguments works.
function f(a) { return arguments.length; };
assertEquals(3, f(1, 2, 3));

function f1(x, y) {
  function g(a) {
    a[0] = "three";
    return a.length;
  }
  var l = g(arguments);
  y = 5;
  assertEquals(2, l);
  assertEquals("three", x);
  assertEquals(5, y);
}
f1(3, "five");


function f2() {
  if (arguments[0] > 0) {
    return arguments.callee(arguments[0] - 1) + arguments[0];
  }
  return 0;
}
assertEquals(55, f2(10));


function f3() {
  assertEquals(0, arguments.length);
}
f3();


function f4() {
  var arguments = 0;
  assertEquals(void 0, arguments.length);
}
f4();


function f5(x, y, z) {
  function g(a) {
    x = "two";
    y = "three";
    a[1] = "drei";
    a[2] = "fuenf";
  };

  g(arguments);
  assertEquals("two", x);
  assertEquals("drei", y);
  assertEquals("fuenf", z);
}
f5(2, 3, 5);


function f6(x, y) {
  x = "x";
  arguments[1] = "y";
  return [arguments.length, arguments[0], y, arguments[2]];
}

assertArrayEquals([0, void 0, void 0, void 0], f6());
assertArrayEquals([1, "x", void 0, void 0], f6(1));
assertArrayEquals([2, "x", "y", void 0], f6(9, 17));
assertArrayEquals([3, "x", "y", 7], f6(3, 5, 7));
assertArrayEquals([4, "x", "y", "c"], f6("a", "b", "c", "d"));


function list_args(a) {
  assertEquals("function", typeof a.callee);
  var result = [];
  result.push(a.length);
  for (i = 0; i < a.length; i++) result.push(a[i]);
  return result;
}


function f7(x, y) {
  function g(p) {
    x = p;
  }
  g(y);
  return list_args(arguments);
}

assertArrayEquals([0], f7());
assertArrayEquals([1, void 0], f7(3));
assertArrayEquals([2, 5, 5], f7(3, 5));
assertArrayEquals([3, 5, 5, 7], f7(3, 5, 7));

// Check out of bounds behavior.
function arg_get(x) { return arguments[x]; }
function arg_del(x) { return delete arguments[x]; }
function arg_set(x) { return (arguments[x] = 117); }
assertEquals(undefined, arg_get(0xFFFFFFFF));
assertEquals(true, arg_del(0xFFFFFFFF));
assertEquals(117, arg_set(0xFFFFFFFF));

(function() {
  function f(a) { return arguments; }
  var a = f(1,2,3);
  // Turn arguments into slow.
  assertTrue(%HasSloppyArgumentsElements(a));
  a[10000] = 1;
  assertTrue(%HasSloppyArgumentsElements(a));
  // Make it fast again by adding values.
  for (var i = 0; i < 1000; i++) {
    a[i] = 1.5;
  }
  assertTrue(%HasSloppyArgumentsElements(a));
})();

(function testDeleteArguments() {
  function f() { return arguments };
  var args = f(1, 2);
  assertEquals(1, args[0]);
  assertEquals(2, args[1]);
  assertEquals(2, args.length);

  delete args[0];
  assertEquals(undefined, args[0]);
  assertEquals(2, args[1]);
  assertEquals(2, args.length);

  delete args[1];
  assertEquals(undefined, args[0]);
  assertEquals(undefined, args[1]);
  assertEquals(2, args.length);
})();

(function testDeleteFastSloppyArguments() {
  function f(a) { return arguments };
  var args = f(1, 2);
  assertEquals(1, args[0]);
  assertEquals(2, args[1]);
  assertEquals(2, args.length);

  delete args[0];
  assertEquals(undefined, args[0]);
  assertEquals(2, args[1]);
  assertEquals(2, args.length);

  delete args[1];
  assertEquals(undefined, args[0]);
  assertEquals(undefined, args[1]);
  assertEquals(2, args.length);
})();

(function testDeleteSlowSloppyArguments() {
  var key = 10000;
  function f(a) {
    arguments[key] = key;
    return arguments
  };
  var args = f(1, 2);
  %HeapObjectVerify(args);
  assertEquals(1, args[0]);
  assertEquals(2, args[1]);
  assertEquals(key, args[key]);
  assertEquals(2, args.length);

  delete args[0];
  %HeapObjectVerify(args);
  assertEquals(undefined, args[0]);
  assertEquals(2, args[1]);
  assertEquals(key, args[key]);
  assertEquals(2, args.length);

  delete args[1];
  %HeapObjectVerify(args);
  assertEquals(undefined, args[0]);
  assertEquals(undefined, args[1]);
  assertEquals(key, args[key]);
  assertEquals(2, args.length);

  delete args[key];
  %HeapObjectVerify(args);
  assertEquals(undefined, args[0]);
  assertEquals(undefined, args[1]);
  assertEquals(undefined, args[key]);
  assertEquals(2, args.length);
})();

(function testDeleteSlowSloppyArguments2() {
  function f(a) {
    return arguments
  };
  var args = f(1, 2);
  %HeapObjectVerify(args);
  assertEquals(1, args[0]);
  assertEquals(2, args[1]);
  assertEquals(2, args.length);

  delete args[1];
  %HeapObjectVerify(args);
  assertEquals(1, args[0]);
  assertEquals(undefined, args[1]);
  assertEquals(undefined, args[2]);
  assertEquals(2, args.length);

  delete args[0];
  %HeapObjectVerify(args);
  assertEquals(undefined, args[0]);
  assertEquals(undefined, args[1]);
  assertEquals(undefined, args[2]);
  assertEquals(2, args.length);
})();

(function testSloppyArgumentProperties() {
  function f(a, b) { return arguments }
  let args = f(1, 2, 3, 4);
  %HeapObjectVerify(args);
  assertEquals(4, args.length);
  args.foo = "foo";
  %HeapObjectVerify(args);
  assertEquals("foo", args.foo);
  assertEquals(4, args.length);

  delete args.foo;
  %HeapObjectVerify(args);
  assertEquals(undefined, args.foo);
  assertEquals(4, args.length);
})();


(function testSloppyArgumentsLengthMapChange() {
  function f(a) { return arguments };
  let args1 = f(1);
  let args2 = f(1,2);
  assertTrue(%HaveSameMap(args1, args2));
  args2.length = 12;
  assertTrue(%HaveSameMap(args1, args2));
  args2.length = "aa"
  assertTrue(%HaveSameMap(args1, args2));

  let args3 = f(1);
  let args4 = f(1,2);
  // Creating holes causes map transitions.
  assertTrue(%HaveSameMap(args1, args3));
  assertTrue(%HaveSameMap(args1, args4));
  delete args3[0];
  assertFalse(%HaveSameMap(args1, args3));
  delete args4[1];
  assertFalse(%HaveSameMap(args1, args4));
})();

(function testSloppyArgumentsLengthMapChange() {
  function f(a) { return arguments };
  let args1 = f(1);
  let args2 = f(1,2);
  assertTrue(%HaveSameMap(args1, args2));
  // Changing the length type doesn't causes a map transition.
  args2.length = 12;
  assertTrue(%HaveSameMap(args1, args2));
  args2.length = 12.0;
  assertTrue(%HaveSameMap(args1, args2));
  args2.length = "aa"
  assertTrue(%HaveSameMap(args1, args2));
})();


(function testArgumentsVerification() {
  (function f2(a,a) {
    %HeapObjectVerify(arguments);
  })(1,2);

  function f7(a,a,a,a,a,a,a) {
    %HeapObjectVerify(arguments);
  };
  f7(1,2,3,4,5,6);
  f7(1,2,3,4,5,6,7);
  f7(1,2,3,4,5,6,7,8);
})();

(function testArgumentsHole() {
  function f(a) {
    arguments[3] = 1;
    return arguments[2];
  };

  assertEquals(undefined, f(1));
  assertEquals(undefined, f(1));
  assertEquals(undefined, f(1));
})();
