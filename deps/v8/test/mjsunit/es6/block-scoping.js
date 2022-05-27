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

// Flags: --allow-natives-syntax --turbofan
// Test functionality of block scopes.

"use strict";

// Hoisting of var declarations.
function f1() {
  {
    var x = 1;
    var y;
  }
  assertEquals(1, x)
  assertEquals(undefined, y)
}
%PrepareFunctionForOptimization(f1);
for (var j = 0; j < 5; ++j) f1();
%OptimizeFunctionOnNextCall(f1);
f1();
assertOptimized(f1);

// Dynamic lookup in and through block contexts.
function f2(one) {
  var x = one + 1;
  let y = one + 2;
  const u = one + 4;
  class a { static foo() { return one + 6; } }
  {
    let z = one + 3;
    const v = one + 5;
    class b { static foo() { return one + 7; } }
    assertEquals(1, eval('one'));
    assertEquals(2, eval('x'));
    assertEquals(3, eval('y'));
    assertEquals(4, eval('z'));
    assertEquals(5, eval('u'));
    assertEquals(6, eval('v'));
    assertEquals(7, eval('a.foo()'));
    assertEquals(8, eval('b.foo()'));
  }
}

f2(1);

// Lookup in and through block contexts.
function f3(one) {
  var x = one + 1;
  let y = one + 2;
  const u = one + 4;
  class a { static foo() { return one + 6; } }
  {
    let z = one + 3;
    const v = one + 5;
    class b { static foo() { return one + 7; } }
    assertEquals(1, one);
    assertEquals(2, x);
    assertEquals(3, y);
    assertEquals(4, z);
    assertEquals(5, u);
    assertEquals(6, v);
    assertEquals(7, a.foo());
    assertEquals(8, b.foo());
  }
}
%PrepareFunctionForOptimization(f3);
for (var j = 0; j < 5; ++j) f3(1);
%OptimizeFunctionOnNextCall(f3);
f3(1);



// Dynamic lookup from closure.
function f4(one) {
  var x = one + 1;
  let y = one + 2;
  const u = one + 4;
  class a { static foo() { return one + 6; } }
  {
    let z = one + 3;
    const v = one + 5;
    class b { static foo() { return one + 7; } }
    function f() {
      assertEquals(1, eval('one'));
      assertEquals(2, eval('x'));
      assertEquals(3, eval('y'));
      assertEquals(4, eval('z'));
      assertEquals(5, eval('u'));
      assertEquals(6, eval('v'));
      assertEquals(7, eval('a.foo()'));
      assertEquals(8, eval('b.foo()'));
    }
    f();
  }
}
f4(1);


// Lookup from closure.
function f5(one) {
  var x = one + 1;
  let y = one + 2;
  const u = one + 4;
  class a { static foo() { return one + 6; } }
  {
    let z = one + 3;
    const v = one + 5;
    class b { static foo() { return one + 7; } }
    function f() {
      assertEquals(1, one);
      assertEquals(2, x);
      assertEquals(3, y);
      assertEquals(4, z);
      assertEquals(5, u);
      assertEquals(6, v);
      assertEquals(7, a.foo());
      assertEquals(8, b.foo());
    }
    f();
  }
}
f5(1);


// Return from block.
function f6() {
  let x = 1;
  const u = 3;
  {
    let y = 2;
    const v = 4;
    return x + y;
  }
}
assertEquals(3, f6(6));


// Variable shadowing and lookup.
function f7(a) {
  let b = 1;
  var c = 1;
  var d = 1;
  const e = 1;
  class f { static foo() { return 1; } }
  { // let variables shadowing argument, let, const, class and var variables
    let a = 2;
    let b = 2;
    let c = 2;
    let e = 2;
    let f = 2;
    assertEquals(2,a);
    assertEquals(2,b);
    assertEquals(2,c);
    assertEquals(2,e);
    assertEquals(2,f);
  }
  { // const variables shadowing argument, let, const and var variables
    const a = 2;
    const b = 2;
    const c = 2;
    const e = 2;
    const f = 2;
    assertEquals(2,a);
    assertEquals(2,b);
    assertEquals(2,c);
    assertEquals(2,e);
    assertEquals(2,f);
  }
  { // class variables shadowing argument, let, const and var variables
    class a { static foo() { return 2; } }
    class b { static foo() { return 2; } }
    class c { static foo() { return 2; } }
    class d { static foo() { return 2; } }
    class e { static foo() { return 2; } }
    class f { static foo() { return 2; } }
    assertEquals(2,a.foo());
    assertEquals(2,b.foo());
    assertEquals(2,c.foo());
    assertEquals(2,e.foo());
    assertEquals(2,f.foo());
  }
  try {
    throw 'stuff1';
  } catch (a) {
    assertEquals('stuff1',a);
    // catch variable shadowing argument
    a = 2;
    assertEquals(2,a);
    {
      // let variable shadowing catch variable
      let a = 3;
      assertEquals(3,a);
      try {
        throw 'stuff2';
      } catch (a) {
        assertEquals('stuff2',a);
        // catch variable shadowing let variable
        a = 4;
        assertEquals(4,a);
      }
      assertEquals(3,a);
    }
    assertEquals(2,a);
  }
  try {
    throw 'stuff3';
  } catch (c) {
    // catch variable shadowing var variable
    assertEquals('stuff3',c);
    {
      // const variable shadowing catch variable
      const c = 3;
      assertEquals(3,c);
    }
    assertEquals('stuff3',c);
    try {
      throw 'stuff4';
    } catch(c) {
      assertEquals('stuff4',c);
      // catch variable shadowing catch variable
      c = 3;
      assertEquals(3,c);
    }
    (function(c) {
      // argument shadowing catch variable
      c = 3;
      assertEquals(3,c);
    })();
    assertEquals('stuff3', c);
    (function() {
      // var variable shadowing catch variable
      var c = 3;
    })();
    assertEquals('stuff3', c);
    c = 2;
  }
  assertEquals(1,c);
  (function(a,b,c,e,f) {
    // arguments shadowing argument, let, const, class and var variable
    a = 2;
    b = 2;
    c = 2;
    e = 2;
    f = 2;
    assertEquals(2,a);
    assertEquals(2,b);
    assertEquals(2,c);
    assertEquals(2,e);
    assertEquals(2,f);
    // var variable shadowing var variable
    var d = 2;
  })(1,1);
  assertEquals(1,a);
  assertEquals(1,b);
  assertEquals(1,c);
  assertEquals(1,d);
  assertEquals(1,e);
  assertEquals(1,f.foo());
}
f7(1);


// Ensure let and const variables are block local
// and var variables function local.
function f8() {
  var let_accessors = [];
  var var_accessors = [];
  var const_accessors = [];
  var class_accessors = [];
  for (var i = 0; i < 10; i++) {
    let x = i;
    var y = i;
    const z = i;
    class a { static foo() { return x; } }
    let_accessors[i] = function() { return x; }
    var_accessors[i] = function() { return y; }
    const_accessors[i] = function() { return z; }
    class_accessors[i] = function() { return a; }
  }
  for (var j = 0; j < 10; j++) {
    y = j + 10;
    assertEquals(j, let_accessors[j]());
    assertEquals(y, var_accessors[j]());
    assertEquals(j, const_accessors[j]());
    assertEquals(j, class_accessors[j]().foo());
  }
}
f8();
