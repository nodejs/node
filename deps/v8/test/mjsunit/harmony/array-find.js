// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --harmony-arrays

assertEquals(1, Array.prototype.find.length);

var a = [21, 22, 23, 24];
assertEquals(undefined, a.find(function() { return false; }));
assertEquals(21, a.find(function() { return true; }));
assertEquals(undefined, a.find(function(val) { return 121 === val; }));
assertEquals(24, a.find(function(val) { return 24 === val; }));
assertEquals(23, a.find(function(val) { return 23 === val; }), null);
assertEquals(22, a.find(function(val) { return 22 === val; }), undefined);


//
// Test predicate is not called when array is empty
//
(function() {
  var a = [];
  var l = -1;
  var o = -1;
  var v = -1;
  var k = -1;

  a.find(function(val, key, obj) {
    o = obj;
    l = obj.length;
    v = val;
    k = key;

    return false;
  });

  assertEquals(-1, l);
  assertEquals(-1, o);
  assertEquals(-1, v);
  assertEquals(-1, k);
})();


//
// Test predicate is called with correct argumetns
//
(function() {
  var a = ["b"];
  var l = -1;
  var o = -1;
  var v = -1;
  var k = -1;

  var found = a.find(function(val, key, obj) {
    o = obj;
    l = obj.length;
    v = val;
    k = key;

    return false;
  });

  assertArrayEquals(a, o);
  assertEquals(a.length, l);
  assertEquals("b", v);
  assertEquals(0, k);
  assertEquals(undefined, found);
})();


//
// Test predicate is called array.length times
//
(function() {
  var a = [1, 2, 3, 4, 5];
  var l = 0;
  var found = a.find(function() {
    l++;
    return false;
  });

  assertEquals(a.length, l);
  assertEquals(undefined, found);
})();


//
// Test Array.prototype.find works with String
//
(function() {
  var a = "abcd";
  var l = -1;
  var o = -1;
  var v = -1;
  var k = -1;
  var found = Array.prototype.find.call(a, function(val, key, obj) {
    o = obj.toString();
    l = obj.length;
    v = val;
    k = key;

    return false;
  });

  assertEquals(a, o);
  assertEquals(a.length, l);
  assertEquals("d", v);
  assertEquals(3, k);
  assertEquals(undefined, found);

  found = Array.prototype.find.apply(a, [function(val, key, obj) {
    o = obj.toString();
    l = obj.length;
    v = val;
    k = key;

    return true;
  }]);

  assertEquals(a, o);
  assertEquals(a.length, l);
  assertEquals("a", v);
  assertEquals(0, k);
  assertEquals("a", found);
})();


//
// Test Array.prototype.find works with exotic object
//
(function() {
  var l = -1;
  var o = -1;
  var v = -1;
  var k = -1;
  var a = {
    prop1: "val1",
    prop2: "val2",
    isValid: function() {
      return this.prop1 === "val1" && this.prop2 === "val2";
    }
  };

  Array.prototype.push.apply(a, [30, 31, 32]);
  var found = Array.prototype.find.call(a, function(val, key, obj) {
    o = obj;
    l = obj.length;
    v = val;
    k = key;

    return !obj.isValid();
  });

  assertArrayEquals(a, o);
  assertEquals(3, l);
  assertEquals(32, v);
  assertEquals(2, k);
  assertEquals(undefined, found);
})();


//
// Test array modifications
//
(function() {
  var a = [1, 2, 3];
  var found = a.find(function(val) { a.push(val); return false; });
  assertArrayEquals([1, 2, 3, 1, 2, 3], a);
  assertEquals(6, a.length);
  assertEquals(undefined, found);

  a = [1, 2, 3];
  found = a.find(function(val, key) { a[key] = ++val; return false; });
  assertArrayEquals([2, 3, 4], a);
  assertEquals(3, a.length);
  assertEquals(undefined, found);
})();


//
// Test predicate is only called for existing elements
//
(function() {
  var a = new Array(30);
  a[11] = 21;
  a[7] = 10;
  a[29] = 31;

  var count = 0;
  a.find(function() { count++; return false; });
  assertEquals(3, count);
})();


//
// Test thisArg
//
(function() {
  // Test String as a thisArg
  var found = [1, 2, 3].find(function(val, key) {
    return this.charAt(Number(key)) === String(val);
  }, "321");
  assertEquals(2, found);

  // Test object as a thisArg
  var thisArg = {
    elementAt: function(key) {
      return this[key];
    }
  };
  Array.prototype.push.apply(thisArg, ["c", "b", "a"]);

  found = ["a", "b", "c"].find(function(val, key) {
    return this.elementAt(key) === val;
  }, thisArg);
  assertEquals("b", found);
})();

// Test exceptions
assertThrows('Array.prototype.find.call(null, function() { })',
  TypeError);
assertThrows('Array.prototype.find.call(undefined, function() { })',
  TypeError);
assertThrows('Array.prototype.find.apply(null, function() { }, [])',
  TypeError);
assertThrows('Array.prototype.find.apply(undefined, function() { }, [])',
  TypeError);

assertThrows('[].find(null)', TypeError);
assertThrows('[].find(undefined)', TypeError);
assertThrows('[].find(0)', TypeError);
assertThrows('[].find(true)', TypeError);
assertThrows('[].find(false)', TypeError);
assertThrows('[].find("")', TypeError);
assertThrows('[].find({})', TypeError);
assertThrows('[].find([])', TypeError);
assertThrows('[].find(/\d+/)', TypeError);

assertThrows('Array.prototype.find.call({}, null)', TypeError);
assertThrows('Array.prototype.find.call({}, undefined)', TypeError);
assertThrows('Array.prototype.find.call({}, 0)', TypeError);
assertThrows('Array.prototype.find.call({}, true)', TypeError);
assertThrows('Array.prototype.find.call({}, false)', TypeError);
assertThrows('Array.prototype.find.call({}, "")', TypeError);
assertThrows('Array.prototype.find.call({}, {})', TypeError);
assertThrows('Array.prototype.find.call({}, [])', TypeError);
assertThrows('Array.prototype.find.call({}, /\d+/)', TypeError);

assertThrows('Array.prototype.find.apply({}, null, [])', TypeError);
assertThrows('Array.prototype.find.apply({}, undefined, [])', TypeError);
assertThrows('Array.prototype.find.apply({}, 0, [])', TypeError);
assertThrows('Array.prototype.find.apply({}, true, [])', TypeError);
assertThrows('Array.prototype.find.apply({}, false, [])', TypeError);
assertThrows('Array.prototype.find.apply({}, "", [])', TypeError);
assertThrows('Array.prototype.find.apply({}, {}, [])', TypeError);
assertThrows('Array.prototype.find.apply({}, [], [])', TypeError);
assertThrows('Array.prototype.find.apply({}, /\d+/, [])', TypeError);
