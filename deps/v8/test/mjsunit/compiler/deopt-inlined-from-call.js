// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//   * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above
//   copyright notice, this list of conditions and the following
//   disclaimer in the documentation and/or other materials provided
//   with the distribution.
//   * Neither the name of Google Inc. nor the names of its
//   contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
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

// Flags: --allow-natives-syntax --opt --no-always-opt

var global = this;

Array.prototype.f = function() {
  return 0;
};

(function() {
  var called = 0;

  function g(x, y, called) {
    return called + 1;
  }

  function f(deopt, called) {
    return g([].f.call({}), deopt + 1, called);
  }

  %PrepareFunctionForOptimization(f);
  called = f(0, called);
  called = f(0, called);
  %OptimizeFunctionOnNextCall(f);
  called = f(0, called);
  assertOptimized(f);
  called = f({}, called);
  assertUnoptimized(f);
  assertEquals(4, called);
})();

(function() {
  // The array built-ins are only inlined if the receiver is a
  // HConstant, this seems to require a *unique* global identifier
  // each time.
  global.a1 = [1,2,3,4];
  var obj = {value: 3};

  function f(b) {
    return [].pop.call(a1) + b.value;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(7, f(obj));
  assertEquals(6, f(obj));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(5, f(obj));
  assertOptimized(f);
  assertEquals(4, f({d: 0, value: 3}));
  assertUnoptimized(f);
  assertEquals(0, a1.length);
})();


(function() {
  global.a2 = [1,2,3,4];
  var obj = {value: 3};

  function f(b) {
    return [].shift.call(a2) + b.value;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(4, f(obj));
  assertEquals(5, f(obj));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(6, f(obj));
  assertOptimized(f);
  assertEquals(7, f({d: 0, value: 3}));
  assertUnoptimized(f);
  assertEquals(0, a2.length);
})();

(function() {
  global.a3 = [1,2,3,4];
  var obj = {value: 3};

  function f(b) {
    return [].push.call(a3, b.value);
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(5, f(obj));
  assertEquals(6, f(obj));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(7, f(obj));
  assertOptimized(f);
  assertEquals(8, f({d: 0, value: 3}));
  assertUnoptimized(f);
  assertEquals(8, a3.length);
  assertEquals(3, a3[7]);
})();

(function() {
  global.a4 = [1,2,3,4];
  var obj = {value: 3};

  function f(b) {
    return [].indexOf.call(a4, b.value);
  }

  %PrepareFunctionForOptimization(f);
  f(obj);
  f(obj);
  %OptimizeFunctionOnNextCall(f);
  var index1 = f(obj);
  assertOptimized(f);
  var index2 = f({d: 0, value: 3});
  assertUnoptimized(f);

  assertEquals(2, index1);
  assertEquals(index1, index2);
})();

(function() {
  global.a5 = [1,2,3,4];
  var obj = {value: 3};

  function f(b) {
    return [].lastIndexOf.call(a5, b.value);
  }

  %PrepareFunctionForOptimization(f);
  f(obj);
  f(obj);
  %OptimizeFunctionOnNextCall(f);
  var index1 = f(obj);
  assertOptimized(f);
  var index2 = f({d: 0, value: 3});
  assertUnoptimized(f);

  assertEquals(2, index1);
  assertEquals(index1, index2);
})();
