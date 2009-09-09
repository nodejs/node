// Copyright 2008 the V8 project authors. All rights reserved.
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

// Test that we can set function prototypes to non-object values.  The
// prototype used for instances in that case should be the initial
// object prototype.  ECMA-262 13.2.2.
function TestNonObjectPrototype(value) {
  function F() {};
  F.prototype = value;
  var f = new F();
  assertEquals(value, F.prototype);
  assertEquals(Object.prototype, f.__proto__);
}

var values = [123, "asdf", true];

values.forEach(TestNonObjectPrototype);


// Test moving between non-object and object values.
function F() {};
var f = new F();
assertEquals(f.__proto__, F.prototype);
F.prototype = 42;
f = new F();
assertEquals(Object.prototype, f.__proto__);
assertEquals(42, F.prototype);
F.prototype = { a: 42 };
f = new F();
assertEquals(42, F.prototype.a);
assertEquals(f.__proto__, F.prototype);


// Test that the fast case optimizations can handle non-functions,
// functions with no prototypes (yet), non-object prototypes,
// functions without initial maps, and the fully initialized
// functions.
function GetPrototypeOf(f) {
  return f.prototype;
};

// Seed the GetPrototypeOf function to enable the fast case
// optimizations.
var p = GetPrototypeOf(GetPrototypeOf);

// Check that getting the prototype of a tagged integer works.
assertTrue(typeof GetPrototypeOf(1) == 'undefined');

function NoPrototypeYet() { }
var p = GetPrototypeOf(NoPrototypeYet);
assertEquals(NoPrototypeYet.prototype, p);

function NonObjectPrototype() { }
NonObjectPrototype.prototype = 42;
assertEquals(42, GetPrototypeOf(NonObjectPrototype));

function NoInitialMap() { }
var p = NoInitialMap.prototype;
assertEquals(p, GetPrototypeOf(NoInitialMap));

// Check the standard fast case.
assertEquals(F.prototype, GetPrototypeOf(F));

// Check that getting the prototype of a non-function works. This must
// be the last thing we do because this will clobber the optimizations
// in GetPrototypeOf and go to a monomorphic IC load instead.
assertEquals(87, GetPrototypeOf({prototype:87}));

// Check the prototype is not enumerable, for compatibility with
// safari.  This is deliberately incompatible with ECMA262, 15.3.5.2.
var foo = new Function("return x");
var result  = ""
for (var n in foo) result += n;
assertEquals(result, "");
