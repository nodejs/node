// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestArrayNonEmptySpecies() {
  class MyArray extends Array {
    constructor() { return [1, 2, 3]; }
  }
  var a = [5, 4];
  a.__proto__ = MyArray.prototype;
  var o = a.filter(() => true);
  assertEquals([5, 4, 3], o);
  assertEquals(3, o.length);
})();

(function TestArrayLeakingSpeciesInsertInCallback() {
  var my_array = [];
  class MyArray extends Array {
    constructor() { return my_array; }
  }
  var a = [5, 4];
  a.__proto__ = MyArray.prototype;
  var o = a.filter(() => (my_array[2] = 3, true));
  assertEquals([5, 4, 3], o);
  assertEquals(3, o.length);
})();

(function TestArrayLeakingSpeciesRemoveInCallback() {
  var my_array = [];
  class MyArray extends Array {
    constructor() { return my_array; }
  }
  var a = [5, 4, 3, 2, 1];
  a.__proto__ = MyArray.prototype;
  var o = a.filter(() => (my_array.length = 0, true));
  assertEquals([,,,,1], o);
  assertEquals(5, o.length);
})();
