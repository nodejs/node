// Copyright 2009 the V8 project authors. All rights reserved.
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

// Testing that optimization of top-level object initialization doesn't
// break V8.

var x = new Object();
x.a = 7;
x.b = function() { return 42; };
x.c = 88;
x.d = "A Man Called Horse";

assertEquals(7, x.a);
assertEquals(42, x.b());
assertEquals(88, x.c);
assertEquals("A Man Called Horse", x.d);

var y = new Object();
y.a = 7;
y.b = function() { return 42; };
y.c = 12 * y.a;
y.d = "A Man Called Horse";

assertEquals(84, y.c);

var z = new Object();
z.a = 3;
function forty_two() { return 42; };
z.a += 4;
z.b = forty_two;
z.c = 12;
z.c *= z.a;
z.d = "A Man Called Horse";

assertEquals(84, z.c);

var x1 = new Object();
var x2 = new Object();
x1.a = 7;
x1.b = function() { return 42; };
x2.a = 7;
x2.b = function() { return 42; };
x1.c = 88;
x1.d = "A Man Called Horse";
x2.c = 88;
x2.d = "A Man Called Horse";

assertEquals(7, x1.a);
assertEquals(42, x1.b());
assertEquals(88, x1.c);
assertEquals("A Man Called Horse", x1.d);

assertEquals(7, x2.a);
assertEquals(42, x2.b());
assertEquals(88, x2.c);
assertEquals("A Man Called Horse", x2.d);

function Calculator(x, y) {
  this.x = x;
  this.y = y;
}

Calculator.prototype.sum = function() { return this.x + this.y; };
Calculator.prototype.difference = function() { return this.x - this.y; };
Calculator.prototype.product = function() { return this.x * this.y; };
Calculator.prototype.quotient = function() { return this.x / this.y; };

var calc = new Calculator(20, 10);
assertEquals(30, calc.sum());
assertEquals(10, calc.difference());
assertEquals(200, calc.product());
assertEquals(2, calc.quotient());

var x = new Object();
x.__defineGetter__('a', function() { return 7 });
x.b = function() { return 42; };
x.c = 88;
x.d = "A Man Called Horse";

assertEquals(7, x.a);
assertEquals(42, x.b());
assertEquals(88, x.c);
assertEquals("A Man Called Horse", x.d);
