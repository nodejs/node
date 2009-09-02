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

function props(x) {
  var array = [];
  for (var p in x) array.push(p);
  return array.sort();
}

function f1() {
  this.x = 1;
}

function f2(x) {
  this.x = x;
}

function f3(x) {
  this.x = x;
  this.y = 1;
  this.z = f1;
}

function f4(x) {
  this.x = x;
  this.y = 1;
  if (x == 1) return;
  this.z = f1;
}

o1_1 = new f1();
assertEquals(1, o1_1.x, "1");
o1_2 = new f1();
assertEquals(1, o1_1.x, "2");
assertArrayEquals(["x"], props(o1_1), "3");
assertArrayEquals(["x"], props(o1_2), "4");

o2_1 = new f2(0);
o2_2 = new f2(0);
assertArrayEquals(["x"], props(o2_1));
assertArrayEquals(["x"], props(o2_2));

o3_1 = new f3(0);
o3_2 = new f3(0);
assertArrayEquals(["x", "y", "z"], props(o3_1));
assertArrayEquals(["x", "y", "z"], props(o3_2));

o4_0_1 = new f4(0);
o4_0_2 = new f4(0);
assertArrayEquals(["x", "y", "z"], props(o4_0_1));
assertArrayEquals(["x", "y", "z"], props(o4_0_2));

o4_1_1 = new f4(1);
o4_1_2 = new f4(1);
assertArrayEquals(["x", "y"], props(o4_1_1));
assertArrayEquals(["x", "y"], props(o4_1_2));

function f5(x, y) {
  this.x = x;
  this.y = y;
}

function f6(x, y) {
  this.y = y;
  this.x = x;
}

function f7(x, y, z) {
  this.x = x;
  this.y = y;
}

function testArgs(fun) {
  obj = new fun();
  assertArrayEquals(["x", "y"], props(obj));
  assertEquals(void 0, obj.x);
  assertEquals(void 0, obj.y);

  obj = new fun("x");
  assertArrayEquals(["x", "y"], props(obj));
  assertEquals("x", obj.x);
  assertEquals(void 0, obj.y);

  obj = new fun("x", "y");
  assertArrayEquals(["x", "y"], props(obj));
  assertEquals("x", obj.x);
  assertEquals("y", obj.y);

  obj = new fun("x", "y", "z");
  assertArrayEquals(["x", "y"], props(obj));
  assertEquals("x", obj.x);
  assertEquals("y", obj.y);
}

for (var i = 0; i < 10; i++) {
  testArgs(f5);
  testArgs(f6);
  testArgs(f7);
}

function g(){
  this.x=1
}

o = new g();
assertEquals(1, o.x);
o = new g();
assertEquals(1, o.x);
g.prototype = {y:2}
o = new g();
assertEquals(1, o.x);
assertEquals(2, o.y);
o = new g();
assertEquals(1, o.x);
assertEquals(2, o.y);

