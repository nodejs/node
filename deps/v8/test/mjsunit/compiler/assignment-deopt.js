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

// Test deopt with count operation on parameter.
var max_smi = 1073741823;
var o = {x:0};

function assign1(x) { x += 1; o.x = x; }
assign1(max_smi);
assertEquals(max_smi + 1, o.x);

assign1(1.1);
assertEquals(2.1, o.x);


// Test deopt with count operation on named property.
function assign2(p) { p.x += 1 }

o.x = "42";
assign2(o);
assertEquals("421", o.x);

var s = max_smi - 10000;
o.x = s;
for(var i = 0; i < 20000; i++) {
  assign2(o);
}
assertEquals(max_smi + 10000, o.x);


// Test deopt with count operation on keyed property.
function assign3(a, b) { a[b] += 1; }

o = ["42"];
assign3(o, 0);
assertEquals("421", o[0]);

var s = max_smi - 10000;
o[0] = s;
for(var i = 0; i < 20000; i++) {
  assign3(o, 0);
}
assertEquals(max_smi + 10000, o[0]);

assign3(o,"0");

assertEquals(max_smi + 10001, o[0]);

// Test bailout when accessing a non-existing array element.
o[0] = 0;
for(var i = 0; i < 10000; i++) {
  assign3(o, 0);
}
assign3(o,1);

// Test bailout with count operation in a value context.
function assign5(x,y) { return (x += 1) + y; }
for (var i = 0; i < 10000; ++i) assertEquals(4, assign5(2, 1));
assertEquals(4.1, assign5(2, 1.1));
assertEquals(4.1, assign5(2.1, 1));

function assign7(o,y) { return (o.x += 1) + y; }
o = {x:0};
for (var i = 0; i < 10000; ++i) {
  o.x = 42;
  assertEquals(44, assign7(o, 1));
}
o.x = 42;
assertEquals(44.1, assign7(o, 1.1));
o.x = 42.1;
assertEquals(44.1, assign7(o, 1));

function assign9(o,y) { return (o[0] += 1) + y; }
q = [0];
for (var i = 0; i < 10000; ++i) {
  q[0] = 42;
  assertEquals(44, assign9(q, 1));
}
q[0] = 42;
assertEquals(44.1, assign9(q, 1.1));
q[0] = 42.1;
assertEquals(44.1, assign9(q, 1));

// Test deopt because of a failed map check on the load.
function assign10(p) { return p.x += 1 }
var g1 = {x:0};
var g2 = {y:0, x:42};
for (var i = 0; i < 10000; ++i) {
  g1.x = 42;
  assertEquals(43, assign10(g1));
  assertEquals(43, g1.x);
}
assertEquals(43, assign10(g2));
assertEquals(43, g2.x);

// Test deopt because of a failed map check on the store.
// The binary operation changes the map as a side effect.
o = {x:0};
var g3 = { valueOf: function() { o.y = "bar"; return 42; }};
function assign11(p) { return p.x += 1; }

for (var i = 0; i < 10000; i++) {
  o.x = "a";
  assign11(o);
}
assertEquals("a11", assign11(o));
o.x = g3;
assertEquals(43, assign11(o));
assertEquals("bar", o.y);

o = [0];
var g4 = { valueOf: function() { o.y = "bar"; return 42; }};
function assign12(p) { return p[0] += 1; }

for (var i = 0; i < 1000000; i++) {
  o[0] = "a";
  assign12(o);
}
assertEquals("a11", assign12(o));
o[0] = g4;
assertEquals(43, assign12(o));
assertEquals("bar", o.y);
