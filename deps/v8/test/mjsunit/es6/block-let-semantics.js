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

"use strict";

// Test temporal dead zone semantics of let bound variables in
// function and block scopes.

function TestFunctionLocal(s) {
  try {
    eval("(function(){" + s + "; })")();
  } catch (e) {
    assertInstanceof(e, ReferenceError);
    return;
  }
  assertUnreachable();
}

function TestBlockLocal(s,e) {
  try {
    eval("(function(){ {" + s + ";} })")();
  } catch (e) {
    assertInstanceof(e, ReferenceError);
    return;
  }
  assertUnreachable();
}


function TestAll(s) {
  TestBlockLocal(s);
  TestFunctionLocal(s);
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
TestAll('let y = x; class x {}');

TestAll('f(); let x; function f() { return x + 1; }');
TestAll('f(); let x; function f() { x = 1; }');
TestAll('f(); let x; function f() { x += 1; }');
TestAll('f(); let x; function f() { ++x; }');
TestAll('f(); let x; function f() { x++; }');
TestAll('f(); const x = 1; function f() { return x; }');
TestAll('f(); class x { }; function f() { return x; }');

TestAll('f()(); let x; function f() { return function() { return x + 1; } }');
TestAll('f()(); let x; function f() { return function() { x = 1; } }');
TestAll('f()(); let x; function f() { return function() { x += 1; } }');
TestAll('f()(); let x; function f() { return function() { ++x; } }');
TestAll('f()(); let x; function f() { return function() { x++; } }');
TestAll('f()(); const x = 1; function f() { return function() { return x; } }');
TestAll('f()(); class x { }; function f() { return function() { return x; } }');

for (var kw of ['let x = 2', 'const x = 2', 'class x { }']) {
  // Use before initialization with a dynamic lookup.
  TestAll(`eval("x"); ${kw};`);
  TestAll(`eval("x + 1;"); ${kw};`);
  TestAll(`eval("x = 1;"); ${kw};`);
  TestAll(`eval("x += 1;"); ${kw};`);
  TestAll(`eval("++x;"); ${kw};`);
  TestAll(`eval("x++;"); ${kw};`);

  // Use before initialization with check for eval-shadowed bindings.
  TestAll(`function f() { eval("var y = 2;"); x + 1; }; f(); ${kw};`);
  TestAll(`function f() { eval("var y = 2;"); x = 1; }; f(); ${kw};`);
  TestAll(`function f() { eval("var y = 2;"); x += 1; }; f(); ${kw};`);
  TestAll(`function f() { eval("var y = 2;"); ++x; }; f(); ${kw};`);
  TestAll(`function f() { eval("var y = 2;"); x++; }; f(); ${kw};`);
}

// Test that variables introduced by function declarations are created and
// initialized upon entering a function / block scope.
function f() {
  {
    assertEquals(2, g1());
    assertEquals(2, eval("g1()"));

    // block scoped function declaration
    function g1() {
      return 2;
    }
  }

  assertEquals(3, g2());
  assertEquals(3, eval("g2()"));
  // function scoped function declaration
  function g2() {
    return 3;
  }
}
f();

// Test that a function declaration introduces a block scoped variable.
TestAll('{ function k() { return 0; } }; k(); ');

// Test that a function declaration sees the scope it resides in.
function f2() {
  let m, n, o, p;
  {
    m = g;
    function g() {
      return a;
    }
    let a = 1;
  }
  assertEquals(1, m());

  try {
    throw 2;
  } catch(b) {
    n = h;
    function h() {
      return b + c;
    }
    let c = 3;
  }
  assertEquals(5, n());

  {
    o = i;
    function i() {
      return d;
    }
    let d = 4;
  }
  assertEquals(4, o());

  try {
    throw 5;
  } catch(e) {
    p = j;
    function j() {
      return e + f;
    }
    let f = 6;
  }
  assertEquals(11, p());
}
f2();

// Test that resolution of let bound variables works with scopes that call eval.
function outer() {
  function middle() {
    function inner() {
      return x;
    }
    eval("1 + 1");
    return x + inner();
  }

  let x = 1;
  return middle();
}

assertEquals(2, outer());
