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

// Flags: --harmony-scoping --allow-natives-syntax

// TODO(ES6): properly activate extended mode
"use strict";

// Check that the following functions are optimizable.
var functions = [ f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14,
                  f15, f16, f17, f18, f19, f20, f21, f22, f23 ];

for (var i = 0; i < functions.length; ++i) {
  var func = functions[i];
  print("Testing:");
  print(func);
  for (var j = 0; j < 10; ++j) {
    func(12);
  }
  %OptimizeFunctionOnNextCall(func);
  func(12);
  assertTrue(%GetOptimizationStatus(func) != 2);
}

function f1() { }

function f2(x) { }

function f3() {
  let x;
}

function f4() {
  function foo() {
  }
}

function f5() {
  let x = 1;
}

function f6() {
  const x = 1;
}

function f7(x) {
  return x;
}

function f8() {
  let x;
  return x;
}

function f9() {
  function x() {
  }
  return x;
}

function f10(x) {
  x = 1;
}

function f11() {
  let x;
  x = 1;
}

function f12() {
  function x() {};
  x = 1;
}

function f13(x) {
  (function() { x; });
}

function f14() {
  let x;
  (function() { x; });
}

function f15() {
  function x() {
  }
  (function() { x; });
}

function f16() {
  let x = 1;
  (function() { x; });
}

function f17() {
  const x = 1;
  (function() { x; });
}

function f18(x) {
  return x;
  (function() { x; });
}

function f19() {
  let x;
  return x;
  (function() { x; });
}

function f20() {
  function x() {
  }
  return x;
  (function() { x; });
}

function f21(x) {
  x = 1;
  (function() { x; });
}

function f22() {
  let x;
  x = 1;
  (function() { x; });
}

function f23() {
  function x() { }
  x = 1;
  (function() { x; });
}


// Test that temporal dead zone semantics for function and block scoped
// let bindings are handled by the optimizing compiler.

function TestFunctionLocal(s) {
  'use strict';
  var func = eval("(function baz(){" + s + "; })");
  print("Testing:");
  print(func);
  for (var i = 0; i < 5; ++i) {
    try {
      func();
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, ReferenceError);
    }
  }
  %OptimizeFunctionOnNextCall(func);
  try {
    func();
    assertUnreachable();
  } catch (e) {
    assertInstanceof(e, ReferenceError);
  }
}

function TestFunctionContext(s) {
  'use strict';
  var func = eval("(function baz(){ " + s + "; (function() { x; }); })");
  print("Testing:");
  print(func);
  for (var i = 0; i < 5; ++i) {
    print(i);
    try {
      func();
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, ReferenceError);
    }
  }
  print("optimize");
  %OptimizeFunctionOnNextCall(func);
  try {
    print("call");
    func();
    assertUnreachable();
  } catch (e) {
    print("catch");
    assertInstanceof(e, ReferenceError);
  }
}

function TestAll(s) {
  TestFunctionLocal(s);
  TestFunctionContext(s);
}

// Use before initialization in declaration statement.
TestAll('let x = x + 1');
TestAll('let x = x += 1');
TestAll('let x = x++');
TestAll('let x = ++x');
TestAll('const x = x + 1');

// Use before initialization in prior statement.
TestAll('x + 1; let x;');
TestAll('x = 1; let x;');
TestAll('x += 1; let x;');
TestAll('++x; let x;');
TestAll('x++; let x;');
TestAll('let y = x; const x = 1;');


function f(x, b) {
  let y = (b ? y : x) + 42;
  return y;
}

function g(x, b) {
  {
    let y = (b ? y : x) + 42;
    return y;
  }
}

for (var i=0; i<10; i++) {
  f(i, false);
  g(i, false);
}

%OptimizeFunctionOnNextCall(f);
%OptimizeFunctionOnNextCall(g);

try {
  f(42, true);
} catch (e) {
  assertInstanceof(e, ReferenceError);
}

try {
  g(42, true);
} catch (e) {
  assertInstanceof(e, ReferenceError);
}
