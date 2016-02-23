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

// Flags: --allow-natives-syntax --max-opt-count=100 --noalways-opt
// Flags: --nocollect-maps

// We specify max-opt-count because we opt/deopt the same function many
// times.

// We specify nocollect-maps because in gcstress we can end up deoptimizing
// a function in a gc in the stack guard at the beginning of the (optimized)
// function due to leftover map clearing work that results in deoptimizing
// dependent code from those maps. The choice is to insert strategic gc()
// calls or specify this flag.

// It's nice to run this in other browsers too.
var standalone = false;
if (standalone) {
  assertTrue = function(val) {
    if (val != true) {
      print("FAILURE");
    }
  }

  assertFalse = function(val) {
    if (val != false) {
      print("FAILURE");
    }
  }

  assertEquals = function(expected, val) {
    if (expected !== val) {
      print("FAILURE");
    }
  }

  empty_func = function(name) { }
  assertUnoptimized = empty_func;
  assertOptimized = empty_func;

  optimize = empty_func;
  clearFunctionTypeFeedback = empty_func;
  deoptimizeFunction = empty_func;
} else {
  optimize = function(name) {
    %OptimizeFunctionOnNextCall(name);
  }
  clearFunctionTypeFeedback = function(name) {
    %ClearFunctionTypeFeedback(name);
  }
  deoptimizeFunction = function(name) {
    %DeoptimizeFunction(name);
  }
}

function base_getter_test(create_func) {
  var calls = 0;

  // Testcase: setter in prototype chain
  foo = function(a) { var x = a[0]; return x + 3; }
  var a = create_func();
  var ap = [];
  ap.__defineGetter__(0, function() { calls++; return 0; });

  foo(a);
  foo(a);
  foo(a);
  delete a[0];

  assertEquals(0, calls);
  a.__proto__ = ap;
  foo(a);
  assertEquals(1, calls);
  optimize(foo);
  foo(a);
  assertEquals(2, calls);
  assertOptimized(foo);

  // Testcase: getter "deep" in prototype chain.
  clearFunctionTypeFeedback(foo);
  deoptimizeFunction(foo);
  clearFunctionTypeFeedback(foo);
  calls = 0;

  a = create_func();
  var ap2 = [];
  a.__proto__ = ap2;
  foo(a);
  foo(a);
  foo(a);
  delete a[0];

  assertEquals(0, calls);

  ap2.__proto__ = ap;  // "sneak" in a callback.
  // The sneak case should be caught by unoptimized code too.
  assertUnoptimized(foo);
  foo(a);
  foo(a);
  foo(a);
  assertEquals(3, calls);

  // Testcase: getter added after optimization (feedback is monomorphic)
  clearFunctionTypeFeedback(foo);
  deoptimizeFunction(foo);
  clearFunctionTypeFeedback(foo);
  calls = 0;

  a = create_func();
  ap2 = [];
  a.__proto__ = ap2;
  foo(a);
  foo(a);
  foo(a);
  optimize(foo);
  foo(a);
  assertOptimized(foo);
  delete a[0];
  ap2.__proto__ = ap;
  foo(a);
  assertOptimized(foo);  // getters don't require deopt on shape change.
  assertEquals(1, calls);

  // Testcase: adding additional getters to a prototype chain that already has
  // one shouldn't deopt anything.
  clearFunctionTypeFeedback(foo);
  calls = 0;

  a = create_func();
  a.__proto__ = ap2;
  bar = function(a) { return a[3] + 600; }
  bar(a);
  bar(a);
  bar(a);
  optimize(bar);
  bar(a);
  assertOptimized(bar);
  assertEquals(0, calls);
  delete a[3];
  ap2.__defineGetter__(3, function() { calls++; return 0; });
  bar(a);
  assertOptimized(bar);
  assertEquals(1, calls);
}

// Verify that map transitions don't confuse us.
create_func_smi = function() { return [,,,,,,5]; }
create_func_double = function() { return [,,,,,,5.5]; }
create_func_fast = function() { return [,,,,,,true]; }

var cf = [create_func_smi,
          create_func_double,
          create_func_fast];

for(var c = 0; c < cf.length; c++) {
  base_getter_test(cf[c]);
}

// A special test for LoadKeyedHoleMode. Ensure that optimized is generated
// which sets ALLOW_RETURN_HOLE, then add a setter on the prototype that should
// cause the function to deoptimize.

var a = [3.5,,,3.5];
fun = function(a) { return a[0] + 5.5; }
fun(a);
fun(a);
fun(a);  // should have a monomorphic KeyedLoadIC.
optimize(fun);
fun(a);
assertOptimized(fun);

// returning undefined shouldn't phase us.
delete a[0];
fun(a);
assertOptimized(fun);

// but messing up the prototype chain will.
a.__proto__ = [];
fun(a);
assertUnoptimized(fun);

// Construct a non-trivial prototype chain.
var a = [3.5,,,,3.5];
var ap = [,,3.5];
ap.__proto__ = a.__proto__;
a.__proto__ = ap;
fun(a);
optimize(fun);
fun(a);
assertOptimized(fun);

var calls = 0;
delete a[0];
ap.__defineGetter__(0, function() { calls++; return 0; });
fun(a);
assertEquals(1, calls);
assertUnoptimized(fun);
