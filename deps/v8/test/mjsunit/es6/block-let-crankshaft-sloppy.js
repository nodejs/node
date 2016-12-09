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

// Check that the following functions are optimizable.
var functions = [ f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14,
                  f15, f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26,
                  f27, f28, f29, f30, f31, f32, f33];

for (var i = 0; i < functions.length; ++i) {
  var func = functions[i];
  print("Testing:");
  print(func);
  for (var j = 0; j < 10; ++j) {
    func(12);
  }
  %OptimizeFunctionOnNextCall(func);
  func(12);
  assertOptimized(func);
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

function f24() {
  let x = 1;
  {
    let x = 2;
    {
      let x = 3;
      assertEquals(3, x);
    }
    assertEquals(2, x);
  }
  assertEquals(1, x);
}

function f25() {
  {
    let x = 2;
    L: {
      let x = 3;
      assertEquals(3, x);
      break L;
      assertTrue(false);
    }
    assertEquals(2, x);
  }
  assertTrue(true);
}

function f26() {
  {
    let x = 1;
    L: {
      let x = 2;
      {
        let x = 3;
        assertEquals(3, x);
        break L;
        assertTrue(false);
      }
      assertTrue(false);
    }
    assertEquals(1, x);
  }
}


function f27() {
  do {
    let x = 4;
    assertEquals(4,x);
    {
      let x = 5;
      assertEquals(5, x);
      continue;
      assertTrue(false);
    }
  } while (false);
}

function f28() {
  label: for (var i = 0; i < 10; ++i) {
    let x = 'middle' + i;
    for (var j = 0; j < 10; ++j) {
      let x = 'inner' + j;
      continue label;
    }
  }
}

function f29() {
  // Verify that the context is correctly set in the stack frame after exiting
  // from with.

  let x = 'outer';
  label: {
    let x = 'inner';
    break label;
  }
  f();  // The context could be restored from the stack after the call.
  assertEquals('outer', x);

  function f() {
    assertEquals('outer', x);
  };
}

function f30() {
  let x = 'outer';
  for (var i = 0; i < 10; ++i) {
    let x = 'inner';
    continue;
  }
  f();
  assertEquals('outer', x);

  function f() {
    assertEquals('outer', x);
  };
}

function f31() {
  {
    let x = 'outer';
    label: for (var i = 0; assertEquals('outer', x), i < 10; ++i) {
      let x = 'middle' + i;
      {
        let x = 'inner' + j;
        continue label;
      }
    }
    assertEquals('outer', x);
  }
}

var c = true;

function f32() {
  {
    let x = 'outer';
    L: {
      {
        let x = 'inner';
        if (c) {
          break L;
        }
      }
      foo();
    }
  }

  function foo() {
    return 'bar';
  }
}

function f33() {
  {
    let x = 'outer';
    L: {
      {
        let x = 'inner';
        if (c) {
          break L;
        }
        foo();
      }
    }
  }

  function foo() {
    return 'bar';
  }
}

function TestThrow() {
  function f() {
    let x = 'outer';
    {
      let x = 'inner';
      throw x;
    }
  }
  for (var i = 0; i < 5; i++) {
    try {
      f();
    } catch (e) {
      assertEquals('inner', e);
    }
  }
  %OptimizeFunctionOnNextCall(f);
  try {
    f();
  } catch (e) {
    assertEquals('inner', e);
  }
  assertOptimized(f);
}

TestThrow();

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

function TestBlockLocal(s) {
  'use strict';
  var func = eval("(function baz(){ { " + s + "; } })");
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

function TestBlockContext(s) {
  'use strict';
  var func = eval("(function baz(){ { " + s + "; (function() { x; }); } })");
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
  TestBlockLocal(s);
  TestBlockContext(s);
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


function f(x) {
  let y = x + 42;
  return y;
}

function g(x) {
  {
    let y = x + 42;
    return y;
  }
}

for (var i=0; i<10; i++) {
  f(i);
  g(i);
}

%OptimizeFunctionOnNextCall(f);
%OptimizeFunctionOnNextCall(g);

f(12);
g(12);

assertTrue(%GetOptimizationStatus(f) != 2);
assertTrue(%GetOptimizationStatus(g) != 2);
