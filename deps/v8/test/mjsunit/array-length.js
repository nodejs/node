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

var a = [0,1,2,3];
assertEquals(0, a.length = 0);

assertEquals('undefined', typeof a[0]);
assertEquals('undefined', typeof a[1]);
assertEquals('undefined', typeof a[2]);
assertEquals('undefined', typeof a[3]);


var a = [0,1,2,3];
assertEquals(2, a.length = 2);

assertEquals(0, a[0]);
assertEquals(1, a[1]);
assertEquals('undefined', typeof a[2]);
assertEquals('undefined', typeof a[3]);


for(var i = 0; i < 10; i++) {
      var array = new Array(i).fill(42);
      array.push(42);
      array.length = i;
      array.length = i+1;
      assertEquals('undefined' , typeof array[i]);
}


var a = new Array();
a[0] = 0;
a[1000] = 1000;
a[1000000] = 1000000;
a[2000000] = 2000000;

assertEquals(2000001, a.length);
assertEquals(0, a.length = 0);
assertEquals(0, a.length);
assertEquals('undefined', typeof a[0]);
assertEquals('undefined', typeof a[1000]);
assertEquals('undefined', typeof a[1000000]);
assertEquals('undefined', typeof a[2000000]);


var a = new Array();
a[0] = 0;
a[1000] = 1000;
a[1000000] = 1000000;
a[2000000] = 2000000;

assertEquals(2000001, a.length);
assertEquals(2000, a.length = 2000);
assertEquals(2000, a.length);
assertEquals(0, a[0]);
assertEquals(1000, a[1000]);
assertEquals('undefined', typeof a[1000000]);
assertEquals('undefined', typeof a[2000000]);


var a = new Array();
a[Math.pow(2,31)-1] = 0;
a[Math.pow(2,30)-1] = 0;
assertEquals(Math.pow(2,31), a.length);


var a = new Array();
a[0] = 0;
a[1000] = 1000;
a[Math.pow(2,30)-1] = Math.pow(2,30)-1;
a[Math.pow(2,31)-1] = Math.pow(2,31)-1;
a[Math.pow(2,32)-2] = Math.pow(2,32)-2;

assertEquals(Math.pow(2,30)-1, a[Math.pow(2,30)-1]);
assertEquals(Math.pow(2,31)-1, a[Math.pow(2,31)-1]);
assertEquals(Math.pow(2,32)-2, a[Math.pow(2,32)-2]);

assertEquals(Math.pow(2,32)-1, a.length);
assertEquals(Math.pow(2,30) + 1, a.length = Math.pow(2,30)+1);  // not a smi!
assertEquals(Math.pow(2,30)+1, a.length);

assertEquals(0, a[0]);
assertEquals(1000, a[1000]);
assertEquals(Math.pow(2,30)-1, a[Math.pow(2,30)-1]);
assertEquals('undefined', typeof a[Math.pow(2,31)-1]);
assertEquals('undefined', typeof a[Math.pow(2,32)-2], "top");


var a = new Array();
assertEquals(Object(12), a.length = new Number(12));
assertEquals(12, a.length);

Number.prototype.valueOf = function() { return 10; }
var n = new Number(100);
assertEquals(n, a.length = n);
assertEquals(10, a.length);
n.valueOf = function() { return 20; }
assertEquals(n, a.length = n);
assertEquals(20, a.length);

var o = { length: -23 };
Array.prototype.pop.apply(o);
assertEquals(0, o.length);

// Check case of compiled stubs.
var a = [];
for (var i = 0; i < 7; i++) {
  assertEquals(3, a.length = 3);

  var t = 239;
  t = a.length = 7;
  assertEquals(7, t);
}

(function () {
  "use strict";
  var frozen_object = Object.freeze({__proto__:[]});
  assertThrows(function () { frozen_object.length = 10 });
})();

(function sloppyReentrantDescriptorChange() {
  var b = [];
  b.length = {
    valueOf() {
      Object.defineProperty(b, "length", {writable: false});
      return 1;
    }
  };
  assertEquals(0, b.length);
})();

(function strictReentrantDescriptorChange() {
  var b = [];
  assertThrows(() => {
    "use strict";
    b.length = {
      valueOf() {
        Object.defineProperty(b, "length", {writable: false});
        return 1;
      }
    };
  }, TypeError);

  b.length = { valueOf() { return 0; } };
  assertEquals(0, b.length);
})();
