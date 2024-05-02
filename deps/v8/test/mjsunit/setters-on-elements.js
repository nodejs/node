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

// Flags: --allow-natives-syntax --noalways-turbofan --turbofan

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
  prepareForOptimize = empty_func;
} else {
  optimize = function(name) {
    %OptimizeFunctionOnNextCall(name);
  }
  clearFunctionTypeFeedback = function(name) {
    %ClearFunctionFeedback(name);
  }
  deoptimizeFunction = function(name) {
    %DeoptimizeFunction(name);
  }
  prepareForOptimize = function(name) {
    %PrepareFunctionForOptimization(name);
  }
}

function base_setter_test(create_func, index, store_value) {
  var calls = 0;

  // Testcase: setter in prototype chain
  foo = function(a) { a[index] = store_value; }
  var a = create_func();
  var ap = [];
  ap.__defineSetter__(index, function() { calls++; });

  prepareForOptimize(foo);
  foo(a);
  foo(a);
  foo(a);
  delete a[index];

  assertEquals(0, calls);
  a.__proto__ = ap;
  foo(a);
  assertEquals(1, calls);
  optimize(foo);
  foo(a);
  assertEquals(2, calls);
  assertOptimized(foo);

  // Testcase: setter added on prototype chain object already in place.
  clearFunctionTypeFeedback(foo);
  deoptimizeFunction(foo);
  clearFunctionTypeFeedback(foo);
  calls = 0;
  a = create_func();
  var apap = [];
  a.__proto__ = apap;
  foo(a);
  foo(a);
  foo(a);
  delete a[index];
  apap.__defineSetter__(index, function() { calls++; });
  foo(a);
  foo(a);
  foo(a);
  assertEquals(3, calls);

  // Testcase: setter "deep" in prototype chain.
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
  delete a[index];

  assertEquals(0, calls);

  ap2.__proto__ = ap;  // "sneak" in a callback.
  // The sneak case should be caught by unoptimized code too.
  assertUnoptimized(foo);
  foo(a);
  foo(a);
  foo(a);
  assertEquals(3, calls);

  // Testcase: setter added after optimization (feedback is monomorphic)
  clearFunctionTypeFeedback(foo);
  deoptimizeFunction(foo);
  clearFunctionTypeFeedback(foo);
  calls = 0;

  a = create_func();
  ap2 = [];
  a.__proto__ = ap2;
  prepareForOptimize(foo);
  foo(a);
  foo(a);
  foo(a);
  optimize(foo);
  foo(a);
  assertOptimized(foo);
  delete a[index];
  ap2.__proto__ = ap;
  foo(a);
  assertUnoptimized(foo);  // map shape change should deopt foo.
  assertEquals(1, calls);

  // Testcase: adding additional setters to a prototype chain that already has
  // one shouldn't deopt anything. (ie, we aren't changing the map shape).
  clearFunctionTypeFeedback(foo);
  calls = 0;

  a = create_func();
  a.__proto__ = ap2;
  bar = function(a) { a[index+1] = store_value; }
  prepareForOptimize(bar);
  bar(a);
  bar(a);
  bar(a);  // store should be generic
  optimize(bar);
  bar(a);
  assertOptimized(bar);
  assertEquals(0, calls);
  delete a[index+1];
  ap2.__defineSetter__(index+1, function() { calls++; });
  bar(a);
  assertOptimized(bar);
  assertEquals(1, calls);
}

// Verify that map transitions don't confuse us.
create_func_smi = function() { return [,,,,,,5]; }
create_func_double = function() { return [0,,3.2,,,,5.5]; }
create_func_fast = function() { return [,,,,,,true]; }
create_func_dictionary = function() { var a = []; a.length = 100000; return a; }

var cf = [create_func_smi,
          create_func_double,
          create_func_fast,
          create_func_dictionary];

var values = [3, 3.5, true];

for(var c = 0; c < cf.length; c++) {
  for(var s = 0; s < values.length; s++) {
    base_setter_test(cf[c], 0, values[s]);
    base_setter_test(cf[c], 1, values[s]);
  }
}
