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

function Test() {
  this.result = 0;
  this.x = 0;
  this.y = 0;
  this.z = 0;
}
var a = 1;
var b = 2;
var c = 4;
var d = 8;

// Test operations expected to stay on the fast path.  Enumerate all binary
// trees with <= 4 leaves.
Test.prototype.test0 = function () {
  this.result = a | b;
};

Test.prototype.test1 = function() {
  this.result = (a | b) | c;
};

Test.prototype.test2 = function() {
  this.result = a | (b | c);
};

Test.prototype.test3 = function() {
  this.result = ((a | b) | c) | d;
};

Test.prototype.test4 = function() {
  this.result = (a | (b | c)) | d;
};

Test.prototype.test5 = function() {
  this.result = (a | b) | (c | d);
};

Test.prototype.test6 = function() {
  this.result = a | ((b | c) | d);
};

Test.prototype.test7 = function() {
  this.result = a | (b | (c | d));
};

// These tests should fail if we bailed out to the beginning of the full
// code.
Test.prototype.test8 = function () {
  // If this.x = 1 and a = 1.1:
  this.y = this.x | b;  // Should be (1 | 2) == 3.
  this.x = c;           // Should be 4.
  this.z = this.x | a;  // Should be (4 | 1.1) == 5.
};

Test.prototype.test9 = function() {
  // If this.x = 2 and a = 1.1:
  this.z =              // (14 | 1.1) == 15
      (this.x =         // (6 | 8) == 14
           (this.y =    // (2 | 4) == 6
                this.x  // 2
                | c)    // 4
            | d)        // 8
       | a;             // 1.1
}

Test.prototype.test10 = function() {
  this.z = (a >> b) | (c >> c);
}

Test.prototype.test11 = function(x) {
  this.z = x >> x;
}

var t = new Test();

t.test0();
assertEquals(3, t.result);

t.test1();
assertEquals(7, t.result);
t.test2();
assertEquals(7, t.result);

t.test3();
assertEquals(15, t.result);
t.test4();
assertEquals(15, t.result);
t.test5();
assertEquals(15, t.result);
t.test6();
assertEquals(15, t.result);
t.test7();
assertEquals(15, t.result);

a = 1.1;
t.x = 1;
t.test8();
assertEquals(4, t.x);
assertEquals(3, t.y);
assertEquals(5, t.z);

t.x = 2;
t.test9();
assertEquals(14, t.x);
assertEquals(6, t.y);
assertEquals(15, t.z);

a = "2";
t.test11(a);
assertEquals(0, t.z);

a = 4;
b = "1";
c = 2;
t.test10();
assertEquals(2, t.z);
