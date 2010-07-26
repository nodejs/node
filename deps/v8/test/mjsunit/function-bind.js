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

// Tests the Function.prototype.bind (ES 15.3.4.5) method.

// Simple tests.
function foo(x, y, z) {
  return x + y + z;
}

var f = foo.bind(foo);
assertEquals(3, f(1, 1, 1));
assertEquals(3, f.length);

f = foo.bind(foo, 2);
assertEquals(4, f(1, 1));
assertEquals(2, f.length);

f = foo.bind(foo, 2, 2);
assertEquals(5, f(1));
assertEquals(1, f.length);

f = foo.bind(foo, 2, 2, 2);
assertEquals(6, f());
assertEquals(0, f.length);

// Test that length works correctly even if more than the actual number
// of arguments are given when binding.
f = foo.bind(foo, 1, 2, 3, 4, 5, 6, 7, 8, 9);
assertEquals(6, f());
assertEquals(0, f.length);

// Use a different bound object.
var obj = {x: 42, y: 43};
// Values that would normally be in "this" when calling f_bound_this.
var x = 42;
var y = 44;

function f_bound_this(z) {
  return z + this.y - this.x;
} 

assertEquals(3, f_bound_this(1))
f = f_bound_this.bind(obj);
assertEquals(2, f(1));
assertEquals(1, f.length);

f = f_bound_this.bind(obj, 2);
assertEquals(3, f());
assertEquals(0, f.length);

// Test chained binds.

// When only giving the thisArg, any number of binds should have 
// the same effect.
f = foo.bind(foo);
assertEquals(3, f(1, 1, 1));
f = foo.bind(foo).bind(foo).bind(foo).bind(foo);
assertEquals(3, f(1, 1, 1));
assertEquals(3, f.length);

// Giving bound parameters should work at any place in the chain.
f = foo.bind(foo, 1).bind(foo).bind(foo).bind(foo);
assertEquals(3, f(1, 1));
assertEquals(2, f.length);

f = foo.bind(foo).bind(foo, 1).bind(foo).bind(foo);
assertEquals(3, f(1, 1));
assertEquals(2, f.length);

f = foo.bind(foo).bind(foo).bind(foo,1 ).bind(foo);
assertEquals(3, f(1, 1));
assertEquals(2, f.length);

f = foo.bind(foo).bind(foo).bind(foo).bind(foo, 1);
assertEquals(3, f(1, 1));
assertEquals(2, f.length);

// Several parameters can be given, and given in different bind invokations.
f = foo.bind(foo, 1, 1).bind(foo).bind(foo).bind(foo);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(foo, 1, 1).bind(foo).bind(foo);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(foo, 1, 1).bind(foo).bind(foo);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(foo).bind(foo, 1, 1).bind(foo);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(foo).bind(foo).bind(foo, 1, 1);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo, 1).bind(foo, 1).bind(foo).bind(foo);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo, 1).bind(foo).bind(foo, 1).bind(foo);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo, 1).bind(foo).bind(foo).bind(foo, 1);
assertEquals(3, f(1));
assertEquals(1, f.length);

f = foo.bind(foo).bind(foo, 1).bind(foo).bind(foo, 1);
assertEquals(3, f(1));
assertEquals(1, f.length);

// Test constructor calls.

function bar(x, y, z) {
  this.x = x;
  this.y = y;
  this.z = z;
}

f = bar.bind(bar);
var obj2 = new f(1,2,3);
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

f = bar.bind(bar, 1);
obj2 = new f(2,3);
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

f = bar.bind(bar, 1, 2);
obj2 = new f(3);
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

f = bar.bind(bar, 1, 2, 3);
obj2 = new f();
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);


// Test bind chains when used as a constructor.

f = bar.bind(bar, 1).bind(bar, 2).bind(bar, 3);
obj2 = new f();
assertEquals(1, obj2.x);
assertEquals(2, obj2.y);
assertEquals(3, obj2.z);

// Test instanceof obj2 is bar, not f.
assertTrue(obj2 instanceof bar);
assertFalse(obj2 instanceof f);

