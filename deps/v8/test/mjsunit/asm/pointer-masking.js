// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var stdlib = this;
var foreign = {};
var heap = new ArrayBuffer(64 * 1024);


var pm1 = (function(stdlib, foreign, heap) {
  "use asm";
  var HEAP8 = new stdlib.Int8Array(heap);
  const MASK1 = 1023;
  function load1(i) {
    i = i|0;
    var j = 0;
    j = HEAP8[(i & MASK1)]|0;
    return j|0;
  }
  function store1(i, j) {
    i = i|0;
    j = j|0;
    HEAP8[(i & MASK1)] = j;
  }
  return {load1: load1, store1: store1};
})(stdlib, foreign, heap);

assertEquals(0, pm1.load1(0));
assertEquals(0, pm1.load1(1025));
pm1.store1(0, 1);
pm1.store1(1025, 127);
assertEquals(1, pm1.load1(0));
assertEquals(1, pm1.load1(1024));
assertEquals(127, pm1.load1(1));
assertEquals(127, pm1.load1(1025));
