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

// Check that constants and computed properties are overwriting each other
// correctly, i.e., the last initializer for any name is stored in the object.


// Tests for the full code generator (if active).

var foo1 = {
  bar: 6,
  bar: 7
};

var foo2 = {
  bar: function(a){},
  bar: 7
};

var foo3 = {
  bar: function(a){},
  bar: function(b){},
  bar: 7
};

var foo4 = {
  bar: function(b){},
  bar: 7,
  bar: function(){return 7},
};

var foo5 = {
  13: function(a){},
  13: 7
}

var foo6 = {
  14.31: function(a){},
  14.31: 7
}

var foo7 = {
  15: 6,
  15: 7
}

assertEquals(7, foo1.bar);
assertEquals(7, foo2.bar);
assertEquals(7, foo3.bar);
assertEquals(7, foo4.bar());
assertEquals(7, foo5[13]);
assertEquals(7, foo6[14.31]);
assertEquals(7, foo7[15]);

// Test for the classic code generator.

function fun(x) {
  var inner = { j: function(x) { return x; }, j: 7 }; 
  return inner.j;
}

assertEquals(7, fun(7) );

// Check that the initializers of computed properties are executed, even if
// no store instructions are generated for the literals.

var glob1 = 0;

var bar1 = { x: glob1++, x: glob1++, x: glob1++, x: 7};

assertEquals(3, glob1);


var glob2 = 0;

function fun2() {
  var r = { y: glob2++, y: glob2++, y: glob2++, y: 7};
  return r.y;
}

var y = fun2();
assertEquals(7, y);
assertEquals(3, glob2);

var glob3 = 0;

function fun3() {
  var r = { 113: glob3++, 113: glob3++, 113: glob3++, 113: 7};
  return r[113];
}

var y = fun3();
assertEquals(7, y);
assertEquals(3, glob3);