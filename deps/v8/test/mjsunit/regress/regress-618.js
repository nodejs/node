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

// Simple class using inline constructor.
function C1() {
  this.x = 23;
};
var c1 = new C1();
assertEquals(23, c1.x);
assertEquals("undefined", typeof c1.y);

// Add setter somewhere on the prototype chain after having constructed the
// first instance.
C1.prototype = { set x(value) { this.y = 23; } };
var c1 = new C1();
assertEquals("undefined", typeof c1.x);
assertEquals(23, c1.y);

// Simple class using inline constructor.
function C2() {
  this.x = 23;
};
var c2 = new C2();
assertEquals(23, c2.x);
assertEquals("undefined", typeof c2.y);

// Add setter somewhere on the prototype chain after having constructed the
// first instance.
C2.prototype.__proto__ = { set x(value) { this.y = 23; } };
var c2 = new C2();
assertEquals("undefined", typeof c2.x);
assertEquals(23, c2.y);

// Simple class using inline constructor.
function C3() {
  this.x = 23;
};
var c3 = new C3();
assertEquals(23, c3.x);
assertEquals("undefined", typeof c3.y);

// Add setter somewhere on the prototype chain after having constructed the
// first instance.
C3.prototype.__defineSetter__('x', function(value) { this.y = 23; });
var c3 = new C3();
assertEquals("undefined", typeof c3.x);
assertEquals(23, c3.y);

// Simple class using inline constructor.
function C4() {
  this.x = 23;
};
var c4 = new C4();
assertEquals(23, c4.x);
assertEquals("undefined", typeof c4.y);

// Add setter somewhere on the prototype chain after having constructed the
// first instance.
C4.prototype.__proto__.__defineSetter__('x', function(value) { this.y = 23; });
var c4 = new C4();
assertEquals("undefined", typeof c4.x);
assertEquals(23, c4.y);
